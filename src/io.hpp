#pragma once

void e_printf(const char *format, ...);
void v_printf(const int v, const char *format, ...);

void increase_verbosity();
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
