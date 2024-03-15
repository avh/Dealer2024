// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include <esp_camera.h>
#include "image.h"

Image::Image() : data(NULL), width(0), height(0), stride(0), owned(false)
{
}

Image::Image(const Image &other) : data(other.data), width(other.width), height(other.height), stride(other.stride), owned(false)
{
}

Image::Image(int width, int height) : width(width), height(height), stride(width), owned(true)
{
  data = (pixel *)malloc(width * height);
}

Image::Image(pixel *data, int width, int height, int stride) : data(data), width(width), height(height), stride(stride), owned(false)
{
}

bool Image::init(int width, int height)
{
  if (owned && data != NULL) {
    if (this->width == width && this->height == height) {
      return true;
    }
    ::free(data);
  }
  this->data = (pixel *)malloc(width * height * sizeof(pixel));
  if (data == NULL) {
    dprintf("WARNING: failed to allocate %d bytes for %dx%d image", width * height * sizeof(pixel), width, height);
  }
  this->width = width;
  this->height = height;
  this->stride = width;
  this->owned = true;
  //dprintf("allocated %p -> %dx%d", this->data, this->width, this->height);
  return data != NULL;
}

Image Image::crop(int x, int y, int w, int h)
{
  x = max(0, min(x, width));
  y = max(0, min(y, height));
  w = min(w, width - x);
  h = min(h, height - y);
  return Image(addr(x, y), w, h, stride);
}

void Image::copy(int x, int y, const Image &src)
{
  x = max(0, min(x, width));
  y = max(0, min(y, height));
  int w = min(src.width, width - x);
  int h = min(src.height, height - y);

  const pixel *sp = src.data;
  pixel *dp = addr(x, y);
  for (int r = 0 ; r < h ; r++, sp += src.stride, dp += stride) {
    bcopy(sp, dp, w);
  }
}

void Image::clear()
{
  for (int r = 0 ; r < height ; r++) {
    memset(data + r*stride, 0, width);
  }
}

void Image::debug(const char *msg)
{
  if (data != NULL) {
    dprintf("%s[%p,%dx%d,%d%s]",msg, data, width, height, stride, owned ? ",owned" : "");
  } else {
    dprintf("%s (not allocated)", msg);
  }
}

int Image::save(const char *fname)
{
    camera_fb_t fb;
    fb.buf = data;
    fb.len = stride * width;
    fb.width = width;
    fb.height = height;
    fb.format = PIXFORMAT_GRAYSCALE;

    File file = SD.open(fname, FILE_WRITE);
    if (!file) {
        dprintf("error: failed to open for write: %s", fname);
        return 1;
    }
    dprintf("writing %s", fname);
    frame2jpg_cb(&fb, 80, [](void *arg, size_t index, const void *data, size_t len) -> unsigned int {
        File *file = (File *)arg;
        return file->write((unsigned char *)data, len);
    }, &file); 
    dprintf("done %s", fname);
    file.close();
    return 0;
}

void Image::free()
{
  if (owned) {
    ::free(data);
    data = NULL;
  }
}

Image::~Image()
{
  Image::free();
}

void image_init()
{
}

//
// Algorithms
//


bool Image::locate(Image &tmp, Image &card, Image &suit)
{ 
  int n = width;
  std::unique_ptr<int[]> sums(new int[n]);
  bzero((void *)sums.get(), n*sizeof(int));

  // determine horizontal position
  for (int c = 0 ; c < width ; c++) {
    int sum = 0;
    pixel *p = data + c;
    for (int i = 0 ; i < height ; i++, p += stride) {
      sum += *p;
    }
    sums[c] = sum;
  }

  // smooth and locate horizontally
  int bestx = 0;
  int x = 0;
  int *p = sums.get();
  for (int c = 1 ; c < width - CARDSUIT_WIDTH - 1; c++, p++) {
    int sum = p[0] + p[1]*4 + p[2] + p[CARDSUIT_WIDTH + 0] + p[CARDSUIT_WIDTH + 1]*4 + p[CARDSUIT_WIDTH + 2];
    if (bestx < sum) {
      bestx = sum;
      x = c;
    }
  }

  // adjust brightness per line into tmp
  tmp.init(CARDSUIT_WIDTH, height);
  for (int r = 0 ; r < tmp.height ; r++) {
    pixel *src = data + x + r*stride;
    pixel *dst = tmp.data + r*tmp.stride;
    int pmin = src[0];
    int pmax = pmin;
    for (int c = 1 ; c < CARDSUIT_WIDTH ; c++) {
      pmin = min(pmin, src[c]);
      pmax = max(pmax, src[c]);
    }
    //dprintf("%4d: contrast pmin=%d, pmax=%d", r, pmin, pmax);
    //int pmax = 250 - (r * 17)/10;
    //int pmin = 150 - (r * 125)/100;
    int offset = (pmin < pmax-40) ? pmin : 0;
    int scale = pmax - offset;
    //int offset = 0;
    //int scale = 255;
    //dprintf("%d: pmin=%d, pmax=%d, offset=%d, scale=%d", r, pmin, pmax, offset, scale);
    for (int c = 0 ; c < CARDSUIT_WIDTH ; c++) {
      dst[c] = max(0, min((src[c] - offset) * 255 / scale, 255));
    }
  }

  // determine vertical location
  int ycard = tmp.vlocate(25, 50);
  int ysuit = tmp.vlocate(ycard + SUIT_OFFSET - 1, ycard + SUIT_OFFSET + 10);
  //dprintf("locate X=%d, YC=%d, YS=%d", x, ycard, ysuit);
  card = tmp.crop(0, ycard, CARD_WIDTH, CARD_HEIGHT);
  suit = tmp.crop(0, ysuit, SUIT_WIDTH, SUIT_HEIGHT);
  return true;
}

int Image::vlocate(int ymin, int ymax)
{
  ymin = max(0, min(ymin, height));
  ymax = max(0, min(ymax, height));
  for (int y = ymin ; y < ymax ; y++) {
    pixel *src = addr(0, y);
    int pmin = *src++;
    int pmax = pmin;
    for (int x = 1 ; x < width ; x++, src++) {
      pmin = min(pmin, *src);
      pmax = max(pmax, *src);
    }
    //dprintf("%3d: ymin=%d, ymax=%d", y, pmin, pmax);
    if (pmax - pmin > 100) {
      return max(ymin, y-1);
    }
  }
  return ymin;
}

int Image::match(const Image &img)
{
  int n = width / img.width;
  int bestd = 999999999;
  int bestm = -1;
  for (int i = 0 ; i < n ; i++) {
    int dist = 0;
    for (int r = 0 ; r < img.height ; r++) {
      const pixel *p1 = addr(i*img.width, r);
      const pixel *p2 = img.addr(0, r);
      for (int c = 0 ; c < img.width ; c++, p1++, p2++) {
        int d = *p1 - *p2;
        dist += d*d;
      }
    }
    //dprintf("match %d = %d", i, dist);
    if (bestd > dist) {
      bestd = dist;
      bestm = i;
    }
  }
  //dprintf("bestm=%d, bestd=%d", bestm, bestd);
  return bestm;
}
