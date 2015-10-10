#pragma once

#include <stdio.h>

class FileIO
{
private:
    FILE *file;
    const char *name;
	
	// prevent copy
	FileIO(const FileIO&) {}
	void operator=(const FileIO&) {}
	// prevent move, for now
	FileIO(FileIO&&) {}
	void operator=(FileIO&&) {}
public:
    FileIO(FILE* fil, const char *aname) : file(fil), name(aname) { }
    ~FileIO() {
        if (file) fclose(file);
    }
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
    const char* getName() {
      return name;
    }
};
