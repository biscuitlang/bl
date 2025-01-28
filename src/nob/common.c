#define NOB_IMPLEMENTATION
#define NOB_STRIP_PREFIX
#include "nob.h"

#define STR_HELPER(x)           #x
#define STR(x)                  STR_HELPER(x)
#define VERSION_STRING(a, b, c) STR(a) "." STR(b) "." STR(c)
#define quote(x)                temp_sprintf("\"%s\"", (x))

#define run_shell_cmd(...)                    \
	do {                                      \
		Cmd cmd = {0};                        \
		cmd_append(&cmd, SHELL, __VA_ARGS__); \
		if (!cmd_run_sync(cmd)) exit(1);      \
		cmd_free(cmd);                        \
	} while (0)

#define wait(procs)                                  \
	do {                                             \
		bool success = true;                         \
		for (int i = 0; i < ARRAY_LEN(procs); ++i) { \
			success &= proc_wait(procs[i]);          \
		}                                            \
		if (!success) exit(1);                       \
	} while (0);

int IS_DEBUG = 0;

void build_dyncall(void);
void build_libyaml(void);
void build_blc(void);
void finalize(void);
void cleanup(void);

void print_help(void) {
	printf("Usage:\n\tbuild [options]\n\n");
	printf("Options:\n");
	printf("\trelease Build in release mode.\n");
	printf("\tdebug   Build in debug mode.\n");
	printf("\tclean   Remove build directory and exit.\n");
	printf("\thelp    Print this help and exit.\n");
}

void parse_command_line_arguments(int argc, char *argv[]) {
	shift_args(&argc, &argv);
	while (argc > 0) {
		char *arg = shift_args(&argc, &argv);
		if (strcmp(arg, "debug") == 0) {
			IS_DEBUG = 1;
		} else if (strcmp(arg, "release") == 0) {
			IS_DEBUG = 0; // by default
		} else if (strcmp(arg, "clean") == 0) {
			// run_shell_cmd("RD", "/S", "/Q", quote(BUILD_DIR));
			cleanup();
			exit(0);
		} else if (strcmp(arg, "help") == 0) {
			print_help();
			exit(0);
		} else {
			nob_log(NOB_ERROR, "Invalid argument '%s'.", arg);
			print_help();
			exit(1);
		}
	}
}