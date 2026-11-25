#pragma once
// (C) Manuel Riecke - 2025, Public Domain.
// Modified to handle Windows line endings ("\r\n").
// ---------------------------
// Standard INI Parser: 20 LOC
// ---------------------------

static inline int _ini_is_blank(char c) {
	return c == ' ' || c == '\t' || c == '\r';
}

static inline int _ini_capture_to(char **cur, char to, char **target) {
	while (_ini_is_blank(**cur))
		(*cur)++;
	*target = *cur;
	while (1) {
		if (**cur == to) {
			char *end = (*cur) - 1;
			while (_ini_is_blank(*end) && end > *target)
				end--;
			end++;
			*end = '\0';
			(*cur)++;
			return **target != '\0';
		}
		if (**cur == '\n' || **cur == '\0') {
			return 0;
		}
		(*cur)++;
	}
}

static inline void _ini_strip_string_quotation(char **str) {
	char *cur = *str;
	if (**str == '"') {
		while (*(++cur) != '\0')
			;
		if (*(cur - 1) == '"') {
			*(cur - 1) = '\0';
			(*str)++;
		}
	}
}

// Returns 0 if the file unexpectedly ends or if a key, sector or value is empty.
static inline int parse_ini(char **cur, char **sector, char **key, char **value) {
	*key   = 0;
	*value = 0;
	while (_ini_is_blank(**cur) || **cur == '\n')
		(*cur)++;
	if (**cur == '\0') return 1; // End of file.
	if (**cur == ';' || **cur == '#') {
		while (**cur != '\n' && **cur != '\0')
			(*cur)++;
		return parse_ini(cur, sector, key, value);
	} // Skip over comments
	if (**cur == '[') {
		(*cur)++;
		if (_ini_capture_to(cur, ']', sector)) return 1;
		sector = 0;
		return 0;
	}
	if (_ini_capture_to(cur, '=', key)) {
		_ini_capture_to(cur, '\n', value);
		_ini_strip_string_quotation(value);
		return 1;
	}
	return 0;
}
