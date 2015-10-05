#ifndef __FILEIO_H__
#define __FILEIO_H__

#include <stdio.h>

class FileIO
{
private:
    FILE *file;
    const char *name;
public:
    FileIO(FILE* fil, const char *aname) : file(fil), name(aname) { }
    int read() {
        int r = fgetc(file);
        if (r < 0) return 0;
        return r;
    }
    void write(int byte) {
        ::fputc(byte, file);
    }
    void flush() {
        fflush(file);
    }
    bool isEOF() {
      return feof(file);
    }
    int ftell() {
      return ::ftell(file);
    }
    int getc() {
      return fgetc(file);
    }
    char * gets(char *buf, int n) {
      return fgets(buf, n, file);
    }
    int fputs(const char *s) {
      return ::fputs(s, file);
    }
    int fputc(int c) {
      return ::fputc(c, file);
    }
    void close() {
      fclose(file);
      file = NULL;
    }
    const char* getName() {
      return name;
    }
};

#endif
