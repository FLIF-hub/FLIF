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
    void fseek(long offset, int where) {
      ::fseek(file, offset,where);
    }
    const char* getName() {
      return name;
    }
    
    FILE* getFileHandle() {
        return file;
    }
};


class BlobIO
{
private:
    FILE *file;
    const char *name;
    
    char* flifBlob;
    uint64_t memoryPos;
    size_t fSize;

    
    // prevent copy
    BlobIO(const BlobIO&) {}
    void operator=(const BlobIO&) {}
    // prevent move, for now
    BlobIO(BlobIO&&) {}
    void operator=(BlobIO&&) {}
public:
    BlobIO(FILE* fil, const char *aname){
        readFlifFromFile(fil, aname);
		closeFile();
    }
    
    BlobIO(FileIO *fio) {
        file = fio->getFileHandle();
        name = fio->getName();
        readFlifFromFile(file, name);
    }
    
    BlobIO() {}
    ~BlobIO() {
        if (flifBlob == NULL) delete [] flifBlob;
    }
    
    void readFlifFromFile(FILE* fil, const char *aname) {
        file = fil;
        name = aname;
        ::fseek(file, 0L, SEEK_END);
        fSize = ::ftell(file);
        ::fseek(file, 0L, SEEK_SET);
        
        flifBlob = new char[fSize];
        
        ::fread(flifBlob, fSize, 1, file);
        memoryPos = 0;
    }
	
	void closeFile() {
		fclose(file);
		file = NULL;
	}
	
    void setFlifBlob(char* blobFlif, size_t size) {
        flifBlob = blobFlif;
        fSize = size;
        memoryPos = 0;
    }
    
    size_t getFlifSize() {
        return fSize;
    }
    
    const char* getFlifBlob() {
        return flifBlob;
    }
    
    void fseek (long offset, int whence) {
        switch (whence) {
            case SEEK_CUR:
                memoryPos += offset;
                break;
            case SEEK_END:
                memoryPos = fSize += offset;
                break;
            case SEEK_SET:
                memoryPos = offset;
                break;
                
            default:
                break;
        }
        if (memoryPos > fSize) {
            memoryPos = fSize-1;
        }
    }
    
    int read() {
        int r = getc();
        if (r == EOF) return 0;
        return r;
    }
    
    void write(int byte) {
//        ::fputc(byte, file);
    }
    void flush() {
//        fflush(file);
    }
    bool isEOF() {
        return (memoryPos >= fSize); //not sure if >= or >
    }
    uint64_t ftell() {
        return memoryPos;
    }

    unsigned char getc() {
        if (isEOF()) {
            return EOF;
        }
        unsigned char rVal = flifBlob[memoryPos];
        memoryPos ++;
    
        return rVal;
    }
    
    char * gets(char *buf, int n) {
        if (memoryPos + n < fSize) {
            memcpy(buf, flifBlob, n-1);
            buf[n - 1] = '\0';
            memoryPos += n - 1;
        } else {
            return NULL;
        }
        return buf;
    }
    
    int fputs(const char *s) {
        return EOF;
//        return ::fputs(s, file);
    }
    int fputc(int c) {
        return EOF;
//        return ::fputc(c, file);
    }
    const char* getName() {
        return name;
    }
    
    void setName(char* pName) {
        name = pName;
    }

};
