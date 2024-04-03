// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include <esp_camera.h>
#include "util.h"
#include "light.h"
#include "image.h"

class Camera : InitComponent {
  public:
    bool active = false;
    LEDArray &light;
    int frame_nr = 0;
    unsigned long frame_tm = 0;
    int card_count = 0;
    volatile int last_card = CARD_NULL;
    int prev_card = CARD_NULL;

  public:
    Camera(LEDArray &light) : InitComponent("capture"), light(light) {}
    virtual void init();

    camera_fb_t *capture();
    bool captureCard();
    void clearCard();
    int predict(const Image &img);
};

extern Camera cam;