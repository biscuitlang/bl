#ifndef BL_MIR_PRINTER_H
#define BL_MIR_PRINTER_H

#include <stdio.h>

struct assembly;
struct mir_instr;
struct mir_fn;

void mir_print_instr(FILE *stream, struct assembly *assembly, struct mir_instr *instr);
void mir_print_fn(FILE *stream, struct assembly *assembly, struct mir_fn *fn);
void mir_print_assembly(FILE *stream, struct assembly *assembly);

#endif
