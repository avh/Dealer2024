// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include <esp_camera.h>
#include "util.h"
#include "light.h"

class Camera : InitComponent {
  public:
    int frame_nr = 0;
    unsigned long frame_tm = 0;
    int card_count = 0;
    volatile int last_card = CARD_NULL;
    int prev_card = CARD_NULL;
    bool learning = false;

  public:
    Camera() : InitComponent("capture") {}
    virtual void init();

    camera_fb_t *capture();
    bool captureCard();
    void clearCard(bool learn = false);
    void collate();
};

extern LEDArray light;
extern Camera cam;