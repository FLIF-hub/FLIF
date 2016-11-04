#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <vector>

#include "image.hpp"

#ifdef HAS_ENCODER


bool image_load_metadata(const char *filename, Image& image, const char *chunkname) {
    FILE *fp = fopen(filename,"rb");
    if (!fp) {
        e_printf("Could not open file: %s\n", filename);
        return false;
    }
    image.init(0, 0, 0, 0, 0);

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    std::vector<unsigned char> contents(fsize + 1);
    if (!fread(contents.data(), fsize, 1, fp)) {
        e_printf("Could not read file: %s\n", filename);
        fclose(fp);
        return false;
    }
    fclose(fp);
    image.set_metadata(chunkname, contents.data(), fsize);
    return true;
}
#endif

bool image_save_metadata(const char *filename, const Image& image, const char *chunkname) {
    unsigned char * contents;
    size_t length;
    if (image.get_metadata(chunkname, &contents, &length)) {
      FILE *fp = fopen(filename,"wb");
      if (!fp) {
        return false;
      }
      fwrite((void *) contents, length, 1, fp);
      fclose(fp);
      free(contents);
      return true;
    } else {
      e_printf("Asking to write metadata of type %s to file %s, however no such metadata is present in the input file.\n", chunkname, filename);
      return false;
    }
}
