// (c)2024, Arthur van Hoff, Artfahrt Inc.

#include "motor.h"
#include "sensor.h"

//
// PWM
//

//int PWM::pwm_channel = 0;

//
// Motor
//

void Motor::stop() 
{
    pwm1.write(0);
    pwm2.write(0);
    goal_speed = 0;
    current_speed = 0;
}

void Motor::brake()
{
    pwm1.write(1);
    pwm2.write(1);
    goal_speed = 0;
    current_speed = 0;
}
void Motor::reverse()
{
    int v2 = pwm2.value;
    pwm2.write(pwm1.value);
    pwm1.write(v2);
    goal_speed = -goal_speed;
    current_speed = -current_speed;
}

void Motor::set_speed(float speed) 
{
    float m = max_speed();
    goal_speed = max(-m, min(speed, m));
    last_speed_ms = millis();
    //dprintf("set_speed %s %f", name, goal_speed);
}
 
void Motor::idle(unsigned long now) 
{
    if (auto_off_ms > 0 && current_speed != 0 && now > last_speed_ms + auto_off_ms) {
        set_speed(0);
    }
    if (current_speed != goal_speed) {
        unsigned long ms = now - last_idle_tm;
        float max_delta = max_accel_per_sec * ms / 1000.0f;
        if (current_speed < goal_speed) {
            current_speed = min(goal_speed, current_speed + max_delta);
        } else {
            current_speed = max(goal_speed, current_speed - max_delta);
        }
        //dprintf("update_speed %s current=%f goal=%f max/sec=%f, delta=%f, ms=%lu", name, current_speed, goal_speed, max_accel_per_sec, max_delta, ms);
        int value = int(abs(current_speed) * PWM_MAX / max_value);
        if (current_speed > 0) {
            pwm1.write(value);
            pwm2.write(0);
        } else {
            pwm1.write(0);
            pwm2.write(value);
        }
    } 
}
