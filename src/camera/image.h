// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include <FS.h>
#include <SD.h>
#include "util.h"

typedef unsigned char pixel;

class Image {
  public:
    pixel *data;
    int width;
    int height;
    int stride;
    bool owned;
    public:
    Image();
    Image(const Image &other);
    Image(int width, int height);
    Image(pixel *data, int width, int height, int stride);
    bool init(int width, int height);
    Image crop(int x, int y, int w, int h);

    inline pixel *addr(int x, int y) {
        return data + x + y * stride;
    }
    inline const pixel *addr(int x, int y) const {
        return data + x + y * stride;
    }

    void copy(int x, int y, const Image &src);
    void clear();
    void debug(const char *msg);

    //
    // card/suit specific operations
    //
    //bool locate(Image &tmp, Image &card, Image &suit);
    //int match(const Image &img);

    int save(const char *fname);
    void free();
    ~Image();

  private:
  //int vlocate(int ymin, int ymax);
};
