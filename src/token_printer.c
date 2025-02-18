#include "builder.h"
#include "stb_ds.h"

void token_printer_run(struct assembly *UNUSED(assembly), struct unit *unit) {
	struct token *tokens_arr = unit->tokens.buf;
	fprintf(stdout, "Tokens: \n");
	struct token *tok;
	s32           line = -1;
	for (usize i = 0; i < arrlenu(tokens_arr); ++i) {
		tok = &tokens_arr[i];

		if (line == -1) {
			line = tok->location.line;
			fprintf(stdout, "%d: ", line);
		} else if (tok->location.line != line) {
			line = tok->location.line;
			fprintf(stdout, "\n%d: ", line);
		}
		fprintf(
		    stdout, "['%s' %i:%i], ", sym_strings[tok->sym], tok->location.line, tok->location.col);
	}
	fprintf(stdout, "\n");
}
