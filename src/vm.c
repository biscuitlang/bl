#include "vm.h"
#include "builder.h"
#include "common.h"
#include "stb_ds.h"
#include "table.h"
#include "vmdbg.h"

#define VM_MAX_ALIGNMENT 8

// =================================================================================================
// fwd decls
// =================================================================================================
static void calculate_binop(struct mir_type *src_type,
                            vm_stack_ptr_t   dest,
                            vm_stack_ptr_t   lhs,
                            vm_stack_ptr_t   rhs,
                            enum binop_kind  op);

static void
calculate_unop(vm_stack_ptr_t dest, vm_stack_ptr_t v, enum unop_kind op, struct mir_type *type);

// zero max nesting = unlimited nesting
static void        dyncall_cb_read_arg(struct virtual_machine      *vm,
                                       struct mir_const_expr_value *dest_value,
                                       DCArgs                      *src);
static char        dyncall_cb_handler(DCCallback *cb, DCArgs *args, DCValue *result, void *userdata);
static void        _dyncall_generate_signature(struct virtual_machine *vm, struct mir_type *type);
static const char *dyncall_generate_signature(struct virtual_machine *vm, struct mir_type *type);
static DCCallback *dyncall_fetch_callback(struct virtual_machine *vm, struct mir_fn *fn);

static void
dyncall_push_arg(struct virtual_machine *vm, vm_stack_ptr_t val_ptr, struct mir_type *type);

static enum vm_interp_state execute_function(struct virtual_machine *vm,
                                             struct mir_fn          *fn,
                                             struct mir_instr_call  *optional_call,
                                             const bool              resume);
static enum vm_interp_state interp_instr(struct virtual_machine *vm, struct mir_instr *instr);

static void interp_extern_call(struct virtual_machine *vm, struct mir_instr_call *call, str_t linkage_name, struct mir_type *fn_type, DCpointer handle);

static void                 interp_instr_toany(struct virtual_machine *vm, struct mir_instr_to_any *toany);
static void                 interp_instr_unreachable(struct virtual_machine *vm, struct mir_instr_unreachable *unr);
static void                 interp_instr_debugbreak(struct virtual_machine      *vm,
                                                    struct mir_instr_debugbreak *debug_break);
static void                 interp_instr_phi(struct virtual_machine *vm, struct mir_instr_phi *phi);
static void                 interp_instr_cast(struct virtual_machine *vm, struct mir_instr_cast *cast);
static void                 interp_instr_addrof(struct virtual_machine *vm, struct mir_instr_addrof *addrof);
static void                 interp_instr_br(struct virtual_machine *vm, struct mir_instr_br *br);
static void                 interp_instr_switch(struct virtual_machine *vm, struct mir_instr_switch *sw);
static void                 interp_instr_elem_ptr(struct virtual_machine *vm, struct mir_instr_elem_ptr *elem_ptr);
static void                 interp_instr_member_ptr(struct virtual_machine      *vm,
                                                    struct mir_instr_member_ptr *member_ptr);
static void                 interp_instr_unroll(struct virtual_machine *vm, struct mir_instr_unroll *unroll);
static void                 interp_instr_arg(struct virtual_machine *vm, struct mir_instr_arg *arg);
static void                 interp_instr_cond_br(struct virtual_machine *vm, struct mir_instr_cond_br *br);
static void                 interp_instr_load(struct virtual_machine *vm, struct mir_instr_load *load);
static void                 interp_instr_store(struct virtual_machine *vm, struct mir_instr_store *store);
static void                 interp_instr_binop(struct virtual_machine *vm, struct mir_instr_binop *binop);
static void                 interp_instr_unop(struct virtual_machine *vm, struct mir_instr_unop *unop);
static enum vm_interp_state interp_instr_call(struct virtual_machine *vm,
                                              struct mir_instr_call  *call);
static void                 interp_instr_ret(struct virtual_machine *vm, struct mir_instr_ret *ret);
static void                 interp_instr_compound(struct virtual_machine    *vm,
                                                  vm_stack_ptr_t             tmp_ptr,
                                                  struct mir_instr_compound *cmp);
static void                 interp_instr_vargs(struct virtual_machine *vm, struct mir_instr_vargs *vargs);
static void                 interp_instr_decl_var(struct virtual_machine *vm, struct mir_instr_decl_var *decl);
static void                 interp_instr_decl_ref(struct virtual_machine *vm, struct mir_instr_decl_ref *ref);
static void                 interp_instr_decl_direct_ref(struct virtual_machine           *vm,
                                                         struct mir_instr_decl_direct_ref *ref);
static void                 eval_instr(struct virtual_machine *vm, struct mir_instr *instr);
static void                 eval_instr_type_info(struct virtual_machine *vm, struct mir_instr_type_info *type_info);
static void                 eval_instr_typeof(struct virtual_machine *vm, struct mir_instr_typeof *type_of);
static void                 eval_instr_call_loc(struct virtual_machine *vm, struct mir_instr_call_loc *loc);
static void                 eval_instr_test_cases(struct virtual_machine *vm, struct mir_instr_test_case *tc);
static void                 eval_instr_member_ptr(struct virtual_machine      *vm,
                                                  struct mir_instr_member_ptr *member_ptr);
static void                 eval_instr_elem_ptr(struct virtual_machine *vm, struct mir_instr_elem_ptr *elem_ptr);
static void                 eval_instr_decl_var(struct virtual_machine *vm, struct mir_instr_decl_var *decl_var);
static void                 eval_instr_decl_ref(struct virtual_machine *vm, struct mir_instr_decl_ref *decl_ref);
static void                 eval_instr_decl_direct_ref(struct virtual_machine           *vm,
                                                       struct mir_instr_decl_direct_ref *decl_ref);
static void                 eval_instr_binop(struct virtual_machine *vm, struct mir_instr_binop *binop);
static void                 eval_instr_unop(struct virtual_machine *vm, struct mir_instr_unop *unop);
static void                 eval_instr_load(struct virtual_machine *vm, struct mir_instr_load *load);
static void                 eval_instr_addrof(struct virtual_machine *vm, struct mir_instr_addrof *addrof);
static void                 eval_instr_set_initializer(struct virtual_machine           *vm,
                                                       struct mir_instr_set_initializer *si);
static void                 eval_instr_cast(struct virtual_machine *vm, struct mir_instr_cast *cast);
static void                 eval_instr_compound(struct virtual_machine *vm, struct mir_instr_compound *cmp);
static void                 eval_instr_unroll(struct virtual_machine *vm, struct mir_instr_unroll *unroll);
static void                 eval_instr_arg(struct virtual_machine *vm, struct mir_instr_arg *arg);

// =================================================================================================
// Inlines
// =================================================================================================
static inline bool fn_does_return(struct mir_fn *fn) {
	return fn->type->data.fn.ret_type->kind != MIR_TYPE_VOID;
}

// Checks whether constant value needs some extra space or if it fits into small memory block hold
// by the value itself.
static inline bool needs_allocation(struct mir_const_expr_value *v) {
	return v->type->store_size_bytes > sizeof(v->_tmp);
}

static inline void eval_abort(struct virtual_machine *vm) {
	vm->aborted = true;
}

// =================================================================================================
// Execution stack manipulation
// =================================================================================================
#define stack_alloc_size(s) ((s) + (VM_MAX_ALIGNMENT - ((s) % VM_MAX_ALIGNMENT)))

static inline struct vm_stack *reset_stack(struct vm_stack *stack);

static struct vm_stack *create_stack(const usize bytes) {
	bassert(bytes > 0 && "Invalid stack size!");
	struct vm_stack *stack = bmalloc(sizeof(char) * bytes);
	stack->allocated_bytes = bytes;
	reset_stack(stack);
	return stack;
}

static inline void terminate_stack(struct vm_stack *stack) {
	bfree(stack);
}

struct vm_stack *reset_stack(struct vm_stack *stack) {
	bassert(stack && stack->allocated_bytes > 0);
	stack->pc         = NULL;
	stack->ra         = NULL;
	stack->prev_block = NULL;
	stack->top_ptr    = (u8 *)stack + stack_alloc_size(sizeof(struct vm_stack));
	return stack;
}

static inline struct vm_stack *swap_current_stack(struct virtual_machine *vm,
                                                  struct vm_stack        *stack) {
	bassert(stack);
	struct vm_stack *previous_stack = vm->stack;
	vm->stack                       = stack;
	vmdbg_notify_stack_swap();
	return previous_stack;
}

static inline vm_stack_ptr_t stack_alloc(struct virtual_machine *vm, usize size) {
	bassert(size && "trying to allocate 0 bytes on stack");
	size               = stack_alloc_size(size);
	vm_stack_ptr_t mem = vm->stack->top_ptr;
	vm->stack->top_ptr += size;
	if (vm->stack->top_ptr > ((u8 *)(vm->stack)) + vm->stack->allocated_bytes) {
		builder_error("Internal execution stack overflow.");
		vm_abort(vm);
	}
	bassert(is_aligned(mem, VM_MAX_ALIGNMENT));
	return mem;
}

// shift stack top by the size in bytes
static inline vm_stack_ptr_t stack_free(struct virtual_machine *vm, usize size) {
	size                   = stack_alloc_size(size);
	vm_stack_ptr_t new_top = vm->stack->top_ptr - size;
	if (new_top < (u8 *)(vm->stack->ra + 1)) {
		builder_error("Internal execution stack underflow.");
		vm_abort(vm);
	}
	vm->stack->top_ptr = new_top;
	return new_top;
}

static inline void push_ra(struct virtual_machine *vm, struct mir_instr_call *caller) {
	struct vm_frame *tmp = (struct vm_frame *)stack_alloc(vm, sizeof(struct vm_frame));
	tmp->caller          = caller;
	tmp->prev            = vm->stack->ra;
	vm->stack->ra        = tmp;
	vmdbg_notify_stack_op(VMDBG_PUSH_RA, NULL, tmp);
}

static inline struct mir_instr_call *pop_ra(struct virtual_machine *vm) {
	if (!vm->stack->ra) return NULL;
	struct mir_instr_call *caller = vm->stack->ra->caller;
	// rollback
	vm_stack_ptr_t new_top_ptr = (vm_stack_ptr_t)vm->stack->ra;
	vm->stack->top_ptr         = new_top_ptr;
	vm->stack->ra              = vm->stack->ra->prev;
	vmdbg_notify_stack_op(VMDBG_POP_RA, NULL, new_top_ptr);
	return caller;
}

static inline vm_stack_ptr_t stack_push_empty(struct virtual_machine *vm, struct mir_type *type) {
	bassert(type);
	const usize size = type->store_size_bytes;
	bassert(size && "pushing zero sized data on stack");
	vm_stack_ptr_t tmp = stack_alloc(vm, size);
	vmdbg_notify_stack_op(VMDBG_PUSH, type, tmp);
	return tmp;
}

static inline vm_stack_ptr_t stack_push(struct virtual_machine *vm, void *value, struct mir_type *type) {
	bassert(value && "try to push NULL value");
	vm_stack_ptr_t tmp = stack_push_empty(vm, type);
	memcpy(tmp, value, type->store_size_bytes);
	return tmp;
}

static inline vm_stack_ptr_t stack_pop(struct virtual_machine *vm, struct mir_type *type) {
	bassert(type);
	const usize size = type->store_size_bytes;
	bassert(size && "Popping zero sized data from stack.");
	const vm_stack_ptr_t ptr = stack_free(vm, size);
	vmdbg_notify_stack_op(VMDBG_POP, type, ptr);
	return ptr;
}

static inline vm_stack_ptr_t stack_peek(struct virtual_machine *vm, struct mir_type *type) {
	usize size = type->store_size_bytes;
	bassert(size && "Peeking zero sized data on stack.");
	size               = stack_alloc_size(size);
	vm_stack_ptr_t top = vm->stack->top_ptr - size;
	if (top < (u8 *)(vm->stack->ra + 1)) babort("Stack underflow!!!");
	return top;
}

// Convert relative local address of variable into absolute address in memory.
static inline vm_stack_ptr_t stack_rel_to_abs_ptr(struct virtual_machine *vm,
                                                  vm_relative_stack_ptr_t rel_ptr) {
	bassert(rel_ptr);
	vm_stack_ptr_t base = (vm_stack_ptr_t)vm->stack->ra;
	bassert(base);
	return base + rel_ptr;
}

// =================================================================================================
// Data buffer
// =================================================================================================
#define DATA_BUFFER_PAGE_SIZE 2048 // bytes

static struct vm_bufpage *data_page_alloc(struct vm_bufpage *prev, usize size_needed) {
	size_needed             = size_needed > DATA_BUFFER_PAGE_SIZE ? size_needed : DATA_BUFFER_PAGE_SIZE;
	struct vm_bufpage *page = bmalloc(size_needed + sizeof(struct vm_bufpage));
	page->prev              = prev;
	page->len               = 0;
	page->cap               = size_needed;
	page->top               = (vm_stack_ptr_t)(page + 1);
	return page;
}

static vm_stack_ptr_t data_alloc(struct virtual_machine *vm, struct mir_type *type) {
	zone();
	bassert(type->store_size_bytes > 0);
	const usize size_needed = type->store_size_bytes + (usize)type->alignment;
	const usize alignment   = (usize)type->alignment;
	if (!vm->data) vm->data = data_page_alloc(NULL, size_needed);
	// lookup free space, if no suitable was found, allocate new block!
	struct vm_bufpage *found = vm->data;
	while (found) {
		if (found->len + size_needed <= found->cap) {
			break;
		}
		found = found->prev;
	}
	if (!found) {
		vm->data = data_page_alloc(vm->data, size_needed);
		found    = vm->data;
	}
	bassert(found);
	vm_stack_ptr_t ptr = next_aligned(found->top + found->len, alignment);
	bassert(is_aligned(ptr, alignment) && "Invalid allocation alignment!");
	found->len += size_needed;
	return_zone(ptr);
}

static void data_free(struct virtual_machine *vm) {
	struct vm_bufpage *current = vm->data;
	while (current) {
		struct vm_bufpage *tmp = current;
		current                = current->prev;
		bfree(tmp);
	}
	vm->data = NULL;
}

// Fetch value; use internal ConstExprValue storage if value is compile time known, otherwise use
// stack.
static inline vm_stack_ptr_t fetch_value(struct virtual_machine *vm, struct mir_const_expr_value *v) {
	if (v->is_comptime) return v->data;
	return stack_pop(vm, v->type);
}

// Similar to fetch_value but in case value comes from stack we keep it there.
static inline vm_stack_ptr_t peek_value(struct virtual_machine *vm, struct mir_const_expr_value *v) {
	if (v->is_comptime) return v->data;
	return stack_peek(vm, v->type);
}

static inline struct mir_instr *get_pc(struct virtual_machine *vm) {
	return vm->stack->pc;
}

static inline struct vm_frame *get_ra(struct virtual_machine *vm) {
	return vm->stack->ra;
}

static inline void set_pc(struct virtual_machine *vm, struct mir_instr *instr) {
	vm->stack->pc = instr;
}

static inline void stack_alloc_var(struct virtual_machine *vm, struct mir_var *var) {
	bassert(var);
	bassert(!var->value.is_comptime && "Cannot allocate compile time constant");
	bassert(isnotflag(var->iflags, MIR_VAR_GLOBAL) &&
	        "This function is not supposed to be used for allocation of global "
	        "variables, use vm_alloc_global instead.");
	vm_stack_ptr_t tmp = stack_push_empty(vm, var->value.type);
	var->vm_ptr.local  = tmp - (vm_stack_ptr_t)vm->stack->ra;
}

static inline void stack_alloc_local_vars(struct virtual_machine *vm, struct mir_fn *fn) {
	bassert(fn);
	// Init all stack variables.
	for (usize i = 0; i < arrlenu(fn->variables); ++i) {
		struct mir_var *var = fn->variables[i];
		if (var->value.is_comptime) continue;
		if (var->ref_count == 0) continue;
		stack_alloc_var(vm, var);
	}
}

//********/
//* impl */
//********/
void calculate_binop(struct mir_type *src_type,
                     vm_stack_ptr_t   dest,
                     vm_stack_ptr_t   lhs,
                     vm_stack_ptr_t   rhs,
                     enum binop_kind  op) {
#define ARITHMETIC(T)                                                  \
	case BINOP_ADD:                                                    \
		vm_write_as(T, dest, vm_read_as(T, lhs) + vm_read_as(T, rhs)); \
		break;                                                         \
	case BINOP_SUB:                                                    \
		vm_write_as(T, dest, vm_read_as(T, lhs) - vm_read_as(T, rhs)); \
		break;                                                         \
	case BINOP_MUL:                                                    \
		vm_write_as(T, dest, vm_read_as(T, lhs) * vm_read_as(T, rhs)); \
		break;

#define ARITHMETIC_DIV_INT(T)                                                            \
	case BINOP_DIV:                                                                      \
		if (vm_read_as(T, rhs) == 0) babort("Divide by zero, this should be an error!"); \
		vm_write_as(T, dest, vm_read_as(T, lhs) / vm_read_as(T, rhs));                   \
		break;

#define ARITHMETIC_DIV_FLOAT(T)                                        \
	case BINOP_DIV:                                                    \
		vm_write_as(T, dest, vm_read_as(T, lhs) / vm_read_as(T, rhs)); \
		break;

#define RELATIONAL(T)                                                      \
	case BINOP_EQ:                                                         \
		vm_write_as(bool, dest, vm_read_as(T, lhs) == vm_read_as(T, rhs)); \
		break;                                                             \
	case BINOP_NEQ:                                                        \
		vm_write_as(bool, dest, vm_read_as(T, lhs) != vm_read_as(T, rhs)); \
		break;                                                             \
	case BINOP_GREATER:                                                    \
		vm_write_as(bool, dest, vm_read_as(T, lhs) > vm_read_as(T, rhs));  \
		break;                                                             \
	case BINOP_LESS:                                                       \
		vm_write_as(bool, dest, vm_read_as(T, lhs) < vm_read_as(T, rhs));  \
		break;                                                             \
	case BINOP_LESS_EQ:                                                    \
		vm_write_as(bool, dest, vm_read_as(T, lhs) <= vm_read_as(T, rhs)); \
		break;                                                             \
	case BINOP_GREATER_EQ:                                                 \
		vm_write_as(bool, dest, vm_read_as(T, lhs) >= vm_read_as(T, rhs)); \
		break;

#define LOGICAL(T)                                                     \
	case BINOP_AND:                                                    \
		vm_write_as(T, dest, vm_read_as(T, lhs) & vm_read_as(T, rhs)); \
		break;                                                         \
	case BINOP_OR:                                                     \
		vm_write_as(T, dest, vm_read_as(T, lhs) | vm_read_as(T, rhs)); \
		break;                                                         \
	case BINOP_XOR:                                                    \
		vm_write_as(T, dest, vm_read_as(T, lhs) ^ vm_read_as(T, rhs)); \
		break;

#define OTHER(T)                                                        \
	case BINOP_MOD:                                                     \
		vm_write_as(T, dest, vm_read_as(T, lhs) % vm_read_as(T, rhs));  \
		break;                                                          \
	case BINOP_SHR:                                                     \
		vm_write_as(T, dest, vm_read_as(T, lhs) >> vm_read_as(T, rhs)); \
		break;                                                          \
	case BINOP_SHL:                                                     \
		vm_write_as(T, dest, vm_read_as(T, lhs) << vm_read_as(T, rhs)); \
		break;

	// Valid types: integers, floats, doubles, enums (as ints), bool, pointers.

	const usize size    = src_type->store_size_bytes;
	const bool  is_real = src_type->kind == MIR_TYPE_REAL;
	const bool  is_signed =
	    (src_type->kind == MIR_TYPE_INT && src_type->data.integer.is_signed) ||
	    (src_type->kind == MIR_TYPE_ENUM && src_type->data.enm.base_type->data.integer.is_signed);

	if (is_real) { // f32 or f64
		if (size == 4) {
			switch (op) {
				ARITHMETIC(f32)
				ARITHMETIC_DIV_FLOAT(f32)
				RELATIONAL(f32)
			default:
				babort("Invalid binary operation!");
			}
		} else if (size == 8) {
			switch (op) {
				ARITHMETIC(f64)
				ARITHMETIC_DIV_FLOAT(f64)
				RELATIONAL(f64)
			default:
				babort("Invalid binary operation!");
			}
		} else {
			abort();
		}
	} else if (is_signed) { // signed integers
		if (size == 1) {
			switch (op) {
				ARITHMETIC(s8)
				ARITHMETIC_DIV_INT(s8)
				RELATIONAL(s8)
				LOGICAL(s8)
				OTHER(s8)
			default:
				babort("Invalid binary operation!");
			}
		} else if (size == 2) {
			switch (op) {
				ARITHMETIC(s16)
				ARITHMETIC_DIV_INT(s16)
				RELATIONAL(s16)
				LOGICAL(s16)
				OTHER(s16)
			default:
				babort("Invalid binary operation!");
			}
		} else if (size == 4) {
			switch (op) {
				ARITHMETIC(s32)
				ARITHMETIC_DIV_INT(s32)
				RELATIONAL(s32)
				LOGICAL(s32)
				OTHER(s32)
			default:
				babort("Invalid binary operation!");
			}
		} else if (size == 8) {
			switch (op) {
				ARITHMETIC(s64)
				ARITHMETIC_DIV_INT(s64)
				RELATIONAL(s64)
				LOGICAL(s64)
				OTHER(s64)
			default:
				babort("Invalid binary operation!");
			}
		} else {
			abort();
		}
	} else { // unsigned integers
		if (size == 1) {
			switch (op) {
				ARITHMETIC(u8)
				ARITHMETIC_DIV_INT(u8)
				RELATIONAL(u8)
				LOGICAL(u8)
				OTHER(u8)
			default:
				babort("Invalid binary operation!");
			}
		} else if (size == 2) {
			switch (op) {
				ARITHMETIC(u16)
				ARITHMETIC_DIV_INT(u16)
				RELATIONAL(u16)
				LOGICAL(u16)
				OTHER(u16)
			default:
				babort("Invalid binary operation!");
			}
		} else if (size == 4) {
			switch (op) {
				ARITHMETIC(u32)
				ARITHMETIC_DIV_INT(u32)
				RELATIONAL(u32)
				LOGICAL(u32)
				OTHER(u32)
			default:
				babort("Invalid binary operation!");
			}
		} else if (size == 8) {
			switch (op) {
				ARITHMETIC(u64)
				ARITHMETIC_DIV_INT(u64)
				RELATIONAL(u64)
				LOGICAL(u64)
				OTHER(u64)
			default:
				babort("Invalid binary operation!");
			}
		} else {
			abort();
		}
	}

#undef ARITHMETIC
#undef ARITHMETIC_DIV_FLOAT
#undef ARITHMETIC_DIV_INT
#undef RELATIONAL
#undef LOGICAL
#undef OTHER
}

void calculate_unop(vm_stack_ptr_t dest, vm_stack_ptr_t v, enum unop_kind op, struct mir_type *type) {
#define UNOP_CASE(T)                                     \
	case sizeof(T): {                                    \
		switch (op) {                                    \
		case UNOP_NOT:                                   \
			vm_write_as(T, dest, !vm_read_as(T, v));     \
			break;                                       \
		case UNOP_BIT_NOT:                               \
			vm_write_as(T, dest, ~vm_read_as(T, v));     \
			break;                                       \
		case UNOP_NEG:                                   \
			vm_write_as(T, dest, vm_read_as(T, v) * -1); \
			break;                                       \
		case UNOP_POS:                                   \
			vm_write_as(T, dest, vm_read_as(T, v));      \
			break;                                       \
		default:                                         \
			BL_UNIMPLEMENTED;                            \
		}                                                \
	} break

#define UNOP_CASE_REAL(T)                                \
	case sizeof(T): {                                    \
		switch (op) {                                    \
		case UNOP_NOT:                                   \
			vm_write_as(T, dest, !vm_read_as(T, v));     \
			break;                                       \
		case UNOP_NEG:                                   \
			vm_write_as(T, dest, vm_read_as(T, v) * -1); \
			break;                                       \
		case UNOP_POS:                                   \
			vm_write_as(T, dest, vm_read_as(T, v));      \
			break;                                       \
		default:                                         \
			BL_UNIMPLEMENTED;                            \
		}                                                \
	} break

	const usize s = type->store_size_bytes;

	switch (type->kind) {
	case MIR_TYPE_ENUM:
		bassert(type->data.enm.is_flags);
		type = type->data.enm.base_type;
		// fall through
	case MIR_TYPE_BOOL:
		// fall through
	case MIR_TYPE_INT: {
		if (type->data.integer.is_signed) {
			switch (s) {
				UNOP_CASE(s8);
				UNOP_CASE(s16);
				UNOP_CASE(s32);
				UNOP_CASE(s64);
			default:
				babort("invalid integer data type");
			}
		} else {
			switch (s) {
				UNOP_CASE(u8);
				UNOP_CASE(u16);
				UNOP_CASE(u32);
				UNOP_CASE(u64);
			default:
				babort("invalid integer data type");
			}
		}
		break;
	}

	case MIR_TYPE_REAL: {
		switch (s) {
			UNOP_CASE_REAL(f32);
			UNOP_CASE_REAL(f64);
		default:
			babort("invalid real data type");
		}
		break;
	}

	default:
		babort("invalid unop type");
	}
#undef UNOP_CASE
#undef UNOP_CASE_REAL
}

//
// Dyncall
//

// Exported dlib_* might be used by dlib.bl module in case we're in VM runtime. For regular binary
// execution linked static library (exporting these functions) is used.

BL_EXPORT void *__dlib_open(const char *libname) {
#if BL_ASSERT_ENABLE
	struct assembly *assembly = builder.current_executed_assembly;
	bassert(assembly);
#endif
	return dlLoadLibrary(libname);
}

BL_EXPORT void __dlib_close(DLLib *lib) {
	if (!lib) return;
	dlFreeLibrary(lib);
}

BL_EXPORT void *__dlib_symbol(DLLib *lib, const char *symname) {
	struct assembly *assembly = builder.current_executed_assembly;
	bassert(assembly);

	if (!symname || strlen(symname) == 0) return NULL;
	if (!lib) return NULL;

	void *sym = dlFindSymbol(lib, symname);
	if (!sym) return NULL;

	// Create function for the found symbol.

	const u32          thread_index = get_worker_index();
	struct mir_arenas *arenas       = &assembly->thread_local_contexts[thread_index].mir_arenas;
	struct mir_fn     *fn           = arena_alloc(&arenas->fn);
	bmagic_set(fn);
	fn->flags                = FLAG_EXTERN;
	fn->is_global            = true;
	fn->dyncall.extern_entry = sym;
	fn->is_fully_analyzed    = true;

	return fn;
}

void dyncall_cb_read_arg(struct virtual_machine       UNUSED(*vm),
                         struct mir_const_expr_value *dest_value,
                         DCArgs                      *src) {
	vm_stack_ptr_t   dest = dest_value->data;
	struct mir_type *type = dest_value->type;

	bassert(dest && "Argument destination is invalid!");
	bassert(type && "Argument destination has no type specified!");

	memset(dest, 0, sizeof(*dest_value->_tmp));

	switch (type->kind) {
	case MIR_TYPE_ENUM:
	case MIR_TYPE_INT: {
		const usize bitcount = (usize)(type->kind == MIR_TYPE_INT ? type->data.integer.bitcount : type->data.enm.base_type->size_bits);
		u64         v        = 0;
		switch (bitcount) {
		case 8:
			v = dcbArgUChar(src);
			break;
		case 16:
			v = dcbArgUShort(src);
			break;
		case 32:
			v = dcbArgULong(src);
			break;
		case 64:
			v = dcbArgULongLong(src);
			break;
		default:
			babort("invalid bitcount");
		}

		vm_write_int(type, dest, v);
		break;
	}

	case MIR_TYPE_REAL: {
		const usize bitcount = type->data.real.bitcount;
		switch (bitcount) {
		case 32:
			vm_write_float(type, dest, dcbArgFloat(src));
			break;
		case 64:
			vm_write_double(type, dest, dcbArgDouble(src));
			break;
		default:
			babort("invalid bitcount");
		}

		break;
	}

	case MIR_TYPE_BOOL: {
		vm_write_int(type, dest, dcbArgBool(src));
		break;
	}

	case MIR_TYPE_PTR: {
		vm_write_ptr(type, dest, dcbArgPointer(src));
		break;
	}

	default:
		BL_UNIMPLEMENTED;
	}
}

char dyncall_cb_handler(DCCallback UNUSED(*cb), DCArgs *dc_args, DCValue *result, void *userdata) {
	// TODO: External callback can be invoked from different thread. This can cause problems for
	//  now since interpreter is strictly single-threaded, but we must handle such situation in
	//  future.
	struct dyncall_cb_context *ctx = (struct dyncall_cb_context *)userdata;
	struct mir_fn             *fn  = ctx->fn;
	struct virtual_machine    *vm  = ctx->vm;
	bassert(fn && vm);

	struct mir_type *ret_type = fn->type->data.fn.ret_type;
	const bool       is_extern =
	    isflag(ctx->fn->flags, FLAG_EXTERN) || isflag(ctx->fn->flags, FLAG_INTRINSIC);

	if (is_extern) {
		// TODO: external callback
		babort("External function used as callback is not supported yet!");
	}

	mir_const_values_t arg_tmp = SARR_ZERO;
	mir_args_t        *args    = fn->type->data.fn.args;
	if (sarrlenu(args)) {
		sarrsetlen(&arg_tmp, sarrlenu(args));
		for (usize i = 0; i < sarrlenu(args); ++i) {
			struct mir_arg              *it = sarrpeek(args, i);
			struct mir_const_expr_value *v  = &sarrpeek(&arg_tmp, i);
			v->type                         = it->type;
			v->data                         = &v->_tmp[0];

			dyncall_cb_read_arg(vm, v, dc_args);
		}
	}

	vm_stack_ptr_t             ret_ptr = NULL;
	const enum vm_interp_state state   = vm_execute_fn(vm, vm->assembly, fn, &arg_tmp, &ret_ptr);
	if (state != VM_INTERP_PASSED) {
		result->L = 0;
	} else if (fn_does_return(fn)) {
		// @Incomplete: Does this work with other types than 64bit ints???
		bassert(ret_ptr && "Function is supposed to return some value.");
		result->L = vm_read_int(ret_type, ret_ptr);
	}

	sarrfree(&arg_tmp);
	return dyncall_generate_signature(vm, ret_type)[0];
}

// @Performance: Remove recursive calls.
void _dyncall_generate_signature(struct virtual_machine *vm, struct mir_type *type) {
	switch (type->kind) {
	case MIR_TYPE_FN: {
		mir_args_t *args = type->data.fn.args;
		for (usize i = 0; i < sarrlenu(args); ++i) {
			struct mir_arg *arg = sarrpeek(args, i);
			_dyncall_generate_signature(vm, arg->type);
		}
		arrput(vm->dcsigtmp, DC_SIGCHAR_ENDARG);
		_dyncall_generate_signature(vm, type->data.fn.ret_type);
		break;
	}

	case MIR_TYPE_INT: {
		const bool is_signed = type->data.integer.is_signed;
		switch (type->store_size_bytes) {
		case 1:
			arrput(vm->dcsigtmp, is_signed ? DC_SIGCHAR_CHAR : DC_SIGCHAR_UCHAR);
			break;
		case 2:
			arrput(vm->dcsigtmp, is_signed ? DC_SIGCHAR_SHORT : DC_SIGCHAR_USHORT);
			break;
		case 4:
			arrput(vm->dcsigtmp, is_signed ? DC_SIGCHAR_INT : DC_SIGCHAR_UINT);
			break;
		case 8:
			arrput(vm->dcsigtmp, is_signed ? DC_SIGCHAR_LONGLONG : DC_SIGCHAR_ULONGLONG);
			break;
		}
		break;
	}

	case MIR_TYPE_REAL: {
		switch (type->store_size_bytes) {
		case 4:
			arrput(vm->dcsigtmp, DC_SIGCHAR_FLOAT);
			break;
		case 8:
			arrput(vm->dcsigtmp, DC_SIGCHAR_DOUBLE);
			break;
		}
		break;
	}

	case MIR_TYPE_NULL:
	case MIR_TYPE_PTR: {
		arrput(vm->dcsigtmp, DC_SIGCHAR_POINTER);
		break;
	}

	case MIR_TYPE_VOID: {
		arrput(vm->dcsigtmp, DC_SIGCHAR_VOID);
		break;
	}

	case MIR_TYPE_STRUCT: {
		mir_members_t *members = type->data.strct.members;
		for (usize i = 0; i < sarrlenu(members); ++i) {
			struct mir_member *member = sarrpeek(members, i);
			_dyncall_generate_signature(vm, member->type);
		}
		break;
	}

	case MIR_TYPE_ENUM: {
		_dyncall_generate_signature(vm, type->data.enm.base_type);
		break;
	}

	case MIR_TYPE_ARRAY: {
		for (s64 i = 0; i < type->data.array.len; i += 1) {
			_dyncall_generate_signature(vm, type->data.array.elem_type);
		}
		break;
	}

	default: {
		str_buf_t type_name = mir_type2str(type, true);
		babort("Unsupported DC-signature type '%s'.", str_buf_to_c(type_name));
		// put_tmp_str(type_name);
	}
	}
}

const char *dyncall_generate_signature(struct virtual_machine *vm, struct mir_type *type) {
	arrsetlen(vm->dcsigtmp, 0);
	_dyncall_generate_signature(vm, type);
	arrput(vm->dcsigtmp, '\0');
	return vm->dcsigtmp;
}

DCCallback *dyncall_fetch_callback(struct virtual_machine *vm, struct mir_fn *fn) {
	if (fn->dyncall.extern_callback_handle) return fn->dyncall.extern_callback_handle;
	const char *sig     = dyncall_generate_signature(vm, fn->type);
	fn->dyncall.context = (struct dyncall_cb_context){.fn = fn, .vm = vm};
	fn->dyncall.extern_callback_handle =
	    dcbNewCallback(sig, &dyncall_cb_handler, &fn->dyncall.context);
	return fn->dyncall.extern_callback_handle;
}

void dyncall_push_arg(struct virtual_machine *vm, vm_stack_ptr_t val_ptr, struct mir_type *type) {
	bassert(type);

	DCCallVM *dvm = vm->assembly->dc_vm;
	bassert(dvm);

	if (type->kind == MIR_TYPE_ENUM) {
		type = type->data.enm.base_type;
	}

	switch (type->kind) {
	case MIR_TYPE_BOOL: {
		dcArgBool(dvm, (DCbool)vm_read_int(type, val_ptr));
		break;
	}

	case MIR_TYPE_INT: {
		const u64 v = vm_read_int(type, val_ptr);
		switch (type->store_size_bytes) {
		case 1:
			dcArgChar(dvm, (DCchar)v);
			break;
		case 2:
			dcArgShort(dvm, (DCshort)v);
			break;
		case 4:
			dcArgInt(dvm, (DCint)v);
			break;
		case 8:
			dcArgLongLong(dvm, (DClonglong)v);
			break;
		default:
			babort("unsupported external call integer argument type");
		}
		break;
	}

	case MIR_TYPE_REAL: {
		switch (type->store_size_bytes) {
		case 4:
			dcArgFloat(dvm, vm_read_float(type, val_ptr));
			break;
		case 8:
			dcArgDouble(dvm, vm_read_double(type, val_ptr));
			break;
		default:
			babort("unsupported external call integer argument type");
		}
		break;
	}

	case MIR_TYPE_NULL: {
		dcArgPointer(dvm, NULL);
		break;
	}

	case MIR_TYPE_STRUCT: {
		babort("External function taking structure argument by value cannot be executed by "
		       "interpreter on this platform.");
		break;
	}

	case MIR_TYPE_ARRAY: {
		babort("External function taking array argument by value cannot be executed by "
		       "interpreter on this platform.");
		break;
	}

	case MIR_TYPE_PTR: {
		vm_stack_ptr_t tmp = vm_read_ptr(type, val_ptr);
		if (mir_deref_type(type)->kind == MIR_TYPE_FN) {
			// Function pointer!
			struct mir_fn *fn = (struct mir_fn *)tmp;
			bassert(fn);
			dcArgPointer(dvm, (DCpointer)dyncall_fetch_callback(vm, fn));
		} else {
			dcArgPointer(dvm, (DCpointer)tmp);
		}
		break;
	}

	default:
		babort("unsupported external call argument type");
	}
}

void interp_extern_call(struct virtual_machine *vm, struct mir_instr_call *call, str_t linkage_name, struct mir_type *fn_type, DCpointer handle) {
	bassert(fn_type && fn_type->kind == MIR_TYPE_FN);
	bassert(call);
	struct mir_type *ret_type = fn_type->data.fn.ret_type;
	bassert(ret_type);

	DCCallVM *dvm = vm->assembly->dc_vm;
	bassert(vm);

	// call setup and clenup
	if (!handle) {
		builder_error("External function '" STR_FMT "' not found!", STR_ARG(linkage_name));
		vm_abort(vm);
		return;
	}

	dcMode(dvm, DC_CALL_C_DEFAULT);
	dcReset(dvm);

	mir_instrs_t *arg_values = call->args;
	for (usize i = 0; i < sarrlenu(arg_values); ++i) {
		struct mir_instr *arg_value = sarrpeek(arg_values, i);
		vm_stack_ptr_t    arg_ptr   = fetch_value(vm, &arg_value->value);
		dyncall_push_arg(vm, arg_ptr, arg_value->value.type);
	}

	bool does_return = true;

	vm_value_t result = {0};
	switch (ret_type->kind) {
	case MIR_TYPE_ENUM:
	case MIR_TYPE_INT:
		switch (ret_type->store_size_bytes) {
		case 1:
			vm_write_as(s8, &result, dcCallChar(dvm, handle));
			break;
		case 2:
			vm_write_as(s16, &result, dcCallShort(dvm, handle));
			break;
		case 4:
			vm_write_as(s32, &result, dcCallInt(dvm, handle));
			break;
		case 8:
			vm_write_as(s64, &result, dcCallLongLong(dvm, handle));
			break;
		default:
			babort("unsupported integer size for external call result");
		}
		break;

	case MIR_TYPE_PTR:
		vm_write_as(vm_stack_ptr_t, &result, dcCallPointer(dvm, handle));
		break;

	case MIR_TYPE_REAL: {
		switch (ret_type->store_size_bytes) {
		case 4:
			vm_write_as(f32, &result, dcCallFloat(dvm, handle));
			break;
		case 8:
			vm_write_as(f64, &result, dcCallDouble(dvm, handle));
			break;
		default:
			babort("Unsupported real number size for external call result");
		}
		break;
	}

	case MIR_TYPE_VOID:
		dcCallVoid(dvm, handle);
		does_return = false;
		break;

	case MIR_TYPE_STRUCT: {
		babort("External function '" STR_FMT "' returning structure cannot be executed by interpreter on "
		       "this platform.",
		       STR_ARG(linkage_name));
	}

	case MIR_TYPE_ARRAY: {
		babort("External function '" STR_FMT "' returning array cannot be executed by interpreter on "
		       "this platform.",
		       STR_ARG(linkage_name));
	}

	case MIR_TYPE_BOOL: {
		vm_write_as(s32, &result, dcCallInt(dvm, handle));
		break;
	}

	default: {
		str_buf_t type_name = mir_type2str(ret_type, true);
		babort("Unsupported external call return type '%s'", str_buf_to_c(type_name));
	}
	}

	// PUSH result only if it is used
	if (call->base.ref_count > 1 && does_return) {
		stack_push(vm, (vm_stack_ptr_t)&result, ret_type);
	}
}

// Execute top level function directly.
// - Expects all arguments already on stack.
// - Return value can be eventually pushed on the stack after execution.
// - Current execution stack pointer is restored in case the execution failed; there is no valid
//   return value on the stack even if the function is supposed to return one.
enum vm_interp_state execute_function(struct virtual_machine *vm,
                                      struct mir_fn          *fn,
                                      struct mir_instr_call  *optional_call,
                                      const bool              resume) {
	const struct mir_instr *fn_terminal_instr = &fn->terminal_instr->base;
	// Reset eventual previous failed state.
	vm->aborted = false;
	if (!resume) {
		struct mir_instr *fn_entry_instr = fn->first_block->entry_instr;
		// push terminal frame on stack
		push_ra(vm, optional_call);
		// allocate local variables
		stack_alloc_local_vars(vm, fn);
		// setup entry instruction
		set_pc(vm, fn_entry_instr);
	}
	// iterate over entry block of executable
	struct mir_instr    *instr, *prev;
	enum vm_interp_state state = VM_INTERP_PASSED;
	while (true) {
		instr = get_pc(vm);
		prev  = instr;
		if (!instr) break;
		if (instr->state != MIR_IS_COMPLETE) {
			state = VM_INTERP_POSTPONE;
		} else {
			state = interp_instr(vm, instr);
		}
		if (state != VM_INTERP_PASSED) break;
		// When we reach terminal instruction of this function, we must stop the execution,
		// otherwise when the interpreted call lives in scope of other function, interpreter will
		// continue with execution.
		if (instr == fn_terminal_instr) break;
		// Stack head can be changed by br instructions.
		if (!get_pc(vm) || get_pc(vm) == prev) set_pc(vm, instr->next);
	}
	switch (state) {
	case VM_INTERP_ABORT:
		// @Incomplete: endless loop?
		while (pop_ra(vm) != optional_call)
			;
		break;
	default:
		break;
	}
	return state;
}

enum vm_interp_state interp_instr(struct virtual_machine *vm, struct mir_instr *instr) {
	bassert(instr);
	bassert(instr->state == MIR_IS_COMPLETE);
	if (vm->assembly->target->vmdbg_break_on == (s32)instr->id) {
		vmdbg_break();
	}
	vmdbg_notify_instr(instr);
	// Skip all comptimes.
	enum vm_interp_state state = VM_INTERP_PASSED;
	if (mir_is_comptime(instr)) return state;

	const enum mir_instr_kind kind = instr->kind;

	switch (kind) {
	case MIR_INSTR_CAST:
		interp_instr_cast(vm, (struct mir_instr_cast *)instr);
		break;
	case MIR_INSTR_ADDROF:
		interp_instr_addrof(vm, (struct mir_instr_addrof *)instr);
		break;
	case MIR_INSTR_BINOP:
		interp_instr_binop(vm, (struct mir_instr_binop *)instr);
		break;
	case MIR_INSTR_UNOP:
		interp_instr_unop(vm, (struct mir_instr_unop *)instr);
		break;
	case MIR_INSTR_CALL:
		state = interp_instr_call(vm, (struct mir_instr_call *)instr);
		break;
	case MIR_INSTR_RET:
		interp_instr_ret(vm, (struct mir_instr_ret *)instr);
		break;
	case MIR_INSTR_DECL_VAR:
		interp_instr_decl_var(vm, (struct mir_instr_decl_var *)instr);
		break;
	case MIR_INSTR_DECL_REF:
		interp_instr_decl_ref(vm, (struct mir_instr_decl_ref *)instr);
		break;
	case MIR_INSTR_DECL_DIRECT_REF:
		interp_instr_decl_direct_ref(vm, (struct mir_instr_decl_direct_ref *)instr);
		break;
	case MIR_INSTR_STORE:
		interp_instr_store(vm, (struct mir_instr_store *)instr);
		break;
	case MIR_INSTR_LOAD:
		interp_instr_load(vm, (struct mir_instr_load *)instr);
		break;
	case MIR_INSTR_BR:
		interp_instr_br(vm, (struct mir_instr_br *)instr);
		break;
	case MIR_INSTR_COND_BR:
		interp_instr_cond_br(vm, (struct mir_instr_cond_br *)instr);
		break;
	case MIR_INSTR_PHI:
		interp_instr_phi(vm, (struct mir_instr_phi *)instr);
		break;
	case MIR_INSTR_UNREACHABLE:
		interp_instr_unreachable(vm, (struct mir_instr_unreachable *)instr);
		break;
	case MIR_INSTR_DEBUGBREAK:
		interp_instr_debugbreak(vm, (struct mir_instr_debugbreak *)instr);
		break;
	case MIR_INSTR_ARG:
		interp_instr_arg(vm, (struct mir_instr_arg *)instr);
		break;
	case MIR_INSTR_ELEM_PTR:
		interp_instr_elem_ptr(vm, (struct mir_instr_elem_ptr *)instr);
		break;
	case MIR_INSTR_MEMBER_PTR:
		interp_instr_member_ptr(vm, (struct mir_instr_member_ptr *)instr);
		break;
	case MIR_INSTR_UNROLL:
		interp_instr_unroll(vm, (struct mir_instr_unroll *)instr);
		break;
	case MIR_INSTR_VARGS:
		interp_instr_vargs(vm, (struct mir_instr_vargs *)instr);
		break;
	case MIR_INSTR_COMPOUND: {
		struct mir_instr_compound *cmp = (struct mir_instr_compound *)instr;
		if (!cmp->is_naked) break;
		interp_instr_compound(vm, NULL, cmp);
		break;
	}
	case MIR_INSTR_TOANY:
		interp_instr_toany(vm, (struct mir_instr_to_any *)instr);
		break;
	case MIR_INSTR_SWITCH:
		interp_instr_switch(vm, (struct mir_instr_switch *)instr);
		break;

	default:
		babort("missing execution for instruction: %s", mir_instr_name(instr));
	}
	return vm->aborted ? VM_INTERP_ABORT : state;
}

void interp_instr_toany(struct virtual_machine *vm, struct mir_instr_to_any *toany) {
	struct mir_var *dest_var  = toany->tmp;
	struct mir_var *type_info = mir_get_rtti(vm->assembly, toany->rtti_type->id.hash);
	bassert(type_info->value.is_comptime);

	struct mir_type *dest_type = dest_var->value.type;
	vm_stack_ptr_t   dest      = vm_read_var(vm, dest_var);

	// type info
	struct mir_type *dest_type_info_type = mir_get_struct_elem_type(dest_type, 0);
	vm_stack_ptr_t   dest_type_info =
	    vm_get_struct_elem_ptr(vm->assembly, dest_var->value.type, dest, 0);

	vm_write_ptr(dest_type_info_type, dest_type_info, vm_read_var(vm, type_info));

	// data
	struct mir_type *dest_data_type = mir_get_struct_elem_type(dest_type, 1);
	vm_stack_ptr_t   dest_data      = vm_get_struct_elem_ptr(vm->assembly, dest_var->value.type, dest, 1);

	struct mir_type *data_type = toany->expr->value.type;

	if (toany->expr_tmp) {
		vm_stack_ptr_t  data      = fetch_value(vm, &toany->expr->value);
		struct mir_var *expr_var  = toany->expr_tmp;
		vm_stack_ptr_t  dest_expr = vm_read_var(vm, expr_var);

		// copy value to the tmp variable
		memcpy(dest_expr, data, data_type->store_size_bytes);

		// setup destination pointer
		memcpy(dest_data, &dest_expr, dest_data_type->store_size_bytes);
	} else if (toany->rtti_data) {
		struct mir_var *rtti_data_var = mir_get_rtti(vm->assembly, toany->rtti_data->id.hash);
		vm_stack_ptr_t  rtti_data     = vm_read_var(vm, rtti_data_var);
		// setup destination pointer
		memcpy(dest_data, &rtti_data, dest_data_type->store_size_bytes);
	} else {
		vm_stack_ptr_t data = fetch_value(vm, &toany->expr->value);
		bassert(mir_is_pointer_type(dest_data_type));
		memcpy(dest_data, data, dest_data_type->store_size_bytes);
	}

	stack_push(vm, &dest, toany->base.value.type);
}

void interp_instr_phi(struct virtual_machine *vm, struct mir_instr_phi *phi) {
	struct mir_instr_block *prev_block = vm->stack->prev_block;
	bassert(prev_block && "Invalid previous block for phi instruction.");
	struct mir_instr *value = NULL;
	for (usize i = 0; i < phi->num; ++i) {
		value = phi->incoming_values[i];
		bassert(value);
		struct mir_instr_block *block = phi->incoming_blocks[i];
		bassert(block);
		if (block->base.id == prev_block->base.id) break;
	}
	bassert(value && "Invalid value for phi income.");
	// Pop used value from stack or use constant. Result will be pushed on the
	// stack or used as constant value of phi when phi is compile time known
	// constant.
	{
		struct mir_type *phi_type = phi->base.value.type;
		bassert(phi_type);
		vm_stack_ptr_t value_ptr = fetch_value(vm, &value->value);
		stack_push(vm, value_ptr, phi_type);
	}
}

void interp_instr_addrof(struct virtual_machine *vm, struct mir_instr_addrof *addrof) {
	struct mir_instr *src  = addrof->src;
	struct mir_type  *type = src->value.type;
	bassert(type);
	if (!mir_is_comptime(src) &&
	    (src->kind == MIR_INSTR_ELEM_PTR || src->kind == MIR_INSTR_COMPOUND)) {
		// address of the element is already on the stack
		bassert(stack_peek(vm, type) != NULL);
		return;
	}
	vm_stack_ptr_t ptr = fetch_value(vm, &src->value);
	ptr                = VM_STACK_PTR_DEREF(ptr);
	stack_push(vm, (vm_stack_ptr_t)&ptr, type);
}

void interp_instr_elem_ptr(struct virtual_machine *vm, struct mir_instr_elem_ptr *elem_ptr) {
	// pop index from stack
	struct mir_type *arr_type   = mir_deref_type(elem_ptr->arr_ptr->value.type);
	vm_stack_ptr_t   index_ptr  = fetch_value(vm, &elem_ptr->index->value);
	vm_stack_ptr_t   arr_ptr    = fetch_value(vm, &elem_ptr->arr_ptr->value);
	vm_stack_ptr_t   result_ptr = NULL;
	bassert(arr_ptr && index_ptr);

	bassert(elem_ptr->index->value.type->store_size_bytes == sizeof(s64));
	const s64 index = vm_read_as(s64, index_ptr);
	arr_ptr         = VM_STACK_PTR_DEREF(arr_ptr);

	switch (arr_type->kind) {
	case MIR_TYPE_ARRAY: {
		const s64 len = arr_type->data.array.len;
		if (index >= len) {
			builder_error("Array index is out of the bounds! Array index "
			              "is: %lli, "
			              "but array size "
			              "is: %lli",
			              (long long)index,
			              (long long)len);
			vm_abort(vm);
		}

		result_ptr = vm_get_array_elem_ptr(arr_type, arr_ptr, (u32)index);
		break;
	}

	case MIR_TYPE_DYNARR:
	case MIR_TYPE_SLICE:
	case MIR_TYPE_STRING:
	case MIR_TYPE_VARGS: {
		struct mir_type *len_type = mir_get_struct_elem_type(arr_type, MIR_SLICE_LEN_INDEX);
		struct mir_type *ptr_type = mir_get_struct_elem_type(arr_type, MIR_SLICE_PTR_INDEX);

		struct mir_type *elem_type = mir_deref_type(ptr_type);
		bassert(elem_type);

		vm_stack_ptr_t len_ptr = vm_get_struct_elem_ptr(vm->assembly, arr_type, arr_ptr, 0);
		vm_stack_ptr_t ptr_ptr = vm_get_struct_elem_ptr(vm->assembly, arr_type, arr_ptr, 1);

		vm_stack_ptr_t ptr_tmp = vm_read_ptr(ptr_type, ptr_ptr);
		const s64      len_tmp = vm_read_int(len_type, len_ptr);

		if (!ptr_tmp) {
			builder_error("Dereferencing null pointer! Slice has not been set?");
			vm_abort(vm);
		}

		if (index >= len_tmp) {
			builder_error("Array index is out of the bounds! Array index is: %lli, but "
			              "array size is: %lli",
			              (long long)index,
			              (long long)len_tmp);
			vm_abort(vm);
		}

		result_ptr = (vm_stack_ptr_t)(ptr_tmp + index * elem_type->store_size_bytes);
		break;
	}

	default:
		babort("Invalid elem ptr target type!");
	}

	// push result address on the stack
	stack_push(vm, (vm_stack_ptr_t)&result_ptr, elem_ptr->base.value.type);
}

void interp_instr_member_ptr(struct virtual_machine *vm, struct mir_instr_member_ptr *member_ptr) {
	bassert(member_ptr->target_ptr);
	struct mir_type *target_type = member_ptr->target_ptr->value.type;

	// lookup for base structure declaration type
	// IDEA: maybe we can store parent type to the member type? But what about
	// builtin types???
	bassert(target_type->kind == MIR_TYPE_PTR && "expected pointer");
	target_type = mir_deref_type(target_type);
	bassert(mir_is_composite_type(target_type) && "expected structure");

	// fetch address of the struct begin
	vm_stack_ptr_t ptr = fetch_value(vm, &member_ptr->target_ptr->value);
	ptr                = VM_STACK_PTR_DEREF(ptr);
	bassert(ptr);

	vm_stack_ptr_t result = NULL;

	if (member_ptr->builtin_id == BUILTIN_ID_NONE) {
		bassert(member_ptr->scope_entry && member_ptr->scope_entry->kind == SCOPE_ENTRY_MEMBER);
		struct mir_member *member = member_ptr->scope_entry->data.member;
		bassert(member);
		const s64 index = member->index;

		// let the llvm solve poiner offset
		result = vm_get_struct_elem_ptr(vm->assembly, target_type, ptr, (u32)index);
	} else {
		// builtin member
		if (member_ptr->builtin_id == BUILTIN_ID_ARR_PTR) {
			// slice .ptr
			result = vm_get_struct_elem_ptr(vm->assembly, target_type, ptr, 1);
		} else if (member_ptr->builtin_id == BUILTIN_ID_ARR_LEN) {
			// slice .len
			result = vm_get_struct_elem_ptr(vm->assembly, target_type, ptr, 0);
		} else {
			babort("invalid slice member!");
		}
	}

	// push result address on the stack
	stack_push(vm, (vm_stack_ptr_t)&result, member_ptr->base.value.type);
}

void interp_instr_unroll(struct virtual_machine *vm, struct mir_instr_unroll *unroll) {
	bassert(unroll->src);
	struct mir_type *src_type = unroll->src->value.type;
	const s32        index    = unroll->index;

	bassert(index >= 0);
	bassert(src_type->kind == MIR_TYPE_PTR && "expected pointer");

	src_type = mir_deref_type(src_type);
	bassert(mir_is_composite_type(src_type) && "expected structure");

	vm_stack_ptr_t ptr = fetch_value(vm, &unroll->src->value);
	ptr                = VM_STACK_PTR_DEREF(ptr);
	bassert(ptr);

	vm_stack_ptr_t result = vm_get_struct_elem_ptr(vm->assembly, src_type, ptr, (u32)index);
	stack_push(vm, (vm_stack_ptr_t)&result, unroll->base.value.type);
}

void interp_instr_unreachable(struct virtual_machine *vm, struct mir_instr_unreachable *unr) {
	vmdbg_break();
	builder_msg(
	    MSG_ERR, ERR_COMPILE_TIME_ABORT, NULL, CARET_AFTER, "Execution reached unreachable code.");
	vm_abort(vm);
}

void interp_instr_debugbreak(struct virtual_machine     *vm,
                             struct mir_instr_debugbreak UNUSED(*debug_break)) {
	vmdbg_break();
}

void interp_instr_br(struct virtual_machine *vm, struct mir_instr_br *br) {
	bassert(br->then_block);
	vm->stack->prev_block = br->base.owner_block;
	set_pc(vm, br->then_block->entry_instr);
}

void interp_instr_switch(struct virtual_machine *vm, struct mir_instr_switch *sw) {
	struct mir_type *value_type = sw->value->value.type;
	vm_stack_ptr_t   value_ptr  = fetch_value(vm, &sw->value->value);
	bassert(value_ptr);

	const s64 value       = vm_read_int(value_type, value_ptr);
	vm->stack->prev_block = sw->base.owner_block;

	mir_switch_cases_t *cases = sw->cases;
	for (usize i = 0; i < sarrlenu(cases); ++i) {
		struct mir_switch_case *c        = &sarrpeek(cases, i);
		const s64               on_value = vm_read_int(value_type, c->on_value->value.data);
		if (value == on_value) {
			set_pc(vm, c->block->entry_instr);
			return;
		}
	}
	set_pc(vm, sw->default_block->entry_instr);
}

void interp_instr_cast(struct virtual_machine *vm, struct mir_instr_cast *cast) {
	if (cast->op == MIR_CAST_NONE) return;
	struct mir_type *dest_type = cast->base.value.type;
	struct mir_type *src_type  = cast->expr->value.type;
	vm_stack_ptr_t   src_ptr   = fetch_value(vm, &cast->expr->value);
	vm_value_t       tmp       = {0};
	vm_do_cast((vm_stack_ptr_t)&tmp, src_ptr, dest_type, src_type, cast->op);
	stack_push(vm, &tmp, dest_type);
}

void interp_instr_arg(struct virtual_machine *vm, struct mir_instr_arg *arg) {
	// Caller is optional, when we call function implicitly there is no call instruction which
	// we can use, so we need to handle also this situation. In such case we expect all
	// arguments to be already pushed on the stack.
	struct mir_instr_call *caller = (struct mir_instr_call *)get_ra(vm)->caller;

	// Call arguments are in reverse order on the stack.

	if (caller) {
		mir_instrs_t *arg_values = caller->args;
		bassert(arg_values && arg->i < sarrlenu(arg_values));
		struct mir_instr *curr_arg_value = sarrpeek(arg_values, arg->i);

		if (mir_is_comptime(curr_arg_value)) {
			struct mir_type *type = curr_arg_value->value.type;
			stack_push(vm, curr_arg_value->value.data, type);
		} else {
			struct mir_instr *arg_value = NULL;
			// starting point
			vm_stack_ptr_t arg_ptr = (vm_stack_ptr_t)vm->stack->ra;
			for (u32 i = 0; i <= arg->i; ++i) {
				arg_value = sarrpeek(arg_values, i);
				bassert(arg_value);
				if (mir_is_comptime(arg_value)) continue;
				arg_ptr -= stack_alloc_size(arg_value->value.type->store_size_bytes);
			}

			stack_push(vm, (vm_stack_ptr_t)arg_ptr, arg->base.value.type);
		}

		return;
	}

	// Caller instruction not specified!!!
	struct mir_fn *fn = arg->base.owner_block->owner_fn;
	bassert(fn && "Arg instruction cannot determinate current function");

	// All arguments must be already on the stack in reverse order.
	mir_args_t *args = fn->type->data.fn.args;
	bassert(args && "Function has no arguments");

	// starting point
	vm_stack_ptr_t arg_ptr = (vm_stack_ptr_t)vm->stack->ra;
	for (u32 i = 0; i <= arg->i; ++i) {
		arg_ptr -= stack_alloc_size(sarrpeek(args, i)->type->store_size_bytes);
	}

	stack_push(vm, (vm_stack_ptr_t)arg_ptr, arg->base.value.type);
}

void interp_instr_cond_br(struct virtual_machine *vm, struct mir_instr_cond_br *br) {
	bassert(br->cond);
	struct mir_type *type = br->cond->value.type;
	// pop condition from stack
	vm_stack_ptr_t cond_ptr = NULL;
	if (br->keep_stack_value) {
		cond_ptr = peek_value(vm, &br->cond->value);
	} else {
		cond_ptr = fetch_value(vm, &br->cond->value);
	}
	bassert(cond_ptr);
	const bool condition = vm_read_int(type, cond_ptr);
	// Set previous block.
	vm->stack->prev_block = br->base.owner_block;
	if (condition) {
		set_pc(vm, br->then_block->entry_instr);
	} else {
		set_pc(vm, br->else_block->entry_instr);
	}
}

void interp_instr_decl_ref(struct virtual_machine *vm, struct mir_instr_decl_ref *ref) {
	struct scope_entry *entry = ref->scope_entry;
	bassert(entry);

	switch (entry->kind) {
	case SCOPE_ENTRY_VAR: {
		struct mir_var *var = entry->data.var;
		bassert(var);

		vm_stack_ptr_t real_ptr = vm_read_var(vm, var);
		stack_push(vm, &real_ptr, ref->base.value.type);
		break;
	}

	case SCOPE_ENTRY_FN:
	case SCOPE_ENTRY_TYPE:
	case SCOPE_ENTRY_MEMBER:
	case SCOPE_ENTRY_VARIANT:
		break;

	case SCOPE_ENTRY_ARG: {
		struct mir_arg *arg = entry->data.arg;
		bassert(arg);
		// Caller is optional, when we call function implicitly there is no call instruction which
		// we can use, so we need to handle also this situation. In such case we expect all
		// arguments to be already pushed on the stack.
		struct mir_instr_call *caller = (struct mir_instr_call *)get_ra(vm)->caller;

		if (caller) {
			mir_instrs_t     *arg_values     = caller->args;
			struct mir_instr *curr_arg_value = sarrpeekor(arg_values, arg->index, NULL);
			bassert(curr_arg_value);

			if (mir_is_comptime(curr_arg_value)) {
				struct mir_type *type = curr_arg_value->value.type;
				stack_push(vm, curr_arg_value->value.data, type);
			} else {
				// Arguments are located in reverse order right before return address on the
				// stack so we can find them inside loop adjusting address up on the stack.
				struct mir_instr *arg_value = NULL;
				// starting point
				vm_stack_ptr_t arg_ptr = (vm_stack_ptr_t)vm->stack->ra;
				for (u32 i = 0; i <= arg->index; ++i) {
					arg_value = sarrpeek(arg_values, i);
					bassert(arg_value);
					if (mir_is_comptime(arg_value)) continue;
					arg_ptr -= stack_alloc_size(arg_value->value.type->store_size_bytes);
				}

				stack_push(vm, (vm_stack_ptr_t)arg_ptr, arg->type);
			}

			return;
		}

		// Caller instruction not specified!!!
		struct mir_fn *fn = ref->base.owner_block->owner_fn;
		bassert(fn && "Argument instruction cannot determinate current function");

		// All arguments must be already on the stack in reverse order.
		mir_args_t *args = fn->type->data.fn.args;
		bassert(args && "Function has no arguments");

		// starting point
		vm_stack_ptr_t arg_ptr = (vm_stack_ptr_t)vm->stack->ra;
		for (u32 i = 0; i <= arg->index; ++i) {
			arg_ptr -= stack_alloc_size(sarrpeek(args, i)->type->store_size_bytes);
		}

		stack_push(vm, (vm_stack_ptr_t)arg_ptr, arg->type);
		break;
	}

	default:
		babort("invalid declaration reference");
	}
}

void interp_instr_decl_direct_ref(struct virtual_machine *vm, struct mir_instr_decl_direct_ref *ref) {
	bassert(ref->ref->kind == MIR_INSTR_DECL_VAR);
	struct mir_var *var = ((struct mir_instr_decl_var *)ref->ref)->var;
	bassert(var);

	vm_stack_ptr_t real_ptr = vm_read_var(vm, var);
	stack_push(vm, &real_ptr, ref->base.value.type);
}

void interp_instr_compound(struct virtual_machine    *vm,
                           vm_stack_ptr_t             tmp_ptr,
                           struct mir_instr_compound *cmp) {
	bassert(!mir_is_comptime(&cmp->base));
	const bool will_push = tmp_ptr == NULL;
	if (will_push) {
		bassert(cmp->tmp_var && "Missing temp variable for compound.");
		tmp_ptr = vm_read_var(vm, cmp->tmp_var);
	}

	bassert(tmp_ptr);

	struct mir_type *type = cmp->base.value.type;
	struct mir_type *elem_type;
	vm_stack_ptr_t   elem_ptr = tmp_ptr;

	ints_t       *mapping = cmp->value_member_mapping;
	mir_instrs_t *values  = cmp->values;
	for (usize i = 0; i < sarrlenu(values); ++i) {
		struct mir_instr *value = sarrpeek(values, i);
		elem_type               = value->value.type;
		switch (type->kind) {

		case MIR_TYPE_STRING:
		case MIR_TYPE_DYNARR:
		case MIR_TYPE_SLICE:
		case MIR_TYPE_VARGS:
		case MIR_TYPE_STRUCT: {
			usize index = mapping ? sarrpeek(mapping, i) : i;
			elem_ptr    = vm_get_struct_elem_ptr(vm->assembly, type, tmp_ptr, (u32)index);
			break;
		}

		case MIR_TYPE_ARRAY:
			elem_ptr = vm_get_array_elem_ptr(type, tmp_ptr, (u32)i);
			break;

		default:
			bassert(i == 0 && "Invalid elem count for non-agregate type!!!");
		}

		vm_stack_ptr_t value_ptr = fetch_value(vm, &value->value);
		memcpy(elem_ptr, value_ptr, elem_type->store_size_bytes);
	}

	if (will_push) stack_push(vm, tmp_ptr, cmp->base.value.type);
}

void interp_instr_vargs(struct virtual_machine *vm, struct mir_instr_vargs *vargs) {
	mir_instrs_t   *values    = vargs->values;
	struct mir_var *arr_tmp   = vargs->arr_tmp;
	struct mir_var *vargs_tmp = vargs->vargs_tmp;

	bassert(vargs_tmp->value.type->kind == MIR_TYPE_VARGS);
	bassert(vargs_tmp->vm_ptr.global && "Unalocated vargs slice!!!");
	bassert(values);

	vm_stack_ptr_t arr_tmp_ptr = arr_tmp ? vm_read_var(vm, arr_tmp) : NULL;

	// Fill vargs tmp array with values from stack or constants.
	for (usize i = 0; i < sarrlenu(values); ++i) {
		struct mir_instr *value = sarrpeek(values, i);
		bassert(arr_tmp_ptr);
		const usize    value_size = value->value.type->store_size_bytes;
		vm_stack_ptr_t dest       = arr_tmp_ptr + i * value_size;

		vm_stack_ptr_t value_ptr = fetch_value(vm, &value->value);
		if (!dest) babort("Bad memory.");
		memcpy(dest, value_ptr, value_size);
	}

	// Push vargs slice on the stack.
	{
		vm_stack_ptr_t vargs_tmp_ptr = vm_read_var(vm, vargs_tmp);
		// set len
		vm_stack_ptr_t len_ptr =
		    vargs_tmp_ptr +
		    vm_get_struct_elem_offset(vm->assembly, vargs_tmp->value.type, MIR_SLICE_LEN_INDEX);

		bassert(mir_get_struct_elem_type(vargs_tmp->value.type, MIR_SLICE_LEN_INDEX)
		            ->store_size_bytes == sizeof(s64));
		vm_write_as(s64, len_ptr, sarrlen(values));

		// set ptr
		vm_stack_ptr_t ptr_ptr =
		    vargs_tmp_ptr +
		    vm_get_struct_elem_offset(vm->assembly, vargs_tmp->value.type, MIR_SLICE_PTR_INDEX);
		vm_write_as(vm_stack_ptr_t, ptr_ptr, arr_tmp_ptr);
		stack_push(vm, vargs_tmp_ptr, vargs_tmp->value.type);
	}
}

void interp_instr_decl_var(struct virtual_machine *vm, struct mir_instr_decl_var *decl) {
	struct mir_var *var = decl->var;
	bassert(var);
	bassert(decl->base.value.type);
	if (isflag(var->iflags, MIR_VAR_GLOBAL) || var->value.is_comptime || var->ref_count == 0)
		return;
	// initialize variable if there is some init value
	if (decl->init) {
		vm_stack_ptr_t var_ptr = vm_read_var(vm, var);

		if (!mir_is_comptime(decl->init) && decl->init->kind == MIR_INSTR_COMPOUND) {
			// used compound initialization!!!
			interp_instr_compound(vm, var_ptr, (struct mir_instr_compound *)decl->init);
		} else {
			// read initialization value if there is one
			vm_stack_ptr_t init_ptr = fetch_value(vm, &decl->init->value);
			memcpy(var_ptr, init_ptr, var->value.type->store_size_bytes);
		}
	}
}

void interp_instr_load(struct virtual_machine *vm, struct mir_instr_load *load) {
	// pop source from stack or load directly when src is declaration, push on
	// to stack dereferenced value of source
	struct mir_type *dest_type = load->base.value.type;
	bassert(dest_type);
	bassert(mir_is_pointer_type(load->src->value.type));
	vm_stack_ptr_t src_ptr = fetch_value(vm, &load->src->value);
	src_ptr                = VM_STACK_PTR_DEREF(src_ptr);
	stack_push(vm, src_ptr, dest_type);
}

void interp_instr_store(struct virtual_machine *vm, struct mir_instr_store *store) {
	// loads destination (in case it is not direct reference to declaration) and
	// source from stack
	vm_stack_ptr_t dest_ptr = fetch_value(vm, &store->dest->value);
	if (store->src->kind == MIR_INSTR_COMPOUND && !mir_is_comptime(store->src)) {
		// Compound initializers referenced by store instruction can be directly used as
		// destination initializer.
		dest_ptr = VM_STACK_PTR_DEREF(dest_ptr);
		interp_instr_compound(vm, dest_ptr, (struct mir_instr_compound *)store->src);
		return;
	}
	struct mir_type *src_type = store->src->value.type;
	bassert(src_type);
	dest_ptr = VM_STACK_PTR_DEREF(dest_ptr);
	if (!dest_ptr) {
		builder_error("Dereferencing null pointer!");
		vm_abort(vm);
		return;
	}

	vm_stack_ptr_t const src_ptr = fetch_value(vm, &store->src->value);
	bassert(dest_ptr && src_ptr);
	memcpy(dest_ptr, src_ptr, src_type->store_size_bytes);
}

enum vm_interp_state interp_instr_call(struct virtual_machine *vm, struct mir_instr_call *call) {
	// Call instruction expects all arguments already pushed on the stack in reverse order.
	bassert(call->callee && call->base.value.type);
	bassert(call->callee->value.type);

	vm_stack_ptr_t   callee_ptr      = fetch_value(vm, &call->callee->value);
	struct mir_type *callee_ptr_type = call->callee->value.type;
	struct mir_type *callee_type     = NULL;

	// Function called via pointer.
	if (mir_is_pointer_type(call->callee->value.type)) {
		callee_type = mir_deref_type(call->callee->value.type);
	} else {
		callee_type = call->callee->value.type;
	}
	bassert(callee_type && callee_type->kind == MIR_TYPE_FN);

	struct mir_fn *fn = (struct mir_fn *)vm_read_ptr(callee_ptr_type, callee_ptr);
	if (!fn) {
		builder_error("Function pointer not set!");
		vm_abort(vm);
		return VM_INTERP_ABORT;
	}
	bmagic_assert(fn);

	if (!fn->is_fully_analyzed) {
		// In case the fetched callee value is coming from the stack, we have to push it back to the
		// stack to get proper address on next try!
		if (!call->callee->value.is_comptime) {
			stack_push(vm, callee_ptr, callee_ptr_type);
		}
		return VM_INTERP_POSTPONE;
	}
	if (isflag(fn->flags, FLAG_EXTERN) || isflag(fn->flags, FLAG_INTRINSIC)) {
		interp_extern_call(vm, call, fn->linkage_name, callee_type, fn->dyncall.extern_entry);
	} else {
		// Push current frame stack top. (Later popped by ret instruction)
		push_ra(vm, call);
		bassert(fn->first_block->entry_instr);
		stack_alloc_local_vars(vm, fn);
		// setup entry instruction
		set_pc(vm, fn->first_block->entry_instr);
	}
	return VM_INTERP_PASSED;
}

void interp_instr_ret(struct virtual_machine *vm, struct mir_instr_ret *ret) {
	struct mir_fn *fn = ret->base.owner_block->owner_fn;
	bassert(fn);
	struct mir_type *ret_type     = fn->type->data.fn.ret_type;
	vm_stack_ptr_t   ret_data_ptr = NULL;

	// pop return value from stack
	if (ret->value) {
		bassert(mir_type_cmp(ret_type, ret->value->value.type));
		bassert(ret_type->kind != MIR_TYPE_VOID && "Void return cannot have specified value.");
		ret_data_ptr = fetch_value(vm, &ret->value->value);
		bassert(ret_data_ptr);
#ifdef BL_DEBUG
	} else {
		bassert(ret_type->kind == MIR_TYPE_VOID);
#endif
	}

	// do frame stack rollback
	struct mir_instr_call *pc = pop_ra(vm);
	// post-processing like cleanup and continuing to the another instruction is allowed only in
	// case the return address call is not compile-time; i.e. type resolver or any #comptime marked
	// top-level executed during evaluation.
	const bool do_post_process = pc && !mir_is_comptime(&pc->base);

	// clean up all arguments from the stack only for non-comptime return address call (in case we
	// have compile-time call here, we suppose it's top-level executed call!
	if (do_post_process) {
		mir_instrs_t *arg_values = pc->args;
		for (usize i = 0; i < sarrlenu(arg_values); ++i) {
			struct mir_instr *arg_value = sarrpeek(arg_values, i);
			if (mir_is_comptime(arg_value)) continue;
			stack_pop(vm, arg_value->value.type);
		}
	}

	// push return value on the stack if there is one
	if (ret_data_ptr) {
		if (do_post_process) {
			if (mir_is_comptime(&pc->base)) {
				pc->base.value.data = ret->value->value.data;
			} else if (pc->base.ref_count > 0) {
				stack_push(vm, ret_data_ptr, ret_type);
			}
		} else {
			stack_push(vm, ret_data_ptr, ret_type);
		}
	}

	// set program counter to next instruction
	set_pc(vm, do_post_process ? pc->base.next : NULL);
}

void interp_instr_binop(struct virtual_machine *vm, struct mir_instr_binop *binop) {
	// binop expects lhs and rhs on stack in exact order and push result again
	// to the stack
	vm_stack_ptr_t lhs_ptr = fetch_value(vm, &binop->lhs->value);
	vm_stack_ptr_t rhs_ptr = fetch_value(vm, &binop->rhs->value);
	bassert(rhs_ptr && lhs_ptr);

	struct mir_type *dest_type = binop->base.value.type;
	struct mir_type *src_type  = binop->lhs->value.type;

	if (binop->op == BINOP_DIV && src_type->kind != MIR_TYPE_REAL) {
		const u64 n = vm_read_int(src_type, rhs_ptr);
		if (n == 0) {
			builder_msg(MSG_ERR,
			            ERR_DIV_BY_ZERO,
			            binop->rhs->node->location,
			            CARET_WORD,
			            "Division by zero.");
			eval_abort(vm);
			return;
		}
	}

	vm_value_t tmp = {0};
	calculate_binop(src_type, (vm_stack_ptr_t)&tmp, lhs_ptr, rhs_ptr, binop->op);

	stack_push(vm, &tmp, dest_type);
}

void interp_instr_unop(struct virtual_machine *vm, struct mir_instr_unop *unop) {
	struct mir_type *type  = unop->base.value.type;
	vm_stack_ptr_t   v_ptr = fetch_value(vm, &unop->expr->value);

	vm_value_t tmp = {0};
	calculate_unop((vm_stack_ptr_t)&tmp, v_ptr, unop->op, type);

	stack_push(vm, &tmp, type);
}

void eval_instr(struct virtual_machine *vm, struct mir_instr *instr) {
	if (!instr) return;
	bassert(instr->value.is_comptime);

	switch (instr->kind) {
	case MIR_INSTR_DECL_REF:
		eval_instr_decl_ref(vm, (struct mir_instr_decl_ref *)instr);
		break;

	case MIR_INSTR_DECL_DIRECT_REF:
		eval_instr_decl_direct_ref(vm, (struct mir_instr_decl_direct_ref *)instr);
		break;

	case MIR_INSTR_BINOP:
		eval_instr_binop(vm, (struct mir_instr_binop *)instr);
		break;

	case MIR_INSTR_UNOP:
		eval_instr_unop(vm, (struct mir_instr_unop *)instr);
		break;

	case MIR_INSTR_SET_INITIALIZER:
		eval_instr_set_initializer(vm, (struct mir_instr_set_initializer *)instr);
		break;

	case MIR_INSTR_LOAD:
		eval_instr_load(vm, (struct mir_instr_load *)instr);
		break;

	case MIR_INSTR_ADDROF:
		eval_instr_addrof(vm, (struct mir_instr_addrof *)instr);
		break;

	case MIR_INSTR_CAST:
		eval_instr_cast(vm, (struct mir_instr_cast *)instr);
		break;

	case MIR_INSTR_DECL_VAR:
		eval_instr_decl_var(vm, (struct mir_instr_decl_var *)instr);
		break;

	case MIR_INSTR_COMPOUND:
		eval_instr_compound(vm, (struct mir_instr_compound *)instr);
		break;

	case MIR_INSTR_ELEM_PTR:
		eval_instr_elem_ptr(vm, (struct mir_instr_elem_ptr *)instr);
		break;

	case MIR_INSTR_MEMBER_PTR:
		eval_instr_member_ptr(vm, (struct mir_instr_member_ptr *)instr);
		break;

	case MIR_INSTR_TYPE_INFO:
		eval_instr_type_info(vm, (struct mir_instr_type_info *)instr);
		break;

	case MIR_INSTR_TYPEOF:
		eval_instr_typeof(vm, (struct mir_instr_typeof *)instr);
		break;

	case MIR_INSTR_TEST_CASES:
		eval_instr_test_cases(vm, (struct mir_instr_test_case *)instr);
		break;

	case MIR_INSTR_CALL_LOC:
		eval_instr_call_loc(vm, (struct mir_instr_call_loc *)instr);
		break;

	case MIR_INSTR_UNROLL:
		eval_instr_unroll(vm, (struct mir_instr_unroll *)instr);
		break;

	case MIR_INSTR_ARG:
		eval_instr_arg(vm, (struct mir_instr_arg *)instr);
		break;

	case MIR_INSTR_PHI:
	case MIR_INSTR_COND_BR:
	case MIR_INSTR_CALL:
	case MIR_INSTR_BLOCK:
	case MIR_INSTR_CONST:
	case MIR_INSTR_FN_PROTO:
	case MIR_INSTR_FN_GROUP:
	case MIR_INSTR_TYPE_ARRAY:
	case MIR_INSTR_TYPE_PTR:
	case MIR_INSTR_TYPE_FN:
	case MIR_INSTR_TYPE_FN_GROUP:
	case MIR_INSTR_TYPE_STRUCT:
	case MIR_INSTR_TYPE_ENUM:
	case MIR_INSTR_TYPE_SLICE:
	case MIR_INSTR_TYPE_DYNARR:
	case MIR_INSTR_TYPE_VARGS:
	case MIR_INSTR_TYPE_POLY:
	case MIR_INSTR_DECL_MEMBER:
	case MIR_INSTR_DECL_ARG:
	case MIR_INSTR_DECL_VARIANT:
	case MIR_INSTR_SIZEOF:
	case MIR_INSTR_ALIGNOF:
	case MIR_INSTR_BR:
	case MIR_INSTR_USING:
	case MIR_INSTR_DESIGNATOR:
		break;

	default:
		babort("Missing evaluation for instruction '%s'.", mir_instr_name(instr));
	}
}

void eval_instr_type_info(struct virtual_machine *vm, struct mir_instr_type_info *type_info) {
	bassert(type_info->rtti_type && "Missing RTTI type!");
	struct mir_var *rtti_var = mir_get_rtti(vm->assembly, type_info->rtti_type->id.hash);
#if defined(BL_DEBUG)
	if (!rtti_var) {
		const str_t  name = type_info->rtti_type->id.str;
		const hash_t hash = type_info->rtti_type->id.hash;
		blog("Rtti type " STR_FMT " [%lu] not found!", STR_ARG(name), hash);
		bassert(rtti_var);
	}
#endif

	MIR_CEV_WRITE_AS(vm_stack_ptr_t, &type_info->base.value, rtti_var->value.data);
}

void eval_instr_typeof(struct virtual_machine *vm, struct mir_instr_typeof *type_of) {
	MIR_CEV_WRITE_AS(struct mir_type *, &type_of->base.value, type_of->expr->value.type);
}

void eval_instr_call_loc(struct virtual_machine UNUSED(*vm), struct mir_instr_call_loc *loc) {
	if (!loc->meta_var) return;
	MIR_CEV_WRITE_AS(vm_stack_ptr_t, &loc->base.value, loc->meta_var->value.data);
}

void eval_instr_test_cases(struct virtual_machine *vm, struct mir_instr_test_case *tc) {
	struct mir_var  *var     = vm->assembly->testing.meta_var;
	struct mir_type *tc_type = tc->base.value.type;

	struct mir_type *len_type = mir_get_struct_elem_type(tc_type, MIR_SLICE_LEN_INDEX);
	struct mir_type *ptr_type = mir_get_struct_elem_type(tc_type, MIR_SLICE_PTR_INDEX);

	vm_stack_ptr_t tc_ptr = tc->base.value.data;

	vm_stack_ptr_t len_ptr = vm_get_struct_elem_ptr(vm->assembly, tc_type, tc_ptr, 0);
	vm_stack_ptr_t ptr_ptr = vm_get_struct_elem_ptr(vm->assembly, tc_type, tc_ptr, 1);

	if (var) {
		bassert(var->value.type && var->value.type->kind == MIR_TYPE_ARRAY);
		const s64 len = var->value.type->data.array.len;
		vm_write_int(len_type, len_ptr, (u64)len);

		vm_stack_ptr_t meta_ptr = vm_read_var(vm, var);
		vm_write_ptr(ptr_type, ptr_ptr, meta_ptr);
	} else {
		vm_write_int(len_type, len_ptr, 0);
		vm_write_ptr(ptr_type, ptr_ptr, 0);
	}
}

void eval_instr_elem_ptr(struct virtual_machine *vm, struct mir_instr_elem_ptr *elem_ptr) {
	struct mir_type *arr_type   = mir_deref_type(elem_ptr->arr_ptr->value.type);
	vm_stack_ptr_t   arr_ptr    = MIR_CEV_READ_AS(vm_stack_ptr_t, &elem_ptr->arr_ptr->value);
	vm_stack_ptr_t   result_ptr = NULL;
	const s64        index      = MIR_CEV_READ_AS(s64, &elem_ptr->index->value);

	switch (arr_type->kind) {
	case MIR_TYPE_ARRAY: {
		result_ptr = vm_get_array_elem_ptr(arr_type, arr_ptr, (u32)index);
		break;
	}

	case MIR_TYPE_DYNARR:
	case MIR_TYPE_SLICE:
	case MIR_TYPE_STRING:
	case MIR_TYPE_VARGS: {
		struct mir_type *len_type = mir_get_struct_elem_type(arr_type, MIR_SLICE_LEN_INDEX);
		struct mir_type *ptr_type = mir_get_struct_elem_type(arr_type, MIR_SLICE_PTR_INDEX);
		bassert(mir_deref_type(ptr_type));
		vm_stack_ptr_t len_ptr = vm_get_struct_elem_ptr(vm->assembly, arr_type, arr_ptr, 0);
		vm_stack_ptr_t ptr_ptr = vm_get_struct_elem_ptr(vm->assembly, arr_type, arr_ptr, 1);
		vm_stack_ptr_t ptr_tmp = vm_read_ptr(ptr_type, ptr_ptr);
		const s64      len_tmp = (s64)vm_read_int(len_type, len_ptr);

		if (!ptr_tmp) {
			builder_msg(MSG_ERR,
			            ERR_JIT_RUN_FAILED,
			            elem_ptr->base.node->location,
			            CARET_WORD,
			            "Dereferencing null pointer! Slice has not been set?");

			eval_abort(vm);
		}

		if (index >= len_tmp) {
			builder_msg(MSG_ERR,
			            ERR_JIT_RUN_FAILED,
			            elem_ptr->base.node->location,
			            CARET_WORD,
			            "Array index is out of the bounds! Array index is: %lli, but "
			            "array size is: %lli",
			            (long long)index,
			            (long long)len_tmp);

			eval_abort(vm);
		}
		break;
	}

	default:
		babort("Invalid elem ptr target type!");
	}

	MIR_CEV_WRITE_AS(vm_stack_ptr_t, &elem_ptr->base.value, result_ptr);
}

void eval_instr_member_ptr(struct virtual_machine       UNUSED(*vm),
                           struct mir_instr_member_ptr *member_ptr) {
	switch (member_ptr->scope_entry->kind) {
	case SCOPE_ENTRY_UNNAMED:
		// @Hack: This is used as dummy scope entry for sub-type members. They are evaluated
		// directly inside the analyze pass, but it should be here.
		break;
	case SCOPE_ENTRY_MEMBER: {
		struct mir_member *member = member_ptr->scope_entry->data.member;
		vm_stack_ptr_t     strct_ptr;

		if (member_ptr->target_ptr->kind == MIR_INSTR_CONST) {
			strct_ptr = (vm_stack_ptr_t)&member_ptr->target_ptr->value;
		} else {
			strct_ptr = MIR_CEV_READ_AS(vm_stack_ptr_t, &member_ptr->target_ptr->value);
		}

		vm_stack_ptr_t result_ptr = NULL;
		result_ptr                = strct_ptr + member->offset_bytes;

		MIR_CEV_WRITE_AS(vm_stack_ptr_t, &member_ptr->base.value, result_ptr);
		break;
	}

	case SCOPE_ENTRY_VARIANT: {
		struct mir_variant *variant = member_ptr->scope_entry->data.variant;
		MIR_CEV_WRITE_AS(u64, &member_ptr->base.value, variant->value);
		break;
	}

	default:
		babort("Invalid scope entry for member_ptr instruction!");
	}
}

void eval_instr_compound(struct virtual_machine *vm, struct mir_instr_compound *cmp) {
	struct mir_const_expr_value *value = &cmp->base.value;
	if (needs_allocation(value)) {
		// Compound data doesn't fit into default static memory register, we need to
		// allocate temporary block on the stack.
		value->data = stack_push_empty(vm, value->type);
	}

	memset(value->data, 0, value->type->store_size_bytes);
	if (mir_is_zero_initialized(cmp)) return;

	usize   index   = 0;
	ints_t *mapping = cmp->value_member_mapping;
	for (usize i = 0; i < sarrlenu(cmp->values); ++i, ++index) {
		struct mir_instr *it = sarrpeek(cmp->values, i);
		bassert(mir_is_comptime(it) && "Expected compile time known value.");
		vm_stack_ptr_t dest_ptr = value->data;
		vm_stack_ptr_t src_ptr  = it->value.data;
		bassert(src_ptr && "Invalid compound element value!");

		if (mapping) index = sarrpeek(mapping, i);

		switch (value->type->kind) {
		case MIR_TYPE_ARRAY: {
			ptrdiff_t offset = vm_get_array_elem_offset(value->type, (u32)index);
			dest_ptr += offset;
			memcpy(dest_ptr, src_ptr, value->type->data.array.elem_type->store_size_bytes);
			break;
		}

		case MIR_TYPE_DYNARR:
		case MIR_TYPE_STRUCT:
		case MIR_TYPE_STRING:
		case MIR_TYPE_SLICE:
		case MIR_TYPE_VARGS: {
			struct mir_type *member_type = mir_get_struct_elem_type(value->type, index);
			ptrdiff_t        offset      = vm_get_struct_elem_offset(vm->assembly, value->type, (u32)index);
			dest_ptr += offset;
			memcpy(dest_ptr, src_ptr, member_type->store_size_bytes);
			break;
		}

		case MIR_TYPE_INT:
		case MIR_TYPE_REAL:
		case MIR_TYPE_BOOL:
		case MIR_TYPE_PTR:
		case MIR_TYPE_ENUM: {
			bassert(i == 0 && "Non-agregate type initialized with multiple values!");
			memcpy(dest_ptr, src_ptr, value->type->store_size_bytes);
			break;
		}

		default:
			babort("Invalid type of compound element!");
		}
	}
}

void eval_instr_unroll(struct virtual_machine *vm, struct mir_instr_unroll *unroll) {
	struct mir_instr *src = unroll->src;
	bassert(src);
	struct mir_type *src_type = src->value.type;
	bassert(src_type);

	if (mir_is_composite_type(src_type) && src_type->data.strct.is_multiple_return_type) {
		vm_stack_ptr_t member_ptr =
		    vm_get_struct_elem_ptr(vm->assembly, src_type, src->value.data, unroll->index);
		MIR_CEV_WRITE_AS(vm_stack_ptr_t, &unroll->base.value, member_ptr);
		return;
	}

	unroll->base.value.data = src->value.data;
}

void eval_instr_arg(struct virtual_machine UNUSED(*vm), struct mir_instr_arg *arg) {
	struct mir_fn *fn = arg->base.owner_block->owner_fn;
	bmagic_assert(fn);
	struct mir_arg *arg_data = mir_get_fn_arg(fn->type, arg->i);
	bassert(arg_data && isflag(arg_data->flags, FLAG_COMPTIME));

	struct mir_instr_call *call = arg_data->generation_call;
	bassert(call && "No compile-time known arguments provided to the function argument evaluator!");
	bassert(arg->i < sarrlenu(call->args) && arg->i >= 0 && "Argument index is out of the range!");
	arg->base.value.data = sarrpeek(call->args, arg->i)->value.data;
}

void eval_instr_decl_var(struct virtual_machine UNUSED(*vm), struct mir_instr_decl_var *decl_var) {
	bassert(decl_var->init && "Missing variable initializer!");
	struct mir_var *var = decl_var->var;
	var->value.data     = decl_var->init->value.data;
	bassert(var->value.data && "Invalid variable initializer!");
}

void eval_instr_cast(struct virtual_machine UNUSED(*vm), struct mir_instr_cast *cast) {
	struct mir_type *dest_type = cast->base.value.type;
	struct mir_type *src_type  = cast->expr->value.type;
	vm_stack_ptr_t   src       = cast->expr->value.data;
	vm_do_cast(cast->base.value.data, src, dest_type, src_type, cast->op);
}

void eval_instr_addrof(struct virtual_machine UNUSED(*vm), struct mir_instr_addrof *addrof) {
	addrof->base.value.data = addrof->src->value.data;
}

void eval_instr_load(struct virtual_machine *vm, struct mir_instr_load *load) {
	if (load->src->value.type->kind == MIR_TYPE_TYPE) {
		struct mir_type *src_type = MIR_CEV_READ_AS(struct mir_type *, &load->src->value);
		bmagic_assert(src_type);
		if (mir_is_pointer_type(src_type)) {
			MIR_CEV_WRITE_AS(struct mir_type *, &load->base.value, src_type->data.ptr.expr);
		} else if (src_type->kind == MIR_TYPE_POLY) {
			MIR_CEV_WRITE_AS(struct mir_type *, &load->base.value, src_type);
		} else {
			assert(false && "Invalid type!");
		}
	} else if (load->src->value.type->kind == MIR_TYPE_POLY) {
		struct mir_type *src_type = MIR_CEV_READ_AS(struct mir_type *, &load->src->value);
		bmagic_assert(src_type);
		MIR_CEV_WRITE_AS(struct mir_type *, &load->base.value, src_type);
	} else {
		const vm_stack_ptr_t src = MIR_CEV_READ_AS(vm_stack_ptr_t, &load->src->value);
		if (!src) {
			builder_msg(MSG_ERR,
			            ERR_NULL_POINTER,
			            load->base.node ? load->base.node->location : NULL,
			            CARET_WORD,
			            "Dereferencing null pointer!");
			eval_abort(vm);
		}
		load->base.value.data = src;
	}
}

void eval_instr_set_initializer(struct virtual_machine *vm, struct mir_instr_set_initializer *si) {
	struct mir_instr *dest = si->dest;
	struct mir_var   *var  = ((struct mir_instr_decl_var *)dest)->var;
	bassert(
	    (isflag(var->iflags, MIR_VAR_GLOBAL) || isflag(var->iflags, MIR_VAR_STRUCT_TYPEDEF)) &&
	    "Only globals can be initialized by initializer!");
	if (var->value.is_comptime) {
		// This is little optimization, we can simply reuse initializer pointer
		// since we are dealing with constant values and variable is immutable
		// comptime.
		var->value.data = si->src->value.data;
	} else {
		const struct mir_type *var_type = var->value.type;
		// Globals always use static segment allocation!!!
		const vm_stack_ptr_t var_ptr = vm_read_var(vm, var);
		// Runtime variable needs it's own memory location so we must create copy of
		// initializer data
		memcpy(var_ptr, si->src->value.data, var_type->store_size_bytes);
	}
}

void eval_instr_unop(struct virtual_machine UNUSED(*vm), struct mir_instr_unop *unop) {
	struct mir_type *type = unop->base.value.type;

	vm_stack_ptr_t v_data    = unop->expr->value.data;
	vm_stack_ptr_t dest_data = unop->base.value.data;

	calculate_unop(dest_data, v_data, unop->op, type);
}

void eval_instr_binop(struct virtual_machine UNUSED(*vm), struct mir_instr_binop *binop) {
	bassert(binop->lhs->value.is_comptime && binop->rhs->value.is_comptime);

	vm_stack_ptr_t lhs_ptr  = binop->lhs->value.data;
	vm_stack_ptr_t rhs_ptr  = binop->rhs->value.data;
	vm_stack_ptr_t dest_ptr = binop->base.value.data;

	struct mir_type *src_type = binop->lhs->value.type;

	if (binop->op == BINOP_DIV && src_type->kind != MIR_TYPE_REAL) {
		const u64 n = vm_read_int(src_type, rhs_ptr);
		if (n == 0) {
			builder_msg(MSG_ERR,
			            ERR_DIV_BY_ZERO,
			            binop->rhs->node->location,
			            CARET_WORD,
			            "Division by zero.");
			eval_abort(vm);
			return;
		}
	}

	calculate_binop(src_type, dest_ptr, lhs_ptr, rhs_ptr, binop->op);
}

void eval_instr_decl_ref(struct virtual_machine UNUSED(*vm), struct mir_instr_decl_ref *decl_ref) {
	struct scope_entry *entry = decl_ref->scope_entry;
	bassert(entry);

	switch (entry->kind) {
	case SCOPE_ENTRY_FN:
		MIR_CEV_WRITE_AS(struct mir_fn *, &decl_ref->base.value, entry->data.fn);
		break;

	case SCOPE_ENTRY_TYPE:
		MIR_CEV_WRITE_AS(struct mir_type *, &decl_ref->base.value, entry->data.type);
		break;

	case SCOPE_ENTRY_VAR:
		MIR_CEV_WRITE_AS(vm_stack_ptr_t, &decl_ref->base.value, entry->data.var->value.data);
		break;

	case SCOPE_ENTRY_VARIANT:
		MIR_CEV_WRITE_AS(u64 *, &decl_ref->base.value, &entry->data.variant->value);
		break;

	case SCOPE_ENTRY_NAMED_SCOPE:
		MIR_CEV_WRITE_AS(struct scope_entry *, &decl_ref->base.value, entry);
		break;

	case SCOPE_ENTRY_ARG: {
		struct mir_arg *arg = entry->data.arg;
		bassert(arg);

		if (!arg->generation_call) break;
		struct mir_instr *comptime_value = sarrpeekor(arg->generation_call->args, arg->index, NULL);
		bassert(comptime_value);
		decl_ref->base.value.data = comptime_value->value.data;
		break;
	}

	default:
		BL_UNIMPLEMENTED;
	}
}

void eval_instr_decl_direct_ref(struct virtual_machine            UNUSED(*vm),
                                struct mir_instr_decl_direct_ref *decl_ref) {
	struct mir_var *var = ((struct mir_instr_decl_var *)decl_ref->ref)->var;
	MIR_CEV_WRITE_AS(vm_stack_ptr_t, &decl_ref->base.value, var->value.data);
}

// =================================================================================================
// Public
// =================================================================================================
void vm_init(struct virtual_machine *vm, usize stack_size) {
	vm->main_stack = create_stack(stack_size);
	swap_current_stack(vm, vm->main_stack);
	mtx_init(&vm->lock, mtx_recursive); // recursive here, we might nest some locking calls...
}

void vm_terminate(struct virtual_machine *vm) {
	data_free(vm);
	mtx_destroy(&vm->lock);
	arrfree(vm->dcsigtmp);
	for (u64 i = 0; i < arrlenu(vm->available_comptime_call_stacks); ++i) {
		terminate_stack(vm->available_comptime_call_stacks[i]);
	}
	arrfree(vm->available_comptime_call_stacks);
	for (u32 i = 0; i < tbl_len(vm->comptime_call_stacks); ++i) {
		terminate_stack(vm->comptime_call_stacks[i].stack);
	}
	tbl_free(vm->comptime_call_stacks);
	terminate_stack(vm->main_stack);
}

void vm_print_backtrace(struct virtual_machine *vm) {
	mtx_lock(&vm->lock);

	builder_note("\nBacktrace:");
	struct mir_instr *instr = vm->stack->pc;
	struct vm_frame  *fr    = vm->stack->ra;
	usize             n     = 0;
	if (!instr) {
		mtx_unlock(&vm->lock);
		return;
	}

	// Print the last instruction
	builder_msg(MSG_ERR_NOTE, 0, instr->node->location, CARET_NONE, "Last called:");
	while (fr) {
		instr = &fr->caller->base;
		fr    = fr->prev;
		if (!instr) break;
		if (n == 100) {
			builder_log("continue...");
			break;
		}
		struct mir_fn *fn = instr->owner_block->owner_fn;
		if (fn && fn->generated.debug_replacement_types.len) {
			const str_t replacement = fn->generated.debug_replacement_types;
			builder_msg(MSG_ERR_NOTE,
			            0,
			            instr->node->location,
			            CARET_NONE,
			            "Called from following location with polymorph replacement: " STR_FMT "",
			            STR_ARG(replacement));
		} else {
			builder_msg(MSG_ERR_NOTE, 0, instr->node->location, CARET_NONE, "Called from:");
		}
		++n;
	}
	mtx_unlock(&vm->lock);
}

void vm_abort(struct virtual_machine *vm) {
	mtx_lock(&vm->lock);
	vm_print_backtrace(vm);
	vm->aborted = true;
	mtx_unlock(&vm->lock);
}

bool vm_eval_instr(struct virtual_machine *vm, struct assembly *assembly, struct mir_instr *instr) {
	zone();
	mtx_lock(&vm->lock);
	vm->aborted  = false;
	vm->assembly = assembly;
	eval_instr(vm, instr);
	mtx_unlock(&vm->lock);
	return_zone(!vm->aborted);
}

void vm_provide_command_line_arguments(struct virtual_machine *vm, const s32 argc, char *argv[]) {
	bassert(argc > 0 && "At least one command line argument must be provided!");
	bassert(argv && "Invalid arguments value pointer!");
	mtx_lock(&vm->lock);

	struct mir_type *slice_type;
	vm_stack_ptr_t   slice_dest;
	vm_stack_ptr_t   args_dest;
	{ // Slice destination pointer.
		struct mir_var *dest_var = vm->assembly->vm_run.command_line_arguments;
		bassert(dest_var && "Missing destination variable for command line arguments!");
		slice_dest = vm_read_var(vm, dest_var);
		bassert(slice_dest && "Command line arguments not allocated!");
		slice_type = dest_var->value.type;
	}

	{ // Allocate temp for arguments.
		struct mir_type *string_type      = vm->assembly->builtin_types.t_string_literal;
		const usize      string_size      = string_type->store_size_bytes;
		const usize      total_array_size = string_size * argc;
		args_dest                         = stack_alloc(vm, total_array_size);
		for (s32 i = 0; i < argc; ++i) {
			vm_stack_ptr_t dest_elem = args_dest + string_size * i;
			vm_write_string(vm, string_type, dest_elem, make_str_from_c(argv[i]));
		}
	}

	vm_write_slice(vm, slice_type, slice_dest, args_dest, argc);
	mtx_unlock(&vm->lock);
}

void vm_override_var(struct virtual_machine *vm, struct mir_var *var, const u64 value) {
	bassert(var);
	struct mir_type *type     = var->value.type;
	vm_stack_ptr_t   dest_ptr = vm_read_var(vm, var);
	bassert(dest_ptr);
	vm_write_int(type, dest_ptr, value);
}

enum vm_interp_state vm_execute_fn(struct virtual_machine *vm,
                                   struct assembly        *assembly,
                                   struct mir_fn          *fn,
                                   mir_const_values_t     *optional_args,
                                   vm_stack_ptr_t         *optional_return) {
	bmagic_assert(fn);
	mtx_lock(&vm->lock);

	vm->assembly = assembly;
	if (optional_args && optional_args->len) {
		bassert(fn->type->data.fn.args);
		bassert(sarrlenu(fn->type->data.fn.args) == sarrlenu(optional_args) &&
		        "Invalid count of explicitly passed arguments");
		// Push all arguments in reverse order on the stack.
		for (usize i = sarrlenu(optional_args); i-- > 0;) {
			struct mir_const_expr_value *value = &sarrpeek(optional_args, i);
			stack_push(vm, value->data, value->type);
		}
	}
	enum vm_interp_state state = execute_function(vm, fn, NULL, false);
	if (state == VM_INTERP_PASSED) {
		vm_stack_ptr_t return_ptr = NULL;
		if (fn_does_return(fn)) {
			struct mir_type *ret_type = fn->type->data.fn.ret_type;
			return_ptr                = stack_pop(vm, ret_type);
		}
		if (optional_return) (*optional_return) = return_ptr;
		if (optional_args) {
			// Cleanup
			for (usize i = 0; i < sarrlenu(optional_args); ++i) {
				struct mir_const_expr_value *value = &sarrpeek(optional_args, i);
				stack_pop(vm, value->type);
			}
		}
	}
	mtx_unlock(&vm->lock);

	return state;
}

static inline void release_snapshot(struct virtual_machine *vm, struct vm_stack *stack) {
	bassert(stack);
	arrput(vm->available_comptime_call_stacks, reset_stack(stack));
}

static inline void store_snapshot(struct virtual_machine *vm, struct vm_stack *stack, struct mir_instr_call *call) {
	bassert(tbl_lookup_index(vm->comptime_call_stacks, call) == -1);
	struct vm_snapshot entry = (struct vm_snapshot){.hash = call, .stack = stack};
	tbl_insert(vm->comptime_call_stacks, entry);
}

struct get_snapshot_result {
	struct vm_stack *stack;
	bool             resume;
};

// Tries to find stack used for previous execution (in case the call was postponed). If there is
// no such stack, new one is created or reused from cache.
static struct get_snapshot_result get_snapshot(struct virtual_machine *vm, struct mir_instr_call *call) {
	bassert(call);
	struct get_snapshot_result result = {0};

	const s32 index = tbl_lookup_index(vm->comptime_call_stacks, call);
	if (index != -1) {
		bassert(vm->comptime_call_stacks[index].hash == call);
		result.stack = vm->comptime_call_stacks[index].stack;
		bcheck_true(tbl_erase(vm->comptime_call_stacks, call));
		result.resume = true;
		return result;
	}

	if (arrlenu(vm->available_comptime_call_stacks)) {
		result.stack = arrpop(vm->available_comptime_call_stacks);
		bassert(result.stack && result.stack->allocated_bytes == VM_COMPTIME_CALL_STACK_SIZE);
	} else {
		result.stack = create_stack(VM_COMPTIME_CALL_STACK_SIZE);
		batomic_fetch_add_s32(&vm->assembly->stats.comptime_call_stacks_count, 1);
	}
	bassert(result.stack);
	return result;
}

enum vm_interp_state vm_execute_comptime_call(struct virtual_machine *vm, struct assembly *assembly, struct mir_instr_call *call) {
	zone();
	mtx_lock(&vm->lock);
	vm->assembly = assembly;
	bassert(call && isflag(call->base.state, MIR_IS_ANALYZED));
	bassert(mir_is_comptime(&call->base) && "Top level call is expected to be comptime.");
	struct mir_fn *fn = mir_get_callee(call);
	bmagic_assert(fn);
	if (isflag(fn->flags, FLAG_EXTERN)) {
		babort("External function cannot be #comptime for now!");
	}

	// Compile-time calls use custom execution stack since its execution can be postponed.
	struct get_snapshot_result snapshot       = get_snapshot(vm, call);
	struct vm_stack           *previous_stack = swap_current_stack(vm, snapshot.stack);

	if (!snapshot.resume) {
		// In case we're resuming, arguments are already on the stack.
		mir_instrs_t *args = call->args;
		bassert(sarrlenu(args) == sarrlenu(fn->type->data.fn.args));
		// Push all arguments in reverse order on the stack.
		for (usize i = sarrlenu(args); i-- > 0;) {
			struct mir_const_expr_value *value = &sarrpeek(args, i)->value;
			stack_push(vm, value->data, value->type);
		}
	}
	enum vm_interp_state state = execute_function(vm, fn, call, snapshot.resume);
	switch (state) {
	case VM_INTERP_PASSED:
		// Pop return value.
		if (fn_does_return(fn)) {
			struct mir_type *ret_type = fn->type->data.fn.ret_type;
			bassert(ret_type->kind != MIR_TYPE_VOID);
			vm_stack_ptr_t ptr  = stack_pop(vm, ret_type);
			vm_stack_ptr_t dest = NULL;
			if (needs_allocation(&call->base.value)) {
				dest = data_alloc(vm, ret_type);
			} else {
				dest = (vm_stack_ptr_t)&call->base.value._tmp;
			}
			memcpy(dest, ptr, ret_type->store_size_bytes);
			call->base.value.data = dest;
		} else {
			call->base.value.data = NULL;
		}
		// @Note: No cleanup is needed here, the stack is dedicated to this call and gets cleaned
		// when it's reused eventually.
		release_snapshot(vm, snapshot.stack);
		break;

	case VM_INTERP_POSTPONE:
		store_snapshot(vm, snapshot.stack, call);
		break;

	case VM_INTERP_ABORT:
		release_snapshot(vm, snapshot.stack);
		break;
	}

	swap_current_stack(vm, previous_stack);
	mtx_unlock(&vm->lock);

	return_zone(state);
}

void vm_alloc_global(struct virtual_machine *vm, struct assembly *assembly, struct mir_var *var) {
	mtx_lock(&vm->lock);

	vm->assembly = assembly;
	bassert(var);
	bassert(isflag(var->iflags, MIR_VAR_GLOBAL) &&
	        "Allocated variable is supposed to be a global variable.");
	struct mir_type *type = var->value.type;
	bassert(type);
	// @Cleanup: Consider if we can simplify this and use just one single pointer (local and global
	// stored in union) here. Even constants can be allocated inside data buffer.
	if (var->value.is_comptime && needs_allocation(&var->value)) {
		var->value.data = data_alloc(vm, type);
	} else if (var->value.is_comptime) {
		var->value.data = (vm_stack_ptr_t)&var->value._tmp;
	} else {
		var->vm_ptr.global = data_alloc(vm, type);
	}

	mtx_unlock(&vm->lock);
}

vm_stack_ptr_t vm_alloc_raw(struct virtual_machine *vm, struct assembly UNUSED(*assembly), struct mir_type *type) {
	mtx_lock(&vm->lock);
	vm_stack_ptr_t result = data_alloc(vm, type);
	mtx_unlock(&vm->lock);
	return result;
}

// Try to fetch variable allocation pointer.
vm_stack_ptr_t vm_read_var(struct virtual_machine *vm, const struct mir_var *var) {
	mtx_lock(&vm->lock);

	vm_stack_ptr_t ptr = NULL;
	if (var->value.is_comptime) {
		ptr = var->value.data;
	} else if (isflag(var->iflags, MIR_VAR_GLOBAL)) {
		ptr = var->vm_ptr.global;
	} else {
		// local
		ptr = stack_rel_to_abs_ptr(vm, var->vm_ptr.local);
	}
	bassert(ptr && "Attempt to get allocation pointer of unallocated variable!");
	mtx_unlock(&vm->lock);

	return ptr;
}

u64 vm_read_int(struct mir_type const *type, vm_stack_ptr_t src) {
	bassert(src && "Attempt to read null source!");
	u64 result = 0;
	memcpy(&result, src, type->store_size_bytes);
	return result;
}

f64 vm_read_double(const struct mir_type *type, vm_stack_ptr_t src) {
#if BL_ASSERT_ENABLE
	const usize size = type->store_size_bytes;
	bassert(src && "Attempt to read null source!");
	bassert(size == sizeof(f64) && "Target type is not f64 type!");
#endif
	return *((f64 *)src);
}

vm_stack_ptr_t vm_read_ptr(const struct mir_type *type, vm_stack_ptr_t src) {
	const usize size = type->store_size_bytes;
	bassert(src && "Attempt to read null source!");
	bassert(size == sizeof(vm_stack_ptr_t) && "Target type is not pointer type!");
	vm_stack_ptr_t result = 0;
	memcpy(&result, src, size);
	return result;
}

f32 vm_read_float(const struct mir_type *type, vm_stack_ptr_t src) {
#if BL_ASSERT_ENABLE
	const usize size = type->store_size_bytes;
	bassert(src && "Attempt to read null source!");
	bassert(size == sizeof(f32) && "Target type is not f64 type!");
#endif

	return *((f32 *)src);
}

str_t vm_read_string(struct virtual_machine *vm, const struct mir_type *type, vm_stack_ptr_t src) {
	bassert((type->kind == MIR_TYPE_SLICE || type->kind == MIR_TYPE_STRING) &&
	        "Expected slice or string type!");
	struct mir_type *len_type = mir_get_struct_elem_type(type, MIR_SLICE_LEN_INDEX);
	struct mir_type *ptr_type = mir_get_struct_elem_type(type, MIR_SLICE_PTR_INDEX);
	vm_stack_ptr_t   len      = vm_get_struct_elem_ptr(vm->assembly, type, src, MIR_SLICE_LEN_INDEX);
	vm_stack_ptr_t   ptr      = vm_get_struct_elem_ptr(vm->assembly, type, src, MIR_SLICE_PTR_INDEX);
	return make_str((char *)vm_read_ptr(ptr_type, ptr), vm_read_int(len_type, len));
}

void vm_write_int(const struct mir_type *type, vm_stack_ptr_t dest, u64 i) {
	bassert(dest && "Attempt to write to the null destination!");
	memcpy(dest, &i, type->store_size_bytes);
}

void vm_write_double(const struct mir_type *type, vm_stack_ptr_t dest, f64 i) {
	const usize size = type->store_size_bytes;
	bassert(size == sizeof(f64) && "Target type is not f64 type!");
	bassert(dest && "Attempt to write to the null destination!");
	memcpy(dest, &i, size);
}

void vm_write_float(const struct mir_type *type, vm_stack_ptr_t dest, f32 i) {
	const usize size = type->store_size_bytes;
	bassert(size == sizeof(f32) && "Target type is not f64 type!");
	bassert(dest && "Attempt to write to the null destination!");
	memcpy(dest, &i, size);
}

void vm_write_ptr(const struct mir_type *type, vm_stack_ptr_t dest, vm_stack_ptr_t ptr) {
	bassert(dest && "Attempt to write to the null destination!");
	memcpy(dest, &ptr, type->store_size_bytes);
}

void _vm_write_value(usize dest_size, vm_stack_ptr_t dest, vm_stack_ptr_t src) {
	bassert(dest && "Attempt to write to the null destination!");
	memcpy(dest, src, dest_size);
}

void vm_write_string(struct virtual_machine *vm,
                     const struct mir_type  *type,
                     vm_stack_ptr_t          dest,
                     str_t                   str) {
	bassert(str.len >= 0 && "Invalid string constant length.");
	bassert(type->kind == MIR_TYPE_SLICE && "Expected slice type!");
	vm_write_slice(vm, type, dest, (void *)str.ptr, str.len);
}

void vm_write_slice(struct virtual_machine *vm,
                    const struct mir_type  *type,
                    vm_stack_ptr_t          dest,
                    void                   *ptr,
                    s64                     len) {
	bassert((type->kind == MIR_TYPE_SLICE || type->kind == MIR_TYPE_STRING) &&
	        "Expected slice or string type!");
	struct mir_type *dest_len_type = mir_get_struct_elem_type(type, MIR_SLICE_LEN_INDEX);
	struct mir_type *dest_ptr_type = mir_get_struct_elem_type(type, MIR_SLICE_PTR_INDEX);
	vm_stack_ptr_t   dest_len      = vm_get_struct_elem_ptr(vm->assembly, type, dest, MIR_SLICE_LEN_INDEX);
	vm_stack_ptr_t   dest_ptr      = vm_get_struct_elem_ptr(vm->assembly, type, dest, MIR_SLICE_PTR_INDEX);
	vm_write_int(dest_len_type, dest_len, (u64)len);
	vm_write_ptr(dest_ptr_type, dest_ptr, (vm_stack_ptr_t)ptr);
}

ptrdiff_t vm_get_struct_elem_offset(struct assembly *assembly, const struct mir_type *type, u32 i) {
	bassert(mir_is_composite_type(type) && "Expected structure type");
	if (type->data.strct.is_union) {
		return 0;
	}

	return (ptrdiff_t)LLVMOffsetOfElement(assembly->llvm.TD, type->llvm_type, i);
}

ptrdiff_t vm_get_array_elem_offset(const struct mir_type *type, u32 i) {
	bassert(type->kind == MIR_TYPE_ARRAY && "Expected array type");
	struct mir_type *elem_type = type->data.array.elem_type;
	bassert(elem_type);
	return (ptrdiff_t)elem_type->store_size_bytes * i;
}

vm_stack_ptr_t vm_get_struct_elem_ptr(struct assembly       *assembly,
                                      const struct mir_type *type,
                                      vm_stack_ptr_t         ptr,
                                      u32                    i) {
	bassert(mir_is_composite_type(type) && "Expected structure type");
	if (type->data.strct.is_union) {
		return ptr;
	}

	return ptr + vm_get_struct_elem_offset(assembly, type, i);
}

vm_stack_ptr_t vm_get_array_elem_ptr(const struct mir_type *type, vm_stack_ptr_t ptr, u32 i) {
	return ptr + vm_get_array_elem_offset(type, i);
}

void vm_do_cast(vm_stack_ptr_t   dest,
                vm_stack_ptr_t   src,
                struct mir_type *dest_type,
                struct mir_type *src_type,
                s32              op) {
	bassert(dest && "Missing cast destination!");
	bassert(src && "Missing cast source!");
	bassert(src != dest && "Cast operation src and dest points to same memory.");
	bassert(dest_type && "Missing cast destination type!");
	bassert(src_type && "Missing cast source type!");

	const usize src_size  = src_type->store_size_bytes;
	const usize dest_size = dest_type->store_size_bytes;

	switch (op) {
	case MIR_CAST_INTTOPTR:
	case MIR_CAST_PTRTOINT:
	case MIR_CAST_NONE:
	case MIR_CAST_BITCAST:
	case MIR_CAST_ZEXT:
	case MIR_CAST_TRUNC:
		memset(dest, 0, dest_size);
		memcpy(dest, src, src_size);
		break;

	case MIR_CAST_PTRTOBOOL: {
		vm_write_int(dest_type, dest, vm_read_as(u64, src) > 0);
		break;
	}

	case MIR_CAST_SEXT: {
		// src is smaller than dest
		switch (src_size) {
		case 1:
			vm_write_int(dest_type, dest, (u64)vm_read_as(s8, src));
			break;
		case 2:
			vm_write_int(dest_type, dest, (u64)vm_read_as(s16, src));
			break;
		case 4:
			vm_write_int(dest_type, dest, (u64)vm_read_as(s32, src));
			break;
		default:
			abort();
		}
		break;
	}

	case MIR_CAST_FPEXT: {
		// src is smaller than dest
		vm_write_double(dest_type, dest, (f64)vm_read_float(src_type, src));
		break;
	}

	case MIR_CAST_FPTRUNC: {
		// src is bigger than dest
		vm_write_float(dest_type, dest, (f32)vm_read_double(src_type, src));
		break;
	}

	case MIR_CAST_FPTOUI:
	case MIR_CAST_FPTOSI: {
		// real to signed integer same size
		switch (src_size) {
		case 4: {
			vm_write_int(dest_type, dest, (u64)vm_read_as(f32, src));
			break;
		}

		case 8: {
			vm_write_int(dest_type, dest, (u64)vm_read_as(f64, src));
			break;
		}
		default:
			babort("Invalid!");
		}
		break;
	}

	case MIR_CAST_SITOFP: {
		// signed integer real
		switch (src_size) {
		case 1: {
			if (dest_size == 4)
				vm_write_as(f32, dest, vm_read_as(s8, src));
			else
				vm_write_as(f64, dest, vm_read_as(s8, src));
			break;
		}

		case 2: {
			if (dest_size == 4)
				vm_write_as(f32, dest, vm_read_as(s16, src));
			else
				vm_write_as(f64, dest, vm_read_as(s16, src));
			break;
		}

		case 4: {
			if (dest_size == 4)
				vm_write_as(f32, dest, (f32)vm_read_as(s32, src));
			else
				vm_write_as(f64, dest, (f64)vm_read_as(s32, src));
			break;
		}

		case 8: {
			if (dest_size == 4)
				vm_write_as(f32, dest, (f32)vm_read_as(s64, src));
			else
				vm_write_as(f64, dest, (f64)vm_read_as(s64, src));
			break;
		}
		default:
			babort("Invalid!");
		}
		break;
	}

	case MIR_CAST_UITOFP: {
		const u64 v = vm_read_int(src_type, src);
		switch (dest_size) {
		case 4: {
			vm_write_as(f32, dest, (f32)v);
			break;
		}

		case 8: {
			vm_write_as(f64, dest, (f64)v);
			break;
		}
		default:
			babort("Invalid!");
		}
		break;
	}

	default:
		babort("invalid cast operation");
	}
}
