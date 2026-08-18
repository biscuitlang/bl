// Those methods are used only during compilation pass to setup builder pipeline. No C interfaces
// are needed here.

#include "builder.h"
#include "stb_ds.h"

// @Cleanup 2025-05-21: All functions taking strings should use ptr + len interface style!

BL_EXPORT struct target *__add_target(const char *name, enum assembly_kind kind) {
	struct target *target = builder_add_target(name);
	target->kind          = kind;
	return target;
}

BL_EXPORT void __add_unit(struct target *target, const char *filepath) {
	target_add_file(target, filepath);
}

BL_EXPORT void __add_lib_path(struct target *target, const char *path) {
	target_add_lib_path(target, path);
}

BL_EXPORT void __add_bool_user_define(
    struct target *target,
    const char    *name,
    s32            name_len,
    bool           value,
    struct ast    *node) {
	bassert(name_len);
	str_t sym_name = make_str(name, name_len);
	target_add_bool_user_define(target, node, sym_name, value);
}

BL_EXPORT void __add_int_user_define(
    struct target *target,
    const char    *name,
    s32            name_len,
    s32            value,
    struct ast    *node) {
	bassert(name_len);
	str_t sym_name = make_str(name, name_len);
	target_add_int_user_define(target, node, sym_name, value);
}

BL_EXPORT s32 __compile(struct target *target) {
	return builder_compile(target);
}

BL_EXPORT s32 __compile_all(void) {
	return builder_compile_all();
}

BL_EXPORT void __link_library(struct target *target, const char *name) {
	target_add_lib(target, name);
}

BL_EXPORT void __append_linker_options(struct target *target, const char *opt) {
	target_append_linker_options(target, opt);
}

BL_EXPORT void __set_output_dir(struct target *target, const char *dir) {
	target_set_output_dir(target, dir);
}

BL_EXPORT const char *__get_output_dir(struct target *target) {
	bmagic_assert(target);
	return target->out_dir.len > 0 ? str_buf_to_c(target->out_dir) : NULL;
}

BL_EXPORT void __set_module_dir(struct target *target, const char *dir) {
	target_set_module_dir(target, dir);
}

BL_EXPORT const char *__get_module_dir(struct target *target) {
	bmagic_assert(target);
	return target->module_dir.len > 0 ? str_buf_to_c(target->module_dir) : NULL;
}

BL_EXPORT const char *__get_default_module_dir(struct target *target) {
	return builder_get_lib_dir_cstr();
}

BL_EXPORT void __get_default_triple(struct target_triple *triple) {
	target_init_default_triple(triple);
}

BL_EXPORT s32 __triple_to_string(struct target_triple *triple, char *buf, s32 buf_len) {
	return target_triple_to_string(triple, buf, buf_len);
}

BL_EXPORT void __builder_get_options(struct builder_options *opt) {
	memcpy(opt, builder.options, sizeof(struct builder_options));
}

BL_EXPORT void __builder_set_options(struct builder_options *opt) {
	memcpy(builder.options, opt, sizeof(struct builder_options));
}

// @Incomplete 2024-12-16: We miss API to import modules!!!!