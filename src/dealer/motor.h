// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once
#include "util.h"

#define PWM_FREQ    500
#define PWM_BITS    10
#define PWM_MAX     ((1<<PWM_BITS)-1)
#define PWM_MIN     (PWM_MAX / 10)

#define MOTOR_VOLTAGE     6
#define BATTERY_VOLTAGE   9

class PWM : public InitComponent {
  public:
    int pin;
    int value = 0;    

  public:
    PWM(int pin) : InitComponent("pwm"), pin(pin) 
    {
    }
    virtual void init() 
    {
      pinMode(pin, OUTPUT);
      write(value);
    }
    void write(int value) 
    {
      value = max(0, min(value, PWM_MAX));
      //dprintf("write pin=%d, value=%d", pin, value);
      this->value = value;
      // Due to restrictions on simultaneous use of PWM pins, 
      // we need to switch to digital mode when possible.
      if (value == 0) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
      } else if (value == PWM_MAX) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
      } else {
        analogWrite(pin, value);
      }
    }
};

class Motor : IdleComponent {
  public:
    PWM pwm1;
    PWM pwm2;
    float max_value;
    float max_accel_per_sec;
    float goal_speed = 0;
    float current_speed = 0;
    unsigned long last_speed_ms = 0; 
    
  public:
    Motor(const char *name, int pin1, int pin2, float max_value=100, float max_accel_per_sec=100000) 
      : IdleComponent(name), pwm1(pin1), pwm2(pin2), max_value(max_value), max_accel_per_sec(max_accel_per_sec)
    {
    }

    virtual void init() 
    {
      stop();
    }

    inline float value() {
      return current_speed;
    }
    inline float min_speed() {
      return max_value * 0.1;
    }
    inline float max_speed() {
      return max_value;
    }
    
    void stop();
    void brake();
    void reverse();
    virtual void set_speed(float speed);
    virtual void idle(unsigned long now);
};

extern Motor motor1;
extern Motor motor2;
extern Motor rotator;

extern int eject();