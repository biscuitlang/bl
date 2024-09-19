// =================================================================================================
// bl
//
// File:   file_loader.c
// Author: Martin Dorazil
// Date:   04/02/2018
//
// Copyright 2018 Martin Dorazil
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

#include "builder.h"
#include <stdio.h>
#include <string.h>

#if BL_PLATFORM_WIN
void file_loader_run(struct assembly *UNUSED(assembly), struct unit *unit) {
	char error_buf[256];
	zone();
	const str_t path = unit->filepath;
	bassert(path.len);

	str_buf_t tmp_path = get_tmp_str();
	HANDLE    f        = CreateFileA(str_dup_if_not_terminated(&tmp_path, path.ptr, path.len), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	put_tmp_str(tmp_path);

	if (f == INVALID_HANDLE_VALUE) {
		get_last_error(error_buf, static_arrlenu(error_buf));
		builder_msg(MSG_ERR, ERR_FILE_NOT_FOUND, TOKEN_OPTIONAL_LOCATION(unit->loaded_from), CARET_WORD, "Cannot open file '" STR_FMT "': %s", STR_ARG(path), error_buf);
		return_zone();
	}

	DWORD bytes = GetFileSize(f, NULL);
	if (bytes == INVALID_FILE_SIZE) {
		CloseHandle(f);
		builder_msg(MSG_ERR, ERR_FILE_NOT_FOUND, TOKEN_OPTIONAL_LOCATION(unit->loaded_from), CARET_WORD, "Cannot get size of file '" STR_FMT "'.", STR_ARG(path));
		return_zone();
	}
	char *data = bmalloc(bytes + 1);
	DWORD rbytes;
	if (!ReadFile(f, data, bytes, &rbytes, NULL)) {
		bfree(data);
		CloseHandle(f);
		get_last_error(error_buf, static_arrlenu(error_buf));
		builder_msg(MSG_ERR, ERR_FILE_NOT_FOUND, TOKEN_OPTIONAL_LOCATION(unit->loaded_from), CARET_WORD, "Cannot read file '" STR_FMT "': %s", STR_ARG(path), error_buf);
		return_zone();
	}
	bassert(rbytes == bytes);
	data[rbytes] = '\0';
	CloseHandle(f);
	unit->src = data;
	return_zone();
}
#else
void file_loader_run(struct assembly *UNUSED(assembly), struct unit *unit) {
	zone();
	const str_t path = unit->filepath;
	bassert(path.len);

	str_buf_t tmp_path = get_tmp_str();
	FILE     *f        = fopen(str_dup_if_not_terminated(&tmp_path, path.ptr, path.len), "rb");
	put_tmp_str(tmp_path);

	if (f == NULL) {
		builder_msg(MSG_ERR, ERR_FILE_READ, TOKEN_OPTIONAL_LOCATION(unit->loaded_from), CARET_WORD, "Cannot read file '" STR_FMT "'.", STR_ARG(path));
		return_zone();
	}

	fseek(f, 0, SEEK_END);
	usize fsize = (usize)ftell(f);
	if (fsize == 0) {
		fclose(f);
		builder_msg(MSG_ERR, ERR_FILE_EMPTY, TOKEN_OPTIONAL_LOCATION(unit->loaded_from), CARET_WORD, "Invalid or empty source file '" STR_FMT "'.", STR_ARG(path));
		return_zone();
	}
	fseek(f, 0, SEEK_SET);

	char *src = bmalloc(fsize + 1);
	if (!fread(src, sizeof(char), fsize, f)) babort("Cannot read file '" STR_FMT "'.", STR_ARG(path));
	src[fsize] = '\0';
	fclose(f);
	unit->src = src;
	return_zone();
}
#endif
