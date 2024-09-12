// =================================================================================================
// bl
//
// File:   conf.c
// Author: Martin Dorazil
// Date:   12/19/21
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

#include "conf.h"
#include "builder.h"
#include "table.h"
#include <yaml.h>

// =================================================================================================
// Generic configuration loading
// =================================================================================================
struct entry {
	hash_t      hash;
	const char *value;
};

struct config {
	my_hash_table(struct entry) data;
	struct string_cache *cache;
};

struct config *confload(const char *filepath) {
	FILE *input = fopen(filepath, "rb");
	if (!input) {
		return NULL;
	}

	yaml_parser_t parser;
	yaml_parser_initialize(&parser);
	yaml_parser_set_input_file(&parser, input);

	struct config *conf = bmalloc(sizeof(struct config));
	conf->data          = NULL;
	conf->cache         = NULL;

	// insert special entry for filename
	struct entry entry = (struct entry){
	    .hash  = strhash(make_str_from_c(CONF_FILEPATH)),
	    .value = scdup(&conf->cache, filepath, strlen(filepath)),
	};

	tbl_insert(conf->data, entry);

	enum state {
		STATE_KEY,
		STATE_VALUE,
	} state = STATE_KEY;

	char blockpath[256] = "";

	yaml_token_t token;
	str_buf_t    key  = get_tmp_str();
	str_buf_t    path = get_tmp_str();

	bool done = false;
	while (!done) {
		if (!yaml_parser_scan(&parser, &token)) goto LOAD_ERROR;

		switch (token.type) {
		case YAML_STREAM_START_TOKEN:
			break;
		case YAML_KEY_TOKEN:
			state = STATE_KEY;
			break;
		case YAML_VALUE_TOKEN:
			state = STATE_VALUE;
			break;
		case YAML_BLOCK_MAPPING_START_TOKEN:
			if (key.len) {
				snprintf(blockpath, static_arrlenu(blockpath), "/%.*s", key.len, key.ptr);
			}
			break;
		case YAML_BLOCK_END_TOKEN:
			for (usize i = strlen(blockpath); i-- > 0;) {
				if (blockpath[i] == '/') {
					blockpath[i] = '\0';
					break;
				}
			}
			break;
		case YAML_SCALAR_TOKEN: {
			const char *value = (const char *)token.data.scalar.value;
			if (state == STATE_KEY) {
				str_buf_clr(&key);
				str_buf_append(&key, make_str_from_c(value));
			} else {
				str_buf_clr(&path);
				str_buf_append_fmt(&path, "{s}/{str}", blockpath, key);
				entry.hash  = strhash(path);
				entry.value = scdup(&conf->cache, value, token.data.scalar.length);

				tbl_insert(conf->data, entry);
			}
			break;
		}
		case YAML_STREAM_END_TOKEN:
			done = true;
			break;
		default:
			bwarn("%s:%d:%d: YAML Unknown token type!",
			      filepath,
			      token.start_mark.line,
			      token.start_mark.column);
			break;
		}
		yaml_token_delete(&token);
	}
	yaml_parser_delete(&parser);
	fclose(input);
	put_tmp_str(key);
	put_tmp_str(path);
	return conf;

LOAD_ERROR:
	yaml_parser_delete(&parser);
	fclose(input);
	confdelete(conf);
	put_tmp_str(key);
	put_tmp_str(path);
	return NULL;
}

void confdelete(struct config *conf) {
	if (!conf) return;
	tbl_free(conf->data);
	scfree(&conf->cache);
	bfree(conf);
}

const char *confreads(struct config *conf, const char *path, const char *default_value) {
	bassert(conf);
	const hash_t hash  = strhash(make_str_from_c(path));
	const s64    index = tbl_lookup_index(conf->data, hash);
	if (index == -1) {
		if (default_value) return default_value;
		babort("Unknown configuration entry '%s'!", path);
	}
	return conf->data[index].value;
}
