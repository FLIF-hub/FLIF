/*
 FLIF - Free Lossless Image Format
 Copyright (C) 2010-2015  Jon Sneyers & Pieter Wuille, LGPL v3+

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Lesser General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

void e_printf(const char *format, ...);
void v_printf(const int v, const char *format, ...);

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
