// (c)2024, Arthur van Hoff, Artfahrt Inc.

#pragma once
#include "util.h"

class AngleSensor : public IdleComponent {
  public:
    float angle = 0;
    bool active = true;

  public:
    AngleSensor(const char *name) : IdleComponent(name) {}

    void init();

    void idle(unsigned long now) 
    {
      angle = active ? readAngle() : 0;
    }

    inline float value() {
      return angle;
    }

    static float readAngle();
};