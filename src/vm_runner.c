// =================================================================================================
// bl
//
// File:   vm_runner.c
// Author: Martin Dorazil
// Date:   8.12.20
//
// Copyright 2020 Martin Dorazil
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

#include "bldebug.h"
#include "builder.h"
#include "stb_ds.h"

#define TEXT_LINE "--------------------------------------------------------------------------------"

void vm_tests_run(struct assembly *assembly) {
	struct virtual_machine *vm             = &assembly->vm;
	struct mir_fn         **cases          = assembly->testing.cases;
	const bool              minimal_output = assembly->target->tests_minimal_output;

	const s64 test_count = arrlen(cases);
	if (test_count == 0) {
		return;
	}

	if (!minimal_output) {
		printf("\nTesting started in compile time for target: %s\n", assembly->target->name);
		printf(TEXT_LINE "\n");
	}

	s32 failed_count = 0;

	struct case_meta {
		str_t name;
		f64   runtime_ms;
	};

	array(struct case_meta) failed = NULL;

	for (s64 i = 0; i < test_count; ++i) {
		struct mir_fn *test_fn = cases[i];
		bassert(isflag(test_fn->flags, FLAG_TEST_FN));
		const f64 start = get_tick_ms();

		builder.current_executed_assembly = assembly;
		const enum vm_interp_state state  = vm_execute_fn(vm, assembly, test_fn, NULL, NULL);
		builder.current_executed_assembly = NULL;

		const f64   runtime_ms = get_tick_ms() - start;
		const str_t name       = test_fn->id->str;
		if (state == VM_INTERP_PASSED) {
			printf("[ PASS |      ] " STR_FMT " (%f ms)\n", STR_ARG(name), runtime_ms);
		} else {
			printf("[      | FAIL ] " STR_FMT " (%f ms)\n", STR_ARG(name), runtime_ms);
			arrput(failed, ((struct case_meta){.name = name, .runtime_ms = runtime_ms}));
			builder.errorc = 0;
			++failed_count;
		}
	}

	s32 perc = 100;
	if (failed_count > 0)
		perc = (s32)((f32)(test_count - failed_count) / ((f32)test_count * 0.01f));
	if (!minimal_output) {
		printf("\nResults:\n");
		printf(TEXT_LINE "\n");
		for (s64 i = 0; i < arrlen(failed); ++i) {
			struct case_meta *f = &failed[i];
			printf("[      | FAIL ] " STR_FMT " (%f ms)\n", STR_ARG(f->name), f->runtime_ms);
		}

		if (arrlen(failed)) printf(TEXT_LINE "\n");
		printf("Executed: %llu, passed %d%%.\n", (unsigned long long)test_count, perc);
		printf(TEXT_LINE "\n");
	}
	arrfree(failed);
	assembly->vm_run.last_execution_status = failed_count;
}

void vm_build_entry_run(struct assembly *assembly) {
	struct virtual_machine *vm     = &assembly->vm;
	struct mir_fn          *entry  = assembly->vm_run.build_entry;
	const struct target    *target = assembly->target;
	if (!entry) {
		builder_error("struct assembly '%s' has no build entry function!", assembly->target->name);
		assembly->vm_run.last_execution_status = EXIT_FAILURE;
		return;
	}
	if (target->vm.argc > 0) {
		vm_provide_command_line_arguments(vm, target->vm.argc, target->vm.argv);
	}
	vm_override_var(vm, assembly->vm_run.is_comptime_run, true);
	builder.current_executed_assembly = assembly;

	vm_execute_fn(vm, assembly, entry, NULL, NULL);

	builder.current_executed_assembly = NULL;
	vm_override_var(vm, assembly->vm_run.is_comptime_run, false);
	assembly->vm_run.last_execution_status = EXIT_SUCCESS;
}

void vm_entry_run(struct assembly *assembly) {
	struct virtual_machine *vm     = &assembly->vm;
	struct mir_fn          *entry  = assembly->vm_run.entry;
	const struct target    *target = assembly->target;
	builder_info("\nExecuting 'main' in compile time...");
	if (!entry) {
		builder_error("struct assembly '%s' has no entry function!", assembly->target->name);
		assembly->vm_run.last_execution_status = EXIT_FAILURE;
		return;
	}
	struct mir_type *fn_type = entry->type;
	bassert(fn_type && fn_type->kind == MIR_TYPE_FN);
	bassert(!fn_type->data.fn.args);
	if (target->vm.argc > 0) {
		vm_provide_command_line_arguments(vm, target->vm.argc, target->vm.argv);
	}

	vm_override_var(vm, assembly->vm_run.is_comptime_run, true);
	builder.current_executed_assembly = assembly;
	vm_stack_ptr_t ret_ptr            = NULL;
	s32            result             = EXIT_SUCCESS;

	const enum vm_interp_state state = vm_execute_fn(vm, assembly, entry, NULL, &ret_ptr);
	if (state == VM_INTERP_PASSED) {
		if (ret_ptr) {
			struct mir_type *ret_type = fn_type->data.fn.ret_type;
			result                    = (s32)vm_read_int(ret_type, ret_ptr);
			builder_info("Execution finished with state: %d\n", result);
		} else {
			builder_info("Execution finished without errors");
		}
	} else {
		builder_warning("Execution finished with errors");
	}
	builder.current_executed_assembly = NULL;
	vm_override_var(vm, assembly->vm_run.is_comptime_run, false);
	assembly->vm_run.last_execution_status = result;
}
