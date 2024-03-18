// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include <esp_camera.h>
#include "util.h"
#include "light.h"

class Camera : IdleComponent {
  public:
    int frame_nr = 0;
    unsigned long frame_tm = 0;
    volatile int last_card = CARD_NULL;

  public:
    Camera() : IdleComponent("capture", 1000) {}
    virtual void init();
    virtual void idle(unsigned long now);

    camera_fb_t *capture();
    bool captureCard(int learn_card = CARD_NULL);
    void clearCard();
    void collate();
};

extern LEDArray light;
extern Camera cam;