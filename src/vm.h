// =================================================================================================
// bl
//
// File:   vm.h
// Author: Martin Dorazil
// Date:   9/17/19
//
// Copyright 2019 Martin Dorazil
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// =================================================================================================

#ifndef BL_VM_H
#define BL_VM_H

#include "common.h"
#include "tinycthread.h"

// Values:
// * compile time constant
//     - allocated in data buffer
//     - available anytime during compilation process
//     - only constants can be evaluated in compile time
// * runtime value
//     - living on the stack just as temporary value
//     - is supposed to be consumed soon by following instructions
//     - small values can live in preallocated registers (to safe stack space)

// Stack data manipulation helper macros.
#define VM_STACK_PTR_DEREF(ptr) ((vm_stack_ptr_t) * ((uintptr_t *)(ptr)))

struct mir_type;
struct mir_instr;
struct mir_instr_block;
struct mir_instr_call;
struct mir_instr_decl_var;
struct mir_fn;
struct mir_var;
struct builder;
struct assembly;

typedef u8        vm_value_t[16];
typedef ptrdiff_t vm_relative_stack_ptr_t;
typedef u8       *vm_stack_ptr_t;

enum vm_interp_state {
	VM_INTERP_PASSED,
	VM_INTERP_POSTPONE,
	VM_INTERP_ABORT,
};

struct vm_frame {
	struct vm_frame       *prev;
	struct mir_instr_call *caller; // Optional
};

struct vm_stack {
	vm_stack_ptr_t          top_ptr;         // pointer to top of the stack
	usize                   allocated_bytes; // total allocated size of the stack in bytes
	struct vm_frame        *ra;              // current frame beginning (return address)
	struct mir_instr       *pc;              // currently executed instruction (program counter)
	struct mir_instr_block *prev_block;      // used by phi instruction
};

struct vm_bufpage {
	struct vm_bufpage *prev;
	usize              len, cap;
	vm_stack_ptr_t     top;
};

struct vm_snapshot {
	struct mir_instr_call *hash;
	struct vm_stack       *stack;
};

struct virtual_machine {
	struct vm_stack   *stack;
	struct vm_stack   *main_stack; // Owner pointer of the main execution stack.
	struct vm_bufpage *data;       // Compile time values + global variables.
	struct assembly   *assembly;
	array(char) dcsigtmp;
	bool aborted;

	// Cache of unused compile-time call executed stacks available for reuse.
	array(struct vm_stack *) available_comptime_call_stacks;

	// Contains cached small stacks used for compile time execution of function calls. The function
	// call in compile time can hit incomplete function in such a case the execution must wait until
	// the function is analyzed. This cache is used to restore previous execution.
	//
	// When the call is successfully completed the cached entry must be removed from the table and
	// returned back to 'available_comptime_call_stacks' array.
	hash_table(struct vm_snapshot) comptime_call_stacks;

	mtx_t lock;
};

enum mir_value_address_mode {
	MIR_VAM_UNKNOWN,

	// Value points to memory allocation on the stack or heap.
	MIR_VAM_LVALUE,
	// Value points to memory allocation on the stack or heap but value itself is immutable and
	// cannot be modified.
	MIR_VAM_LVALUE_CONST,
	// Does not point to allocated memory (i.e.: const literals).
	MIR_VAM_RVALUE,
};

// This should not be there but whatever.
struct mir_const_expr_value {
	// @Performance: _tmp and data can be packed in union
	vm_value_t                  _tmp;
	vm_stack_ptr_t              data;
	struct mir_type            *type;
	enum mir_value_address_mode addr_mode;
	bool                        is_comptime;
};

typedef sarr_t(struct mir_const_expr_value, 32) mir_const_values_t;

void vm_init(struct virtual_machine *vm, usize stack_size);
void vm_terminate(struct virtual_machine *vm);
bool vm_eval_instr(struct virtual_machine *vm, struct assembly *assembly, struct mir_instr *instr);

// Execute top level call instruction, called function must be fully analyzed. Return value is set
// into call value data pointer.
//
// Internally a new small execution stack can be allocated in case none is available; this stack can
// be reused in case the execution chain hits incomplete function.
enum vm_interp_state vm_execute_comptime_call(struct virtual_machine *vm,
                                              struct assembly        *assembly,
                                              struct mir_instr_call  *call);

enum vm_interp_state vm_execute_fn(struct virtual_machine *vm,
                                   struct assembly        *assembly,
                                   struct mir_fn          *fn,
                                   mir_const_values_t     *optional_args,
                                   vm_stack_ptr_t         *optional_return);

void vm_provide_command_line_arguments(struct virtual_machine *vm, const s32 argc, char *argv[]);
void vm_override_var(struct virtual_machine *vm, struct mir_var *var, u64 value);
void vm_do_cast(vm_stack_ptr_t dest, vm_stack_ptr_t src, struct mir_type *dest_type, struct mir_type *src_type, s32 op);
void vm_alloc_global(struct virtual_machine *vm, struct assembly *assembly, struct mir_var *var);

// Allocate raw memory on the stack to hold sizeof(type) value.
vm_stack_ptr_t vm_alloc_raw(struct virtual_machine *vm, struct assembly *assembly, struct mir_type *type);
void           vm_print_backtrace(struct virtual_machine *vm);
void           vm_abort(struct virtual_machine *vm);

// Return pointer to constant or stack allocated variable.
vm_stack_ptr_t vm_read_var(struct virtual_machine *vm, const struct mir_var *var);

#define vm_read_as(T, src)        (*((T *)(src)))
#define vm_write_as(T, dest, src) (*((T *)(dest)) = (T)(src))

u64            vm_read_int(const struct mir_type *type, vm_stack_ptr_t src);
f64            vm_read_double(const struct mir_type *type, vm_stack_ptr_t src);
f32            vm_read_float(const struct mir_type *type, vm_stack_ptr_t src);
vm_stack_ptr_t vm_read_ptr(const struct mir_type *type, vm_stack_ptr_t src);
str_t          vm_read_string(struct virtual_machine *vm, const struct mir_type *type, vm_stack_ptr_t src);
void           vm_write_int(const struct mir_type *type, vm_stack_ptr_t dest, u64 i);
void           vm_write_double(const struct mir_type *type, vm_stack_ptr_t dest, f64 i);
void           vm_write_float(const struct mir_type *type, vm_stack_ptr_t dest, f32 i);
void           vm_write_ptr(const struct mir_type *type, vm_stack_ptr_t dest, vm_stack_ptr_t ptr);
void           vm_write_string(struct virtual_machine *vm, const struct mir_type *type, vm_stack_ptr_t dest, str_t str);
void           vm_write_slice(struct virtual_machine *vm, const struct mir_type *type, vm_stack_ptr_t dest, void *ptr, s64 len);
ptrdiff_t      vm_get_struct_elem_offset(struct assembly *assembly, const struct mir_type *type, u32 i);
ptrdiff_t      vm_get_array_elem_offset(const struct mir_type *type, u32 i);
vm_stack_ptr_t vm_get_struct_elem_ptr(struct assembly *assembly, const struct mir_type *type, vm_stack_ptr_t ptr, u32 i);
vm_stack_ptr_t vm_get_array_elem_ptr(const struct mir_type *type, vm_stack_ptr_t ptr, u32 i);

#endif
