#include "conf.h"
#include "builder.h"
#include "table.h"
#include <yaml.h>
#include "../deps/ini.h"

// =================================================================================================
// Generic configuration loading
// =================================================================================================
struct entry {
	hash_t      hash;
	const char *value;
};

struct config {
	hash_table(struct entry) data;
	struct string_cache *cache;
};

static struct config *load_from_yaml(const char *filepath) {
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
				snprintf(blockpath, static_arrlenu(blockpath), "/" STR_FMT, STR_ARG(key));
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

static struct config *load_from_ini(const char *filepath) {
	FILE *file = fopen(filepath, "rb");
	if (!file) {
		builder_msg(MSG_ERR, ERR_FILE_NOT_FOUND, NULL, CARET_WORD, "Cannot open config file: '%s'.", filepath);
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	usize fsize = (usize)ftell(file);
	if (fsize == 0) {
		fclose(file);
		builder_msg(MSG_ERR, ERR_FILE_EMPTY, NULL, CARET_WORD, "Invalid or empty config file '%s'.", filepath);
		return NULL;
	}

	fseek(file, 0, SEEK_SET);
	char *file_contents = bmalloc(fsize + 1);
	if (!fread(file_contents, sizeof(char), fsize, file)) {
		fclose(file);
		builder_msg(MSG_ERR, ERR_FILE_READ, NULL, CARET_WORD, "Cannot read config file '%s'.", filepath);
		return NULL;
	}
	file_contents[fsize] = '\0';
	fclose(file);

	char        *cursor = file_contents, *sector = NULL, *key = NULL, *value = NULL;
	struct entry entry = {0};
	str_buf_t    path  = get_tmp_str();

	struct config *conf = bmalloc(sizeof(struct config));
	conf->data          = NULL;
	conf->cache         = NULL;

	while (*cursor != '\0') {
		if (!parse_ini(&cursor, &sector, &key, &value)) {
			builder_msg(MSG_ERR, ERR_INVALID_TOKEN, NULL, CARET_WORD, "Syntax error when parsing config file: '%s'.", filepath);
			break;
		}

		if (sector) {
			str_buf_append_fmt(&path, "/{s}", sector);
		}

		if (key) {
			str_buf_append_fmt(&path, "/{s}", key);
		}

		if (value) {
			blog("Add: " STR_FMT " with value: %s", STR_ARG(path), value);

			entry.hash  = strhash(path);
			entry.value = scdup(&conf->cache, value, strlen(value));
			tbl_insert(conf->data, entry);
		}

		str_buf_clr(&path);
	}

	put_tmp_str(path);
	return conf;
}

struct config *confload(const char *filepath) {
	const str_t ext = get_file_extension(make_str_from_c(filepath));
	if (str_match(ext, cstr("yaml"))) {
		return load_from_yaml(filepath);
	}

	// ini is expected.
	return load_from_ini(filepath);
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
