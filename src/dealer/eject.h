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
    unsigned long fan_tm;
    bool learning = false;
    int learn_card = CARD_NULL;
    int current_card = CARD_NULL;
    int loaded_card = CARD_NULL;
    
public:
    Ejector(const char *name) : IdleComponent(name) {}

    bool captureCard();
    bool identifyCard();

    bool load(bool learn = false);
    bool eject();
    virtual void idle(unsigned long now);
};