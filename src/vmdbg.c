// =================================================================================================
// bl
//
// File:   vmdbg.c
// Author: Martin Dorazil
// Date:   27/09/2021
//
// Copyright 2021 Martin Dorazil
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

#include "vmdbg.h"
#include "builder.h"
#include "mir_printer.h"
#include "stb_ds.h"
#include "table.h"

enum state {
	CONTINUE,
	STEPPING,
};

struct stack_context {
	struct vm_stack *hash;
	array(uintptr_t) stackops;
};

static enum state              state                  = CONTINUE;
static bool                    mir_mode               = false;
static bool                    verbose_stack          = false;
static struct virtual_machine *current_vm             = NULL;
static hash_table(struct stack_context) stack_context = NULL;

static void print(struct mir_instr *instr) {
	if (!mir_mode && instr->node && instr->node->location) {
		builder_print_location(stdout, instr->node->location, 0, 0);
	} else {
		if (instr->prev) {
			printf("  ");
			mir_print_instr(stdout, current_vm->assembly, instr->prev);
		}
		printf("> ");
		mir_print_instr(stdout, current_vm->assembly, instr);
		if (instr->next) {
			printf("  ");
			mir_print_instr(stdout, current_vm->assembly, instr->next);
		}
	}
}

static void print_data(struct mir_type *type, vm_stack_ptr_t ptr) {
	printf("(%p) ", ptr);
	if (!type) return;

	switch (type->kind) {
	case MIR_TYPE_INT: {
		printf("%llu", vm_read_int(type, ptr));
		return;
	}
	case MIR_TYPE_REAL: {
		if (type->data.real.bitcount == 32)
			printf("%f", vm_read_float(type, ptr));
		else if (type->data.real.bitcount == 64)
			printf("%f", vm_read_double(type, ptr));
		return;
	}
	case MIR_TYPE_PTR: {
		printf("%p", vm_read_ptr(type, ptr));
		return;
	}
	case MIR_TYPE_STRING: {
		const str_t str = vm_read_string(current_vm, type, ptr);
		printf("\"%.*s\"", str.len, str.ptr);
		return;
	}
	case MIR_TYPE_SLICE: {
		struct mir_type *len_type  = mir_get_struct_elem_type(type, MIR_SLICE_LEN_INDEX);
		struct mir_type *elem_type = mir_get_struct_elem_type(type, MIR_SLICE_PTR_INDEX);

		const vm_stack_ptr_t len_ptr =
		    vm_get_struct_elem_ptr(current_vm->assembly, type, ptr, MIR_SLICE_LEN_INDEX);
		const s64 len         = (s64)vm_read_int(len_type, len_ptr);
		s64       limited_len = len;
		CLAMP(limited_len, 0, 256);

		if (elem_type->kind == MIR_TYPE_PTR) {
			struct mir_type *char_type = mir_deref_type(elem_type);
			if (char_type->kind == MIR_TYPE_INT && char_type->data.integer.bitcount == 8 &&
			    !char_type->data.integer.is_signed) {
				const vm_stack_ptr_t elem_ptr =
				    vm_get_struct_elem_ptr(current_vm->assembly, type, ptr, MIR_SLICE_PTR_INDEX);
				const char *str = (const char *)vm_read_ptr(elem_type, elem_ptr);

				s64 len_without_new_lines = 0;
				for (; len_without_new_lines < limited_len; ++len_without_new_lines) {
					if (str[len_without_new_lines] == '\n') break;
				}
				printf("[%lld] \"%.*s", len, (s32)len_without_new_lines, str);
				const s64 count_of_new_lines = limited_len - len_without_new_lines;
				for (s64 i = 0; i < count_of_new_lines; ++i) {
					printf("\\n");
				}
				printf("\"");
				return;
			}
		}
	}
	default:
		break;
	}
}

static void print_variable(struct mir_var *var) {
	bassert(var);

	if (var->value.type) {
		str_buf_t type_name = mir_type2str(var->value.type, true);
		printf("%-32.*s%-32.*s",
		       var->linkage_name.len,
		       var->linkage_name.ptr,
		       type_name.len,
		       type_name.ptr);
		put_tmp_str(type_name);
	} else {
		printf("%-32.*s%-32s", var->linkage_name.len, var->linkage_name.ptr, "<UNKNONW_TYPE>");
	}

	vm_stack_ptr_t data = vm_read_var(current_vm, var);
	print_data(var->value.type, data);
	printf("\n");
}

static void print_local_variables(struct mir_instr *instr) {
	bassert(instr);
	const struct mir_fn *fn = mir_instr_owner_fn(instr);
	if (!fn) return;

	printf("%-32s%-32s%-32s\n", "Name", "Type", "Value");
	for (usize i = 0; i < arrlenu(fn->variables); ++i) {
		struct mir_var *var = fn->variables[i];
		if (!var) {
			continue;
		}
		print_variable(var);
	}
}

static void cmd(void) {
#define CMD(s, l) ((strcmp(buf, s) == 0) || (strcmp(buf, l) == 0))
	static s32  i          = 0;
	static char tmp[2][64] = {{0}, {0}};

NEXT:
	printf(": ");
	char *buf = tmp[i ^= 1];
	if (!fgets(buf, static_arrlenu(tmp[0]), stdin)) goto NEXT;
	if (buf[0] == '\n') {
		buf = tmp[i ^= 1];
		if (buf[0] == '\0') goto NEXT;
	} else {
		const usize len = strlen(buf);
		bassert(len);
		if (buf[len - 1] == '\n') buf[len - 1] = '\0';
	}
	if (CMD("q", "quit")) {
		printf("exiting...\n");
		vmdbg_detach();
	} else if (CMD("n", "next")) {
		state = STEPPING;
	} else if (CMD("c", "continue")) {
		state = CONTINUE;
	} else if (CMD("p", "print")) {
		print(current_vm->stack->pc);
		goto NEXT;
	} else if (CMD("pl", "print-locals")) {
		print_local_variables(current_vm->stack->pc);
		goto NEXT;
	} else if (CMD("bt", "backtrace")) {
		vm_print_backtrace(current_vm);
		goto NEXT;
	} else if (CMD("vs=on", "verbose-stack=on")) {
		verbose_stack = true;
		goto NEXT;
	} else if (CMD("vs=off", "verbose-stack=off")) {
		verbose_stack = false;
		goto NEXT;
	} else if (CMD("mir=on", "mir-mode=on")) {
		mir_mode = true;
		goto NEXT;
	} else if (CMD("mir=off", "mir-mode=off")) {
		mir_mode = false;
		goto NEXT;
	} else if (CMD("h", "help")) {
		printf("  h, help                             = Show this help.\n"
		       "  q, quit                             = Stop debugging.\n"
		       "  n, next                             = Step to next instruction.\n"
		       "  c, continue                         = Continue execution.\n"
		       "  p, print                            = Print current instruction.\n"
		       "  pl, print-locals                    = Print local variables.\n"
		       "  bt, backtrace                       = Print current backtrace.\n"
		       "  vs=<on|off>, verbose-stack=<on|off> = Log stack operations.\n"
		       "  mir=<on|off>, mir-mode=<on|off>     = Enable/disable MIR instruction level "
		       "debugging.\n");
		goto NEXT;
	} else {
		builder_error("Invalid command.");
		builder.errorc = 0; // @Hack: reset error count.
		buf[0]         = '\0';
		goto NEXT;
	}
#undef CMD
}

// =================================================================================================
// Public
// =================================================================================================

void vmdbg_attach(struct virtual_machine *vm) {
	bassert(current_vm == NULL);
	current_vm = vm;
}

void vmdbg_detach(void) {
	for (u64 i = 0; i < tbl_len(stack_context); ++i) {
		arrfree(stack_context[i].stackops);
	}
	tbl_free(stack_context);

	current_vm = NULL;
	state      = CONTINUE;
}

void vmdbg_notify_instr(struct mir_instr *instr) {
	static struct unit *last_unit = NULL;
	static u16          last_line = -1;
	if (!current_vm) return;
	if (state != STEPPING) return;
	if (mir_mode) {
		print(instr);
	} else if (instr->node && instr->node->location) {
		struct location *loc = instr->node->location;
		if (last_unit == loc->unit && last_line == loc->line) {
			return;
		}
		last_unit = loc->unit;
		last_line = loc->line;
		print(instr);
	} else {
		return;
	}
	cmd();
}

static struct stack_context *get_stack_context(void) {
	bassert(current_vm->stack);
	struct vm_stack *stack = current_vm->stack;
	bassert(stack);
	s64 index = tbl_lookup_index(stack_context, stack);
	if (index == -1) {
		tbl_insert(stack_context, ((struct stack_context){.hash = stack}));
		index = tbl_len(stack_context) - 1; // New item is last in the array.
		bassert(index != -1);
	}
	return &stack_context[index];
}

static bool pop_is_valid(void *ptr) {
	struct stack_context *sctx = get_stack_context();
	if (arrlenu(sctx->stackops) == 0) return false;
	return arrpop(sctx->stackops) == (uintptr_t)ptr;
}

static bool rollback_is_valid(void *ptr) {
	struct stack_context *sctx = get_stack_context();
	while (arrlenu(sctx->stackops)) {
		if (arrpop(sctx->stackops) == (uintptr_t)ptr) return true;
	}
	return false;
}

void vmdbg_notify_stack_op(enum vmdbg_stack_op op, struct mir_type *type, void *ptr) {
	if (!current_vm) return;
	struct virtual_machine *vm = current_vm;

	bassert(ptr);
	if (verbose_stack) {
		switch (op) {
		case VMDBG_PUSH_RA:
			if (vm->stack->pc) {
				color_print(stdout,
				            BL_RED,
				            "%6zu %20s  PUSH RA (%p)\n",
				            (size_t)vm->stack->pc->id,
				            mir_instr_name(vm->stack->pc),
				            ptr);
			} else {
				color_print(stdout, BL_RED, "     - %20s  PUSH RA\n", "Terminal");
			}
			break;
		case VMDBG_POP_RA:
			color_print(stdout,
			            BL_BLUE,
			            "%6llu %20s  POP RA  (%p)\n",
			            vm->stack->pc->id,
			            mir_instr_name(vm->stack->pc),
			            ptr);
			break;
		case VMDBG_PUSH: {
			unsigned long long size      = type->store_size_bytes;
			str_buf_t          type_name = mir_type2str(type, true);
			if (vm->stack->pc) {
				color_print(stdout,
				            BL_RED,
				            "%6llu %20s  PUSH    (%lluB, %p) %s\n",
				            (unsigned long long)vm->stack->pc->id,
				            mir_instr_name(vm->stack->pc),
				            size,
				            ptr,
				            str_buf_to_c(type_name));
			} else {
				color_print(stdout,
				            BL_RED,
				            "     -                       PUSH    (%lluB, %p) %s\n",
				            size,
				            ptr,
				            str_buf_to_c(type_name));
			}
			put_tmp_str(type_name);
			break;
		}
		case VMDBG_POP: {
			unsigned long long size      = type->store_size_bytes;
			str_buf_t          type_name = mir_type2str(type, true);
			if (vm->stack->pc) {
				color_print(stdout,
				            BL_BLUE,
				            "%6llu %20s  POP     (%lluB, %p) %s\n",
				            vm->stack->pc->id,
				            mir_instr_name(vm->stack->pc),
				            size,
				            ptr,
				            str_buf_to_c(type_name));
			} else {
				color_print(stdout,
				            BL_BLUE,
				            "     -                       POP     (%lluB, %p) %s\n",
				            size,
				            ptr,
				            str_buf_to_c(type_name));
			}
			put_tmp_str(type_name);
			break;
		}
		}
	}
	switch (op) {
	case VMDBG_PUSH_RA:
	case VMDBG_PUSH: {
		struct stack_context *sctx = get_stack_context();
		arrput(sctx->stackops, (uintptr_t)ptr);
		break;
	}
	case VMDBG_POP:
		if (!pop_is_valid(ptr)) {
			builder_error("Invalid POP operation on address %p", ptr);
			print(vm->stack->pc);
			vm_print_backtrace(vm);
			babort("Stack memory corrupted!");
		}
		break;
	case VMDBG_POP_RA:
		if (!rollback_is_valid(ptr)) {
			builder_error("Invalid POP RA rollback operation on address %p", ptr);
			print(vm->stack->pc);
			vm_print_backtrace(vm);
			babort("Stack memory corrupted!");
		}
		break;
	}
}

void vmdbg_notify_stack_swap(void) {
	if (!current_vm) return;
	printf("     -                       SWAP\n");
}

void vmdbg_break(void) {
	if (!current_vm) return;
	printf("\nHit breakpoint in assembly '%s'.\n", current_vm->assembly->target->name);
	state = STEPPING;
	vmdbg_notify_instr(current_vm->stack->pc);
}
