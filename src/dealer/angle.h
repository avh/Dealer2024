// (c)2024, Arthur van Hoff, Artfahrt Inc.

#pragma once
#include "util.h"
#include "motor.h"

#define mod360(a)   ((a) - floor((a) / 360.0f) * 360.0f)
#define adiff(a, b)  (mod360((a - b) + 180.0f) - 180.0f)
#define sign(x)     ((x) > 0 ? 1 : ((x) < 0 ? -1 : 0))

class AngleSensor : public IdleComponent {
  public:
    Motor &rotator;
    float north = -1;
    float angle = 0;
    float target_angle = -1;
    bool active = true;

  public:
    AngleSensor(const char *name, Motor &rotator) : IdleComponent(name, 10), rotator(rotator) {}

    void init();

    virtual void idle(unsigned long now);

    inline float value() {
      return north < 0 ? angle : mod360(angle - north);
    }
    inline bool near(float a) {
      return abs(adiff(a, value())) < 5;
    }

    void turnTo(float target);

  public:
    static float readAngle();
};