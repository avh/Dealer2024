// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include "util.h"
#define FASTLED_INTERNAL
#include <FastLED.h>

class LEDArray : public IdleComponent {
  public:
    int pin;
    int nleds;
    int offset;
    float brightness;
    CRGB *leds;
    unsigned long on_tm = 0;
    unsigned long off_tm = 0;
    bool value = false;
    unsigned long brightness_tm = 0;
    float brightness_target = 0;
    float brightness_step = 0;

  public:
    LEDArray(const char *name, int nleds, int brightness = 100, int offset = 0) : 
        IdleComponent(name, 1000), nleds(nleds), brightness(255 * brightness / 100), offset(offset), leds(new CRGB[nleds]) {
    }

    void clear() {
        setAll(0, 0, 0);
        value = false;
    }

    void setAll(int r, int g, int b) {
        for (int i = 0 ; i < nleds ; i++) {
            setOne(i, r, g, b);
        }
    }

    void setOne(int i, int r, int g, int b) {
        r = max(0, min(r * brightness / 100, 255));
        g = max(0, min(g * brightness / 100, 255));
        b = max(0, min(b * brightness / 100, 255));
        if (r != 0 || g != 0 || b != 0) {
            //dprintf("setOne(%d) = %d, %d, %d", i, r, g, b);
            value = true;
        }
        // NOTE: red and green are flipped
        leds[(i + offset) % nleds] = CRGB(g, r, b);
    }

    void show() {
        FastLED.show();
    }

    void set_brightness(int b, int ms = 0) {
        brightness_target = max(0, min(255 * b / 100, 255));
        interval = 10;
        brightness_tm = millis() + ms;
        brightness_step = interval * (brightness_target - brightness) / ms;
        //dprintf("set_brightness(%d,%d) target=%f, step=%f", b, ms, brightness_target, brightness_step);
    }

    virtual void idle(unsigned long now) {
        if (brightness_tm != 0 && brightness != brightness_target) {
            if (now >= brightness_tm) {
                brightness = brightness_target;
                brightness_tm = 0;
            } else {
                brightness = max(0, min(brightness + brightness_step, 255));
            }
        }
        if (off_tm > 0 && now >= off_tm) {
            off();
        }
        if (off_tm == 0 && brightness_tm == 0) {
            interval = 1000;
        }
    }

    void on(int v = 100, int ms = 0) {
        unsigned long now = millis();
        if (on_tm == 0) {
            on_tm = now;
        }
        setAll(v, v, v);
        show();
        off_tm = ms == 0 ? 0 : now + ms;
        interval = 10;
    }

    void off() {
        clear();
        show();
        on_tm = 0;
        off_tm = 0;
        value = false;
        interval = 1000;
    }
};

class LEDRing : public LEDArray {
  public:
    int width;
    int height;
    float angle;
  public:
    LEDRing(const char *name, int width, int height, int brightness = 255, int offset = 0) : width(width), height(height), LEDArray(name, 2*width + 2*height, brightness, offset) {
        angle = atan(float(width)/float(height)) * 180 / M_PI;
    }
    int bearing(float d) {
        d = fmod(d, 360.);

        if (d < angle || d > 360.0 - angle) {
            return int(round(tan(d * M_PI / 180.0) * (height/2.0) + width/2.0 - 0.5));
        }
        if (d > 180 - angle && d < 180.0 + angle) {
            return int(round(tan((d - 180.0) * M_PI / 180.0) * (height/2.0) + width + height + width/2.0 - 0.5));
        }
        if (d >= angle && d <= 180.0 - angle) {
            return int(round(tan((d - 90.0) * M_PI / 180.0) * (width/2.0) + width + height/2.0 - 0.5));
        }
        return int(round(tan((d - 270.0) * M_PI / 180.0) * (width/2.0) + width*2 + height + height/2.0 - 0.5));
    }
};

template<int PIN>
class LightArray : public LEDArray 
{
  public:
    LightArray(const char *name, int nleds, int brightness = 255) : LEDArray(name, nleds, brightness) {
    }
    virtual void init() {
        FastLED.addLeds<WS2812, PIN>(leds, nleds);
        off();
    }
};

template<int PIN>
class LightRing : public LEDRing
{
  public:
    LightRing(const char *name, int width, int height, int brightness = 255, int offset = 0) : LEDRing(name, width, height, brightness, offset) {
    }
    virtual void init() {
        FastLED.addLeds<WS2812, PIN>(leds, nleds);
        off();
        LEDRing::init();
    }
};