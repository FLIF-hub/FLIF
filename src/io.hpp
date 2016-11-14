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

#pragma once

void e_printf(const char *format, ...);
void v_printf(const int v, const char *format, ...);
void v_printf_tty(const int v, const char *format, ...);
void redirect_stdout_to_stderr();

void increase_verbosity(int how_much=1);
int get_verbosity();

template<class IO>
bool ioget_int_8bit (IO& io, int* result)
{
    int c = io.getc();
    if (c == io.EOS) {
        e_printf ("Unexpected EOS");
        return false;
    }

    *result = c;
    return true;
}

template<class IO>
bool ioget_int_16bit_bigendian (IO& io, int* result)
{
    int c1;
    int c2;
    if (!(ioget_int_8bit (io, &c1) &&
          ioget_int_8bit (io, &c2)))
        return false;

    *result = (c1 << 8) + c2;
    return true;
}
