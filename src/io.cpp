/*
FLIF - Free Lossless Image Format

Copyright 2010-2016, Jon Sneyers & Pieter Wuille

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include <stdio.h>
#include <stdarg.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "io.hpp"

void e_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    fflush(stderr);
    va_end(args);
}

#ifdef DEBUG
static int verbosity = 10;
#else
static int verbosity = 1;
#endif
static FILE * my_stdout = stdout;
void increase_verbosity(int how_much) {
    verbosity += how_much;
}

int get_verbosity() {
    return verbosity;
}

void v_printf(const int v, const char *format, ...) {
    if (verbosity < v) return;
    va_list args;
    va_start(args, format);
    vfprintf(my_stdout, format, args);
    fflush(my_stdout);
    va_end(args);
}

void v_printf_tty(const int v, const char *format, ...) {
    if (verbosity < v) return;
#ifdef _WIN32
    if(!_isatty(_fileno(my_stdout))) return;
#else
    if(!isatty(fileno(my_stdout))) return;
#endif
    va_list args;
    va_start(args, format);
    vfprintf(my_stdout, format, args);
    fflush(my_stdout);
    va_end(args);
}

void redirect_stdout_to_stderr() {
    my_stdout = stderr;
}
