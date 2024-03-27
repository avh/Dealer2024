// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once
#include "util.h"

class IRSensor : public IdleComponent {
  public:
    int pin;
    bool state;
    int active_state;
    unsigned long last_tm;
  public:
    IRSensor(const char *name, int pin, int active_state=LOW) : IdleComponent(name), pin(pin), active_state(active_state) {
    }

    virtual void init() {
      attachInterrupt(digitalPinToInterrupt(pin), handle_interrupt, CHANGE);
    }
    virtual void idle(unsigned long now) {
      bool s = digitalRead(pin) == active_state;
      if (s != state) {
        // REMIND: this happens
        //dprintf("fixing sensor, state=%d", s);
        state = s;
        last_tm = now;
      }
    }
    virtual void halt() {
      detachInterrupt(digitalPinToInterrupt(pin));
    }

    static void handle_interrupt();
};

extern IRSensor card;