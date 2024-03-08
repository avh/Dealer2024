// (c)2024, Arthur van Hoff, Artfahrt Inc.

#pragma once
#include "util.h"
#include "sensor.h"
#include "motor.h"

enum EjectState {
    EJECT_IDLE,
    EJECT_RETRACTING,
    EJECT_EJECTING,
    EJECT_LOADING,
    EJECT_FAILED,
    EJECT_OK,
};

class Ejector : IdleComponent {
public:
    EjectState state;
    int speed = 800;
    unsigned long eject_tm;
    unsigned long card_tm;
    
public:
    Ejector(const char *name) : IdleComponent(name) {}

    bool load();
    bool eject();
    virtual void idle(unsigned long now);
};