// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#pragma once
#include "util.h"

class Button : public IdleComponent {
  public:
    int pin;
    int active_state = LOW;
    bool state = false;
    volatile unsigned long pressed_tm = 0;
    volatile unsigned long released_tm = 0;
    volatile int click_count = 0;
  public:
    Button(const char *name, int pin, int active_state = LOW) : IdleComponent(name), pin(pin), active_state(active_state) {
    }
    virtual void init() {
        pinMode(pin, active_state == LOW ? INPUT_PULLUP : INPUT_PULLDOWN);
        button = this;
        attachInterrupt(digitalPinToInterrupt(pin), handle_interrupt, CHANGE);
    }
    bool down() {
        return digitalRead(pin) == active_state;
    }
    virtual void idle(unsigned long now);
    virtual void pressed(int count) {}
    virtual void hold() {}
    virtual void released() {}

  public:
    static Button *button;
    static void handle_interrupt();
};
