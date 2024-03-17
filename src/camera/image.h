// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include <FS.h>
#include <SD.h>
#include "util.h"

#define CARDSUIT_NCOLS    13
#define CARDSUIT_NROWS    4

#define CARDSUIT_WIDTH        38
#define CARDSUIT_HEIGHT       100
#define CARD_WIDTH            CARDSUIT_WIDTH
#define CARD_HEIGHT           54
#define SUIT_WIDTH            CARDSUIT_WIDTH
#define SUIT_HEIGHT           44
#define SUIT_OFFSET           (CARDSUIT_HEIGHT - SUIT_HEIGHT)

#define CARD_MATCH_NONE       -1
#define CARD_MATCH_EMPTY      (4*13)

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
    bool locate(Image &tmp, Image &card, Image &suit);
    int match(const Image &img);

    void send(class HTTP& http);
    int save(const char *fname);
    void free();
    ~Image();

  private:
    int hlocate(int xmin, int xmax, int w);
    int vlocate(int ymin, int ymax, int h);
};
