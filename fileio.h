#pragma once

#include <stdio.h>
#include <string>

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
    std::string name;
    
	std::vector<char>blob;
    uint64_t memoryPos;

    
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
    
    BlobIO(FileIO *fio) { //used only for testing - will remove when there's a way to get blobs elsewhere
        file = fio->getFileHandle();
        name = fio->getName();
        readFlifFromFile(file, name.c_str());
    }
    
    BlobIO(const char* aname) {
        name = aname;
        initializeVars();
    }
    
    BlobIO() {
        initializeVars();
    }
    ~BlobIO() {
    }
    
    void initializeVars() {
        memoryPos = 0;
    }
    
    void readFlifFromFile(FILE* fil, const char *aname) { //used only for testing - will remove when there's a way to get blobs elsewhere
        file = fil;
        name = aname;
        ::fseek(file, 0L, SEEK_END);
        size_t fSize = ::ftell(file);
        ::fseek(file, 0L, SEEK_SET);
        
        for (size_t i = 0; i < fSize; i++) {
            blob.push_back(fgetc(file));
        }
        initializeVars();
    }
	
	void closeFile() {
		fclose(file);
		file = NULL;
	}
	
    void setBlob(char* blobFlif, size_t size) {
        for (size_t i = 0; i < size; i++) {
            blob.push_back(blobFlif[i]);
        }
        memoryPos = 0;
    }
    
    /**
     Sets the blob to the vector passed. This actually moves the vector to belong to the BlobIO class.
     @code
        std::vector<char> outsideVector = {'a', 'b', 'c'};
        BlobIO blobIO;
        blobIO.setBlob(std::move(outsideVector));
     @endcode
     @param pBlob
     Input BLOB
     */
    void setBlob(std::vector<char>&& pBlob) {
        blob = std::move(pBlob);
    }
    
    size_t getFlifSize() {
        return blob.size();
    }
    
    const char* getCharBlob() {
        char* flifBlob = new char[blob.size()];
        for (size_t i = 0; i < blob.size(); i++) {
            flifBlob[i] = blob[i];
        }
        return flifBlob;
    }
    
    std::vector<char>& getVectorBlob() {
        return blob;
    }
    
    void fseek (long offset, int whence) {
        size_t fSize = blob.size();
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
        fputc(byte);
    }
    void flush() {
//        fflush(file); //// I don't think this would really have an equivalent for writing to memory, since it writes directly to memory in the first place?
    }
    bool isEOF() {
        return (memoryPos >= blob.size() ); //not sure if >= or >
    }
    uint64_t ftell() {
        return memoryPos;
    }

    unsigned char getc() {
        if (isEOF()) {
            return EOF;
        }
        unsigned char rVal = blob[memoryPos];
        memoryPos ++;
    
        return rVal;
    }
    
    char * gets(char *buf, int n) {
        if (memoryPos + n < blob.size()) {
            for (size_t i = 0; i < (n-1); i++) {
                buf[i] = blob[memoryPos + i];
            }
            buf[n - 1] = '\0';
            memoryPos += n - 1;
        } else {
            return NULL;
        }
        return buf;
    }
    
    int fputs(const char *s) {
        size_t count = strlen(s);
        for (size_t i = 0; i < count; i ++) {
            blob.push_back(s[i]);
        }
        memoryPos += count;
        return (int)count;
    }
    int fputc(int c) {
        unsigned char tBuffer = c;
        blob.push_back(tBuffer);
        memoryPos++;
        return tBuffer;
    }
    const char* getName() {
        return name.c_str();
    }
    
    void setName(char* pName) {
        name = pName;
    }
    
    void saveToFile() { //used only for testing - will remove when there's a way to send blobs elsewhere
        if (name.empty()) {
            name = "newFile";
        }
        
        FILE* newFile = fopen(name.c_str(),"wb");
        for (size_t i = 0; i < blob.size(); i++) {
            ::fputc(blob[i], newFile);
        }
        fclose(newFile);
    }

};
