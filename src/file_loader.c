#include "builder.h"
#include <stdio.h>
#include <string.h>

static FILE *get_file_handle(const str_t filepath) {
	if (str_match(filepath, cstr(STDIN_FILEPATH))) {
		return stdin;
	}

	str_buf_t tmp_path = get_tmp_str();
	FILE     *file     = fopen(str_dup_if_not_terminated(&tmp_path, filepath.ptr, filepath.len), "rb");
	put_tmp_str(tmp_path);

	return file;
}

void file_loader_run(struct assembly *UNUSED(assembly), struct unit *unit) {
	zone();
	const str_t path = unit->filepath;
	bassert(path.len);

	FILE *file = get_file_handle(path);

	if (file == NULL) {
		builder_msg(MSG_ERR, ERR_FILE_READ, TOKEN_OPTIONAL_LOCATION(unit->loaded_from), CARET_WORD, "Cannot read file '" STR_FMT "'.", STR_ARG(path));
		return_zone();
	}

	fseek(file, 0, SEEK_END);
	usize fsize = (usize)ftell(file);
	if (fsize == 0) {
		fclose(file);
		builder_msg(MSG_ERR, ERR_FILE_EMPTY, TOKEN_OPTIONAL_LOCATION(unit->loaded_from), CARET_WORD, "Invalid or empty source file '" STR_FMT "'.", STR_ARG(path));
		return_zone();
	}
	fseek(file, 0, SEEK_SET);

	char *src = bmalloc(fsize + 1);
	if (!fread(src, sizeof(char), fsize, file)) {
		fclose(file);
		builder_msg(MSG_ERR, ERR_FILE_EMPTY, TOKEN_OPTIONAL_LOCATION(unit->loaded_from), CARET_WORD, "Invalid or source file '" STR_FMT "'.", STR_ARG(path));
		return_zone();
	}
	src[fsize] = '\0';
	fclose(file);
	unit->src = src;

	return_zone();
}
