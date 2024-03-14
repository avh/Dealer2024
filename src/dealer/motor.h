// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once
#include "util.h"

#define PWM_FREQ    500
#define PWM_BITS    10
#define PWM_MAX     ((1<<PWM_BITS)-1)

#define MOTOR_VOLTAGE     6
#define BATTERY_VOLTAGE   9

class PWM : public InitComponent {
  public:
    int pin;
    //int chan = -1;
    int value = -1;    
    //static int pwm_channel;

  public:
    PWM(int pin) : InitComponent("pwm"), pin(pin) 
    {
    }
    virtual void init() 
    {
      //chan = pwm_channel++;
      //ledcSetup(chan, PWM_FREQ, PWM_BITS);
      //ledcAttach(pin, PWM_FREQ, PWM_BITS);
      //dprintf("pwm init pin=%d, chan=%d", pin, chan);
      if (value >= 0) {
        write(value);
      }
    }
    void write(int value) 
    {
      value = max(0, min(value, PWM_MAX));
      //dprintf("write pin=%d, value=%d", pin, value);
      this->value = value;
      analogWrite(pin, value);
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
    int auto_off_ms = 0;
    unsigned long last_speed_ms = 0; 
    
  public:
    Motor(const char *name, int pin1, int pin2, float max_value=100, float max_accel_per_sec=100000, int auto_off_ms=0) 
      : IdleComponent(name), pwm1(pin1), pwm2(pin2), max_value(max_value), max_accel_per_sec(max_accel_per_sec), auto_off_ms(auto_off_ms)
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
extern Motor fan;
extern Motor rotator;

extern int eject();