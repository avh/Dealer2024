// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include <esp_camera.h>
#include <JPEGDecoder.h>
#include "image.h"
#include "webserver.h"

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
      bzero(data, width * height);
      return true;
    }
    ::free(data);
  }
  this->data = (pixel *)malloc(width * height * sizeof(pixel));
  if (data == NULL) {
    dprintf("WARNING: failed to allocate %d bytes for %dx%d image", width * height * sizeof(pixel), width, height);
  } else {
    bzero(data, width * height);
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

    unsigned long tm = millis();
    File file = LittleFS.open(fname, FILE_WRITE);
    if (!file) {
        dprintf("error: failed to open for write: %s", fname);
        return 1;
    }
    frame2jpg_cb(&fb, 80, [](void *arg, size_t index, const void *data, size_t len) -> unsigned int {
        File *file = (File *)arg;
        return file->write((unsigned char *)data, len);
    }, &file); 
    dprintf("saved %s in %lums", fname, millis() - tm);
    file.close();
    return 0;
}

void Image::send(HTTP &http)
{
    if (data == NULL) {
      http.header(404, "Image Not Initialized");
      http.close();
      return;
    }
    camera_fb_t fb;
    fb.buf = data;
    fb.len = stride * width;
    fb.width = width;
    fb.height = height;
    fb.format = PIXFORMAT_GRAYSCALE;

    http.header(200, "File Follows");
    http.printf("Content-Type: image/jpeg\n");
    http.body();
    dprintf("sending %s", http.path.c_str());
    frame2jpg_cb(&fb, 80, [](void *arg, size_t index, const void *data, size_t len) -> unsigned int {
        HTTP *http = (HTTP *)arg;
        return http->write((unsigned char *)data, len);
    }, &http); 
    dprintf("done %s", http.path.c_str());
    http.close();
}

bool Image::load(const char *fname) {
  unsigned long ms = millis();
  File f = LittleFS.open(fname, FILE_READ);
  if (!f) {
    dprintf("image: failed to open %s", fname);
    return false;
  }
  JpegDec.decodeFsFile(f);
  if (!JpegDec.width || !JpegDec.height) {
    dprintf("image: failed to decode %s", fname);
    return false;
  }
  init(JpegDec.width, JpegDec.height);

  while (JpegDec.read()) {
    uint16_t *pImg = JpegDec.pImage;
    int xoff = JpegDec.MCUx * JpegDec.MCUWidth;
    int yoff = JpegDec.MCUy * JpegDec.MCUHeight;
    int w = min(JpegDec.MCUWidth, width - xoff);
    int h = min(JpegDec.MCUHeight, height - yoff);
    for (int y = 0 ; y < h ; y++) {
      for (int x = 0 ; x < w ; x++) {
        *addr(xoff + x, yoff + y) = *pImg++ >> 8;
      }
    }
  }
  dprintf("image: loaded %dx%d in %lums", JpegDec.width, JpegDec.height, millis()- ms);
  JpegDec.abort();
  return true;
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
  int x = hlocate(0, width/2, CARDSUIT_WIDTH);
  tmp.init(CARDSUIT_WIDTH, height);
  tmp.copy(0, 0, crop(x, 0, CARDSUIT_WIDTH, height));

  // determine vertical location
  int ycard = tmp.vlocate(5, 35, CARD_HEIGHT-10) - 5;
  int ysuit = tmp.vlocate(ycard + SUIT_OFFSET - 10, ycard + SUIT_OFFSET + 10, SUIT_HEIGHT);
  //dprintf("locate X=%d, YC=%d, YS=%d", x, ycard, ysuit);
  card = tmp.crop(0, ycard, CARD_WIDTH, CARD_HEIGHT);
  suit = tmp.crop(0, ysuit, SUIT_WIDTH, SUIT_HEIGHT);

  return true;
}

int Image::hlocate(int xmin, int xmax, int w)
{
  int n = xmax - xmin + w;
  std::unique_ptr<unsigned long[]> sums(new unsigned long[n]);
  bzero((void *)sums.get(), n*sizeof(int));

  // determine horizontal position
  for (int i = 0 ; i < n ; i++) {
    unsigned long sum = 0;
    pixel *p = addr(xmin + i, 0);
    for (int j = 0 ; j < height-1 ; j++, p += stride) {
      long v = p[0] - p[stride];
      sum += v * v;
    }
    sums[i] = sum;
  }

  // smooth and locate horizontally
  unsigned long bestx = 0;
  int x = -1;
  unsigned long *p = sums.get();
  for (int i = 0 ; i < n - w ; i++, p++) {
    unsigned long sum = p[0]*4 + p[1]*2 + p[2] + p[w - 3] + p[w - 2]*2 + p[w - 1]*4;
    if (x < 0 || bestx > sum) {
      bestx = sum;
      x = xmin + i;
    }
  }
  return x;
}

int Image::vlocate(int ymin, int ymax, int h)
{
  int n = ymax - ymin + h;
  std::unique_ptr<unsigned long[]> sums(new unsigned long[n]);
  bzero((void *)sums.get(), n*sizeof(int));

  // determine vertical position
  for (int i = 0 ; i < n ; i++) {
    pixel *p = addr(0, ymin + i);
    int vmin = 255, vmax = 0;
    for (int j = 0 ; j < width ; j++, p += 1) {
      int v = *p;
      vmin = min(vmin, v);
      vmax = max(vmax, v);
    }
    sums[i] = (vmax - vmin) * (vmax - vmin);
    //dprintf("vlocate: %d: %d, %d, %d", i, vmin, vmax, sums[i]);
  }

  int m = h/4;
  int y = -1;
  unsigned long besty = 0;
  for (int i = 0 ; i < ymax - ymin ; i++) {
    unsigned long sum = 0;
    for (int j = 0 ; j < m ; j++) {
      sum += (j+1) * (2*sums[i+j] + sums[(i+h-1)-j]);
    }
    if (y < 0 || besty < sum) {
      besty = sum;
      y = ymin + i;
    }
  }
  //dprintf("y=%d, besty=%ul, ymin=%d, ymax=%d, h=%d", y, besty, ymin, ymax, h);
  return y;
}

float Image::distance(Image &other)
{
  float dist = 0;
  for (int r = 0 ; r < height ; r++) {
    const pixel *p1 = addr(0, r);
    const pixel *p2 = other.addr(0, r);
    for (int c = 0 ; c < width ; c++) {
      float d = *p1++ - *p2++;
      dist += d * d;
    }
  }
  return sqrt(dist / (width * height));
}

int Image::match(Image &samples)
{
  int n = samples.width / width;
  int besti = -1;
  float bestd = 0;
  for (int i = 0 ; i < n ; i++) {
    Image sample = samples.crop(i*width, 0, width, height);
    float d = distance(sample);
    //dprintf("distance %d: %f", i, d);
    if (besti < 0 || d < bestd) {
      besti = i;
      bestd = d;
    }
  }
  //dprintf("match: %d, %f", besti, bestd);
  return bestd < 100.0f ? besti : -1;
}

bool Image::same(Image &other)
{
  if (width != other.width || height != other.height) {
    return false;
  }
  for (int r = 0 ; r < height ; r++) {
    const pixel *p1 = addr(0, r);
    const pixel *p2 = other.addr(0, r);
    for (int c = 0 ; c < width ; c++) {
      if (*p1++ != *p2++) {
        return false;
      }
    }
  }
  return true;
}
