// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include "util.h"
#define FASTLED_INTERNAL
#include <FastLED.h>

class LEDArray : public IdleComponent {
  public:
    int nleds;
    int brightness;
    CRGB *leds;
    unsigned long on_tm = 0;
    unsigned long off_tm = 0;
    bool value = false;

  public:
    LEDArray(const char *name, int nleds, int brightness = 255) : 
        IdleComponent(name, 5000), nleds(nleds), brightness(brightness), leds(new CRGB[nleds]) {
    }
    virtual void init() {
        FastLED.addLeds<WS2812, LIGHT_PIN>(leds, nleds);
        off();
    }

    void setRGB(int r, int g, int b) {
        for (int i = 0; i < nleds; i++) {
            leds[i] = CRGB(r, g, b);
        }
        FastLED.show();
    }

    virtual void idle(unsigned long now) {
        if (off_tm > 0 && now >= off_tm) {
            off();
        }
    }
    void on(int v = 100, int ms = 0) {
        unsigned long now = millis();
        if (on_tm == 0) {
            on_tm = now;
        }
        int g = v * brightness / 100;
        setRGB(g, g, g);
        off_tm = ms == 0 ? 0 : now + ms;
        value = true;
    }
    void off() {
        setRGB(0, 0, 0);
        on_tm = 0;
        off_tm = 0;
        value = false;
    }
};