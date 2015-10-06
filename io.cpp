#include <stdio.h>
#include <stdarg.h>

#include "io.h"

void e_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fflush(stderr);
    va_end(args);
}

static int verbosity = 1;
void increase_verbosity() {
    verbosity ++;
}

int get_verbosity() {
    return verbosity;
}

void v_printf(const int v, const char *format, ...) {
    if (verbosity < v) return;
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    fflush(stdout);
    va_end(args);
}

