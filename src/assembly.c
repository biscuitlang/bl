// =================================================================================================
// blc
//
// File:   assembly.c
// Author: Martin Dorazil
// Date:   09/02/2018
//
// Copyright 2017 Martin Dorazil
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

#include "assembly.h"
#include "conf.h"
#if !BL_PLATFORM_WIN
#include <errno.h>
#endif

#include "builder.h"
#include "stb_ds.h"
#include <string.h>

// Total size of all small arrays allocated later in a single arena.
static const usize SARR_TOTAL_SIZE = sizeof(union {
	ast_nodes_t        _1;
	mir_args_t         _2;
	mir_fns_t          _3;
	mir_types_t        _4;
	mir_members_t      _5;
	mir_variants_t     _6;
	mir_instrs_t       _7;
	mir_switch_cases_t _8;
	ints_t             _9;
});

const char *arch_names[] = {
#define GEN_ARCH
#define entry(X) #X,
#include "target.def"
#undef entry
#undef GEN_ARCH
};

const char *vendor_names[] = {
#define GEN_VENDOR
#define entry(X) #X,
#include "target.def"
#undef entry
#undef GEN_VENDOR
};

const char *os_names[] = {
#define GEN_OS
#define entry(X) #X,
#include "target.def"
#undef entry
#undef GEN_OS
};

const char *env_names[] = {
#define GEN_ENV
#define entry(X) #X,
#include "target.def"
#undef entry
#undef GEN_ENV
};

static void sarr_dtor(sarr_any_t *arr) {
	sarrfree(arr);
}

static void dl_init(struct assembly *assembly) {
	DCCallVM *vm = dcNewCallVM(4096);
	dcMode(vm, DC_CALL_C_DEFAULT);
	assembly->dc_vm = vm;
}

static void dl_terminate(struct assembly *assembly) {
	dcFree(assembly->dc_vm);
}

static void parse_triple(const char *llvm_triple, struct target_triple *out_triple) {
	bassert(out_triple);
	char *arch, *vendor, *os, *env;
	arch = vendor = os = env = "";
	const char *delimiter    = "-";
	char       *tmp          = strdup(llvm_triple);
	char       *token;
	char       *it    = tmp;
	s32         state = 0;
	// arch-vendor-os-evironment
	while ((token = strtok_r(it, delimiter, &it))) {
		switch (state++) {
		case 0:
			arch = token;
			break;
		case 1:
			vendor = token;
			break;
		case 2:
			os = token;
			break;
		case 3:
			env = token;
			break;
		default:
			break;
		}
	}

	out_triple->arch = ARCH_unknown;
	for (usize i = 0; i < static_arrlenu(arch_names); ++i) {
		if (strcmp(arch, arch_names[i]) == 0) {
			out_triple->arch = i;
			break;
		}
	}

	out_triple->vendor = VENDOR_unknown;
	for (usize i = 0; i < static_arrlenu(vendor_names); ++i) {
		if (strncmp(vendor, vendor_names[i], strlen(vendor_names[i])) == 0) {
			out_triple->vendor = i;
			break;
		}
	}

	out_triple->os = OS_unknown;
	for (usize i = 0; i < static_arrlenu(os_names); ++i) {
		if (strncmp(os, os_names[i], strlen(os_names[i])) == 0) {
			out_triple->os = i;
			break;
		}
	}

	out_triple->env = ENV_unknown;
	for (usize i = 0; i < static_arrlenu(env_names); ++i) {
		if (strcmp(env, env_names[i]) == 0) {
			out_triple->env = i;
			break;
		}
	}
	free(tmp);
}

static void llvm_init(struct assembly *assembly) {
	const s32 triple_len = target_triple_to_string(&assembly->target->triple, NULL, 0);
	char     *triple     = bmalloc(triple_len);
	target_triple_to_string(&assembly->target->triple, triple, triple_len);

	char *cpu       = /*LLVMGetHostCPUName()*/ "";
	char *features  = /*LLVMGetHostCPUFeatures()*/ "";
	char *error_msg = NULL;
	builder_log("Target: %s", triple);
	LLVMTargetRef llvm_target = NULL;
	if (LLVMGetTargetFromTriple(triple, &llvm_target, &error_msg)) {
		builder_error("Cannot get target with error: %s!", error_msg);
		LLVMDisposeMessage(error_msg);
		builder_error("Available targets are:");
		LLVMTargetRef target = LLVMGetFirstTarget();
		while (target) {
			builder_error("  %s", LLVMGetTargetName(target));
			target = LLVMGetNextTarget(target);
		}
		babort("Cannot get target");
	}
	LLVMRelocMode reloc_mode = LLVMRelocDefault;
	switch (assembly->target->kind) {
	case ASSEMBLY_SHARED_LIB:
		reloc_mode = LLVMRelocPIC;
		break;
	default:
		break;
	}
	LLVMTargetMachineRef llvm_tm = LLVMCreateTargetMachine(llvm_target, triple, cpu, features, opt_to_LLVM(assembly->target->opt), reloc_mode, LLVMCodeModelDefault);
	LLVMTargetDataRef    llvm_td = LLVMCreateTargetDataLayout(llvm_tm);
	assembly->llvm.ctx           = llvm_context_create();
	assembly->llvm.TM            = llvm_tm;
	assembly->llvm.TD            = llvm_td;
	assembly->llvm.triple        = triple;
}

static void llvm_terminate(struct assembly *assembly) {
	LLVMDisposeModule(assembly->llvm.module);
	LLVMDisposeTargetMachine(assembly->llvm.TM);
	LLVMDisposeTargetData(assembly->llvm.TD);
	llvm_context_dispose(assembly->llvm.ctx);
	bfree(assembly->llvm.triple);
}

static void native_lib_terminate(struct native_lib *lib) {
	if (lib->handle) dlFreeLibrary(lib->handle);
	if (lib->is_internal) return;
}

// Create directory tree and set out_path to full path.
static bool create_auxiliary_dir_tree_if_not_exist(const str_t _path, str_buf_t *out_path) {
	bassert(_path.len);
	bassert(out_path);
#if BL_PLATFORM_WIN
	str_buf_t tmp_path = get_tmp_str();
	str_buf_append(&tmp_path, _path);
	win_path_to_unix(tmp_path);
	const str_t path = str_buf_view(tmp_path);
#else
	const str_t path = _path;
#endif
	if (!dir_exists(path)) {
		if (!create_dir_tree(path)) {
#if BL_PLATFORM_WIN
			put_tmp_str(tmp_path);
#endif
			return false;
		}
	}
	str_buf_t tmp_full_path = get_tmp_str();
	if (!brealpath(path, &tmp_full_path)) {
		put_tmp_str(tmp_full_path);
		return false;
	}
	str_buf_clr(out_path);
	str_buf_append(out_path, tmp_full_path);
#if BL_PLATFORM_WIN
	put_tmp_str(tmp_path);
#endif
	put_tmp_str(tmp_full_path);
	return true;
}

// =================================================================================================
// PUBLIC
// =================================================================================================
struct target *target_new(const char *name) {
	bassert(name && "struct assembly name not specified!");
	struct target *target = bmalloc(sizeof(struct target));
	memset(target, 0, sizeof(struct target));
	bmagic_set(target);
	str_buf_setcap(&target->default_custom_linker_opt, 128);
	str_buf_setcap(&target->module_dir, 128);
	str_buf_setcap(&target->out_dir, 128);
	target->name = strdup(name);

	// Default target uses current working directory which may be changed by user compiler flags
	// later (--work-dir).
	str_buf_append(&target->out_dir, cstr("."));

	// Setup some defaults.
	target->opt       = ASSEMBLY_OPT_DEBUG;
	target->kind      = ASSEMBLY_EXECUTABLE;
	target->reg_split = true;
#ifdef BL_DEBUG
	target->verify_llvm = true;
#endif

#if BL_PLATFORM_WIN
	target->di        = ASSEMBLY_DI_CODEVIEW;
	target->copy_deps = true;
#else
	target->di        = ASSEMBLY_DI_DWARF;
	target->copy_deps = false;
#endif
	target->triple = (struct target_triple){
	    .arch = ARCH_unknown, .vendor = VENDOR_unknown, .os = OS_unknown, .env = ENV_unknown};
	return target;
}

struct target *target_dup(const char *name, const struct target *other) {
	bmagic_assert(other);
	struct target *target = target_new(name);
	memcpy(target, other, sizeof(struct {TARGET_COPYABLE_CONTENT}));
	target_set_output_dir(target, str_buf_to_c(other->out_dir));
	target->vm = other->vm;
	bmagic_set(target);
	return target;
}

void target_delete(struct target *target) {
	for (usize i = 0; i < arrlenu(target->files); ++i)
		free(target->files[i]);
	for (usize i = 0; i < arrlenu(target->default_lib_paths); ++i)
		free(target->default_lib_paths[i]);
	for (usize i = 0; i < arrlenu(target->default_libs); ++i)
		free(target->default_libs[i]);

	arrfree(target->files);
	arrfree(target->default_lib_paths);
	arrfree(target->default_libs);
	str_buf_free(&target->out_dir);
	str_buf_free(&target->default_custom_linker_opt);
	str_buf_free(&target->module_dir);
	free(target->name);
	bfree(target);
}

void target_add_file(struct target *target, const char *filepath) {
	bmagic_assert(target);
	bassert(filepath && "Invalid filepath!");
	char *dup = strdup(filepath);
	win_path_to_unix(make_str_from_c(dup));
	arrput(target->files, dup);
}

void target_set_vm_args(struct target *target, s32 argc, char **argv) {
	bmagic_assert(target);
	target->vm.argc = argc;
	target->vm.argv = argv;
}

void target_add_lib_path(struct target *target, const char *path) {
	bmagic_assert(target);
	if (!path) return;
	char *dup = strdup(path);
	win_path_to_unix(make_str_from_c(dup));
	arrput(target->default_lib_paths, dup);
}

void target_add_lib(struct target *target, const char *lib) {
	bmagic_assert(target);
	if (!lib) return;
	char *dup = strdup(lib);
	win_path_to_unix(make_str_from_c(dup));
	arrput(target->default_libs, dup);
}

void target_set_output_dir(struct target *target, const char *dir) {
	bmagic_assert(target);
	if (!dir) builder_error("Cannot create output directory.");
	if (!create_auxiliary_dir_tree_if_not_exist(make_str_from_c(dir), &target->out_dir)) {
		builder_error("Cannot create output directory '%s'.", dir);
	}
}

void target_append_linker_options(struct target *target, const char *option) {
	bmagic_assert(target);
	if (!option) return;
	str_buf_append_fmt(&target->default_custom_linker_opt, "{s} ", option);
}

void target_set_module_dir(struct target *target, const char *dir) {
	bmagic_assert(target);
	if (!dir) {
		builder_error("Cannot create module directory.");
		return;
	}
	if (!create_auxiliary_dir_tree_if_not_exist(make_str_from_c(dir), &target->module_dir)) {
		builder_error("Cannot create module directory '%s'.", dir);
		return;
	}
}

bool target_is_triple_valid(struct target_triple *triple) {
	char triple_str[TRIPLE_MAX_LEN];
	target_triple_to_string(triple, triple_str, static_arrlenu(triple_str));
	bool   is_valid = false;
	char **list     = builder_get_supported_targets();
	char **it       = list;
	for (; *it; it++) {
		if (strcmp(triple_str, *it) == 0) {
			is_valid = true;
			break;
		}
	}
	bfree(list);
	return is_valid;
}

bool target_init_default_triple(struct target_triple *triple) {
	char *llvm_triple = LLVMGetDefaultTargetTriple();
	parse_triple(llvm_triple, triple);
	if (!target_is_triple_valid(triple)) {
		builder_error("Target triple '%s' is not supported by the compiler.", llvm_triple);
		LLVMDisposeMessage(llvm_triple);
		return false;
	}
	LLVMDisposeMessage(llvm_triple);
	return true;
}

s32 target_triple_to_string(const struct target_triple *triple, char *buf, s32 buf_len) {
	const char *arch, *vendor, *os, *env;
	arch = vendor = os = env = "";
	s32 len                  = 0;
	if (triple->arch < static_arrlenu(arch_names)) arch = arch_names[triple->arch];
	if (triple->vendor < static_arrlenu(vendor_names)) vendor = vendor_names[triple->vendor];
	if (triple->os < static_arrlenu(os_names)) os = os_names[triple->os];
	if (triple->env < static_arrlenu(env_names)) env = env_names[triple->env];
	if (triple->env == ENV_unknown) {
		len = snprintf(NULL, 0, "%s-%s-%s", arch, vendor, os) + 1;
		if (buf) snprintf(buf, MIN(buf_len, len), "%s-%s-%s", arch, vendor, os);
	} else {
		len = snprintf(NULL, 0, "%s-%s-%s-%s", arch, vendor, os, env) + 1;
		if (buf) snprintf(buf, MIN(buf_len, len), "%s-%s-%s-%s", arch, vendor, os, env);
	}
	return len;
}

static void thread_local_init(struct assembly *assembly) {
	zone();
	const u32 thread_count = get_thread_count();
	arrsetlen(assembly->thread_local_contexts, thread_count);
	bl_zeromem(assembly->thread_local_contexts, sizeof(struct assembly_thread_local_context) * thread_count);
	for (u32 i = 0; i < thread_count; ++i) {
		scope_thread_local_init(&assembly->thread_local_contexts[i].scope_thread_local, i);
		mir_arenas_init(&assembly->thread_local_contexts[i].mir_arenas, i);
		ast_arena_init(&assembly->thread_local_contexts[i].ast_arena, i);
		arena_init(&assembly->thread_local_contexts[i].small_array, SARR_TOTAL_SIZE, 16, 2048, i, (arena_elem_dtor_t)sarr_dtor);
	}
	return_zone();
}

static void thread_local_terminate(struct assembly *assembly) {
	zone();
	for (u32 i = 0; i < arrlenu(assembly->thread_local_contexts); ++i) {
		scope_thread_local_terminate(&assembly->thread_local_contexts[i].scope_thread_local);
		mir_arenas_terminate(&assembly->thread_local_contexts[i].mir_arenas);
		ast_arena_terminate(&assembly->thread_local_contexts[i].ast_arena);
		arena_terminate(&assembly->thread_local_contexts[i].small_array);
		scfree(&assembly->thread_local_contexts[i].string_cache);
	}

	arrfree(assembly->thread_local_contexts);
	return_zone();
}

struct assembly *assembly_new(const struct target *target) {
	bmagic_assert(target);
	struct assembly *assembly = bmalloc(sizeof(struct assembly));
	memset(assembly, 0, sizeof(struct assembly));
	assembly->target = target;

	mtx_init(&assembly->units_lock, mtx_plain);
	mtx_init(&assembly->modules_lock, mtx_plain);
	spl_init(&assembly->custom_linker_opt_lock);
	spl_init(&assembly->lib_paths_lock);
	spl_init(&assembly->libs_lock);

	llvm_init(assembly);
	arrsetcap(assembly->units, 64);
	str_buf_setcap(&assembly->custom_linker_opt, 128);
	vm_init(&assembly->vm, VM_STACK_SIZE);

	thread_local_init(assembly);

	const u32                  thread_index       = get_worker_index();
	struct scope_thread_local *scope_thread_local = &assembly->thread_local_contexts[thread_index].scope_thread_local;

	// set defaults
	assembly->gscope = scope_create(scope_thread_local, SCOPE_GLOBAL, NULL, NULL);
	scope_reserve(assembly->gscope, 256);

	dl_init(assembly);
	mir_init(assembly);

	if (assembly->target->kind != ASSEMBLY_DOCS) {
		assembly->module_scope = scope_create(scope_thread_local, SCOPE_MODULE, assembly->gscope, NULL);
		scope_reserve(assembly->module_scope, 8192);

		static struct id assembly_module_id;
		id_init(&assembly_module_id, cstr("__assembly_module"));
		struct scope_entry *scope_entry = scope_create_entry(scope_thread_local, SCOPE_ENTRY_NAMED_SCOPE, &assembly_module_id, NULL, false);
		scope_entry->data.scope         = assembly->module_scope;

		scope_insert(assembly->gscope, SCOPE_DEFAULT_LAYER, scope_entry);

		struct scope *unit_parent_scope = assembly->module_scope;

		// Add units from target
		for (usize i = 0; i < arrlenu(target->files); ++i) {
			assembly_add_unit(assembly, make_str_from_c(target->files[i]), NULL, unit_parent_scope, NULL);
		}
	} else {
		// Add units from target, for the documentation we create separate scope for each file to prevent
		// symbol collisions.
		for (usize i = 0; i < arrlenu(target->files); ++i) {
			struct scope *unit_parent_scope = scope_create(scope_thread_local, SCOPE_GLOBAL, assembly->gscope, NULL);
			scope_reserve(unit_parent_scope, 256);
			assembly_add_unit(assembly, make_str_from_c(target->files[i]), NULL, unit_parent_scope, NULL);
		}
	}

	const str_t preload_file = make_str_from_c(read_config(builder.config, assembly->target, "preload_file", ""));

	// Add default units based on assembly kind
	str_t default_units[5] = {0};
	s32   default_unit_num = 0;

	switch (assembly->target->kind) {
	case ASSEMBLY_EXECUTABLE:
		if (assembly->target->no_api) break;
		default_units[0] = cstr(BUILTIN_FILE);
		default_units[1] = preload_file;
		default_unit_num = 2;
		break;
	case ASSEMBLY_SHARED_LIB:
		if (assembly->target->no_api) break;
		default_units[0] = cstr(BUILTIN_FILE);
		default_units[1] = preload_file;
		default_unit_num = 2;
		break;
	case ASSEMBLY_BUILD_PIPELINE:
		default_units[0] = cstr(BUILTIN_FILE);
		default_units[1] = preload_file;
		default_units[2] = cstr(BUILD_API_FILE);
		default_units[3] = cstr(BUILD_SCRIPT_FILE);
		default_unit_num = 4;
		break;
	case ASSEMBLY_DOCS:
		break;
	}

	for (s32 i = 0; i < default_unit_num; ++i) {
		assembly_add_unit(assembly, default_units[i], NULL, assembly->gscope, NULL);
	}

	if (assembly->target->kind != ASSEMBLY_DOCS) {
		// Duplicate default library paths
		for (usize i = 0; i < arrlenu(target->default_lib_paths); ++i)
			assembly_add_lib_path(assembly, target->default_lib_paths[i]);

		// Duplicate default libs
		for (usize i = 0; i < arrlenu(target->default_libs); ++i)
			assembly_add_native_lib(assembly, target->default_libs[i], NULL, false);

		// Append custom linker options
		assembly_append_linker_options(assembly, str_buf_to_c(target->default_custom_linker_opt));
	}

	return assembly;
}

void assembly_delete(struct assembly *assembly) {
	zone();
	for (usize i = 0; i < arrlenu(assembly->units); ++i)
		unit_delete(assembly->units[i]);
	for (usize i = 0; i < arrlenu(assembly->libs); ++i)
		native_lib_terminate(&assembly->libs[i]);
	for (usize i = 0; i < arrlenu(assembly->lib_paths); ++i)
		free(assembly->lib_paths[i]);

	arrfree(assembly->libs);
	arrfree(assembly->lib_paths);
	arrfree(assembly->testing.cases);
	arrfree(assembly->units);

	mtx_destroy(&assembly->units_lock);
	mtx_destroy(&assembly->modules_lock);
	spl_destroy(&assembly->custom_linker_opt_lock);
	spl_destroy(&assembly->lib_paths_lock);
	spl_destroy(&assembly->libs_lock);

	str_buf_free(&assembly->custom_linker_opt);
	vm_terminate(&assembly->vm);
	llvm_terminate(assembly);
	dl_terminate(assembly);
	mir_terminate(assembly);
	thread_local_terminate(assembly);
	bfree(assembly);
	return_zone();
}

void assembly_add_lib_path(struct assembly *assembly, const char *path) {
	if (!path) return;
	char *tmp = strdup(path);
	if (!tmp) return;
	spl_lock(&assembly->lib_paths_lock);
	arrput(assembly->lib_paths, tmp);
	spl_unlock(&assembly->lib_paths_lock);
}

void assembly_append_linker_options(struct assembly *assembly, const char *opt) {
	if (!opt) return;
	if (opt[0] == '\0') return;

	spl_lock(&assembly->custom_linker_opt_lock);
	str_buf_append_fmt(&assembly->custom_linker_opt, "{s} ", opt);
	spl_unlock(&assembly->custom_linker_opt_lock);
}

static inline struct unit *lookup_unit(struct assembly *assembly, const hash_t hash, str_t filepath, struct scope *parent_scope) {
	for (usize i = 0; i < arrlenu(assembly->units); ++i) {
		struct unit *unit = assembly->units[i];
		if (hash == unit->hash && parent_scope == unit->parent_scope && str_match(filepath, unit->filepath)) {
			return unit;
		}
	}
	return NULL;
}

void assembly_add_unit(struct assembly *assembly, const str_t filepath, struct token *load_from, struct scope *parent_scope, struct module *module) {
	zone();
	bassert(filepath.len && filepath.ptr);
	bassert(parent_scope);
	struct unit *unit = NULL;

	str_buf_t    tmp_fullpath = get_tmp_str();
	struct unit *parent_unit  = load_from ? load_from->location.unit : NULL;
	if (!search_source_file(filepath, parent_unit ? parent_unit->dirpath : str_empty, &tmp_fullpath)) {
		put_tmp_str(tmp_fullpath);
		builder_msg(MSG_ERR, ERR_FILE_NOT_FOUND, TOKEN_OPTIONAL_LOCATION(load_from), CARET_WORD, "File not found '" STR_FMT "'.", STR_ARG(filepath));
		return_zone();
	}

	const hash_t hash = strhash(tmp_fullpath);

	bool submit = false;

	mtx_lock(&assembly->units_lock);
	unit = lookup_unit(assembly, hash, str_buf_view(tmp_fullpath), parent_scope);
	if (!unit) {
		unit = unit_new(assembly, str_buf_view(tmp_fullpath), filepath, hash, load_from, parent_scope, module);
		arrput(assembly->units, unit);

		submit = true;
	}
	mtx_unlock(&assembly->units_lock);
	if (submit) builder_submit_unit(assembly, unit);

	put_tmp_str(tmp_fullpath);
	return_zone();
}

void assembly_add_native_lib(struct assembly *assembly,
                             const char      *lib_name,
                             struct token    *link_token,
                             bool             runtime_only) {
	const u32 thread_index = get_worker_index();

	spl_lock(&assembly->libs_lock);
	const hash_t hash = strhash(make_str_from_c(lib_name));
	{ // Search for duplicity.
		for (usize i = 0; i < arrlenu(assembly->libs); ++i) {
			struct native_lib *lib = &assembly->libs[i];
			if (lib->hash == hash) goto DONE;
		}
	}
	struct native_lib lib = {0};
	lib.hash              = hash;
	lib.user_name         = scdup2(&assembly->thread_local_contexts[thread_index].string_cache, make_str_from_c(lib_name));
	lib.linked_from       = link_token;
	lib.runtime_only      = runtime_only;
	arrput(assembly->libs, lib);
DONE:
	spl_unlock(&assembly->libs_lock);
}

typedef struct {
	struct assembly *assembly;
	struct scope    *parent_scope;
	struct module   *module;

	str_t module_root_path;
	s32   is_supported_for_current_target;

	char target_triple_str[TRIPLE_MAX_LEN];
} import_elem_context_t;

static void import_source(import_elem_context_t *ctx, const char *srcfile) {
	str_buf_t path = get_tmp_str();
	str_buf_append_fmt(&path, "{str}/{s}", ctx->module_root_path, srcfile);
	assembly_add_unit(ctx->assembly, str_buf_view(path), NULL, ctx->parent_scope, ctx->module);
	put_tmp_str(path);
}

static void import_lib_path(import_elem_context_t *ctx, const char *dirpath) {
	str_buf_t path = get_tmp_str();
	str_buf_append_fmt(&path, "{str}/{s}", ctx->module_root_path, dirpath);
	if (!dir_exists(path)) {
		builder_msg(MSG_ERR, ERR_FILE_NOT_FOUND, NULL, CARET_NONE, "Cannot find module imported library path '" STR_FMT "'.", STR_ARG(path));
	} else {
		assembly_add_lib_path(ctx->assembly, str_buf_to_c(path));
	}
	put_tmp_str(path);
}

static void import_link(import_elem_context_t *ctx, const char *lib) {
	assembly_add_native_lib(ctx->assembly, lib, NULL, false);
}

static void validate_module_target(import_elem_context_t *ctx, const char *triple_str) {
	if (strcmp(ctx->target_triple_str, triple_str) == 0) ++ctx->is_supported_for_current_target;
}

static inline struct module *lookup_module(struct assembly *assembly, const hash_t hash, str_t modulepath) {
	for (usize i = 0; i < arrlenu(assembly->modules); ++i) {
		struct module *module = assembly->modules[i];
		if (hash == module->hash && str_match(modulepath, module->modulepath)) {
			return module;
		}
	}
	return NULL;
}

static struct module *import_module(struct assembly *assembly, str_t module_path, hash_t module_hash, struct token *import_from) {
	builder_log("Import module: '" STR_FMT "'", STR_ARG(module_path));
	bassert(mtx_trylock(&assembly->modules_lock) != thrd_success && "Unsafe import!");

	struct config *config = confload(module_path.ptr);
	if (!config) {
		builder_msg(MSG_ERR,
		            ERR_FILE_NOT_FOUND,
		            TOKEN_OPTIONAL_LOCATION(import_from),
		            CARET_WORD,
		            "Failed to load module configuration file '" STR_FMT "'.",
		            STR_ARG(module_path));
		return NULL;
	}

	const str_t module_root_dir = get_dir_from_filepath(module_path);
	const str_t module_name     = get_filename_from_filepath(module_root_dir);

	// I. Initialize module in assembly for later use...
	struct module *module = bmalloc(sizeof(struct module)); // @Performance 2024-09-14 Use arena?
	// bl_zeromem(module, sizeof(struct module)); // We set everything later...

	const u32                  thread_index       = get_worker_index();
	struct scope_thread_local *scope_thread_local = &assembly->thread_local_contexts[thread_index].scope_thread_local;
	struct string_cache      **string_cache       = &assembly->thread_local_contexts[thread_index].string_cache;

	module->scope      = scope_create(scope_thread_local, SCOPE_MODULE, assembly->gscope, NULL);
	module->modulepath = scdup2(string_cache, module_path);
	module->hash       = module_hash;

	// 2025-01-01: We might want to create module private scope only in cases it's used, but that would require some additional locking.
	module->private_scope = scope_create(scope_thread_local, SCOPE_MODULE_PRIVATE, module->scope, NULL);

	scope_reserve(module->scope, 1024);
	module->scope->name = scdup2(string_cache, module_name);

	arrput(assembly->modules, module);

	// II. Read global module options.

	import_elem_context_t ctx = {assembly, module->scope, module, module_root_dir};
	// @Performance 2024-08-02: We might want to cache this???
	target_triple_to_string(&assembly->target->triple, ctx.target_triple_str, static_arrlenu(ctx.target_triple_str));

	assembly_append_linker_options(assembly, confreads(config, "/linker_opt", ""));

	process_tokens(&ctx,
	               confreads(config, "/src", ""),
	               CONFIG_SEPARATOR,
	               (process_tokens_fn_t)&import_source);

	process_tokens(&ctx,
	               confreads(config, "/linker_lib_path", ""),
	               CONFIG_SEPARATOR,
	               (process_tokens_fn_t)&import_lib_path);

	process_tokens(&ctx, confreads(config, "/link", ""), CONFIG_SEPARATOR, (process_tokens_fn_t)&import_link);

	// This is optional configuration entry, this way we might limit supported platforms of this module. In case the
	// entry is missing from the config file, module is supposed to run anywhere.
	// Another option might be check presence of platform specific entries, but we might have modules requiring some
	// platform specific configuration only on some platforms, but still be fully functional on others...
	if (process_tokens(&ctx, confreads(config, "/supported", ""), ",", (process_tokens_fn_t)&validate_module_target)) {
		if (!ctx.is_supported_for_current_target) {
			builder_msg(MSG_ERR,
			            ERR_UNSUPPORTED_TARGET,
			            TOKEN_OPTIONAL_LOCATION(import_from),
			            CARET_WORD,
			            "Module is not supported for compilation target platform triple '%s'. "
			            "The module explicitly specifies supported platforms in 'supported' module configuration section. "
			            "Module directory might contain information about how to compile module dependencies for your target. "
			            "Module configuration is imported from '%s'.",
			            ctx.target_triple_str,
			            module_path.ptr);
		}
	}

	// III. Read target triple specific module options if any.
	assembly_append_linker_options(assembly,
	                               read_config(config, assembly->target, "linker_opt", ""));
	process_tokens(&ctx,
	               read_config(config, assembly->target, "src", ""),
	               CONFIG_SEPARATOR,
	               (process_tokens_fn_t)&import_source);
	process_tokens(&ctx,
	               read_config(config, assembly->target, "linker_lib_path", ""),
	               CONFIG_SEPARATOR,
	               (process_tokens_fn_t)&import_lib_path);
	process_tokens(&ctx,
	               read_config(config, assembly->target, "link", ""),
	               CONFIG_SEPARATOR,
	               (process_tokens_fn_t)&import_link);

	return module;
}

struct module *assembly_import_module(struct assembly *assembly,
                                      str_t            modulepath,
                                      struct token    *import_from,
                                      struct scope    *scope) {
	struct module *module = NULL;

	if (!modulepath.len) {
		builder_msg(MSG_ERR,
		            ERR_FILE_NOT_FOUND,
		            TOKEN_OPTIONAL_LOCATION(import_from),
		            CARET_WORD,
		            "Module name is empty.");
		return module;
	}

	str_buf_t module_config_path = get_tmp_str();
	bool      found              = false;

	const struct target *target = assembly->target;

	str_buf_t path = get_tmp_str();
	str_buf_append_fmt(&path, "{str}/{s}", modulepath, MODULE_CONFIG_FILE);
	found = search_source_file(str_buf_view(path), str_buf_view(target->module_dir), &module_config_path);
	put_tmp_str(path);

	if (!found) {
		builder_msg(MSG_ERR,
		            ERR_FILE_NOT_FOUND,
		            TOKEN_OPTIONAL_LOCATION(import_from),
		            CARET_WORD,
		            "Module not found.");
		goto DONE;
	}

	const hash_t module_hash = strhash(module_config_path);
	mtx_lock(&assembly->modules_lock);
	module = lookup_module(assembly, module_hash, str_buf_view(module_config_path));
	if (!module) {
		module = import_module(assembly, str_buf_view(module_config_path), module_hash, import_from);
	}
	mtx_unlock(&assembly->modules_lock);

DONE:
	put_tmp_str(module_config_path);
	return module;
}

DCpointer assembly_find_extern(struct assembly *assembly, const str_t symbol) {
	// We have to duplicate the symbol name to be sure it's zero terminated...
	str_buf_t tmp = get_tmp_str();
	str_buf_append(&tmp, symbol);

	void              *handle = NULL;
	struct native_lib *lib;
	for (usize i = 0; i < arrlenu(assembly->libs); ++i) {
		lib    = &assembly->libs[i];
		handle = dlFindSymbol(lib->handle, str_buf_to_c(tmp));
		if (handle) break;
	}

	put_tmp_str(tmp);
	return handle;
}
