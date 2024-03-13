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
