// (c)2024, Arthur van Hoff, Artfahrt Inc.

#include "eject.h"
#include "bus.h"

extern BusMaster bus;

bool Ejector::captureCard()
{
    return bus.request(0x10, (const unsigned char []){CMD_CAPTURE}, 1);
}
bool Ejector::identifyCard()
{
    unsigned char buf[1];
    for (unsigned int start_tm = millis() ; millis() < start_tm + 1000 ;) {
        if (!bus.request(0x10, (const unsigned char []){CMD_IDENTIFY}, 1, buf, 1)) {
            return CARD_FAIL;;
        }
        current_card = buf[0];
        if (current_card != CARD_NULL) {
            return true;
        }
        delay(1);
    }
    current_card = CARD_FAIL;
    return false;
}

bool Ejector::load()
{
    dprintf("load");
    if (card.state) {
        return true;
    }
    if (current_card == CARD_NULL) {
        captureCard();
        identifyCard();
    }
    if (current_card == CARD_EMPTY) {
        dprintf("hopper empty");
        return true;
    }

    state = EJECT_LOADING;
    motor1.stop();
    motor2.stop();
    motor1.set_speed(speed);
    motor2.set_speed(speed);
    fan.set_speed(100);
    card_tm = card.last_tm;
    eject_tm = millis();
    fan_tm = millis();
    return true;
}

bool Ejector::eject()
{
    dprintf("eject");
    if (!card.state) {
        return false;
    }
    if (current_card == CARD_NULL) {
        identifyCard();
    }
    state = EJECT_EJECTING;
    motor1.stop();
    motor2.stop();
    motor1.set_speed(0);
    motor2.set_speed(speed);
    fan.set_speed(100);

    card_tm = card.last_tm;
    eject_tm = millis();
    fan_tm = millis();
    return true;
}

void Ejector::idle(unsigned long now)
{
    switch (state) {
      case EJECT_EJECTING:
        if (!card.state || card.last_tm != card_tm) {
            motor1.set_speed(speed);
            motor2.set_speed(speed);
            card_tm = card.last_tm;
            state = EJECT_LOADING;
            card_tm = card.last_tm;
            eject_tm = now;
            dprintf("loading after eject");
            loaded_card = CARD_NULL;
        } else if (now > eject_tm + 300) {
            state = EJECT_FAILED;
            motor1.stop();
            motor2.stop();
            dprintf("eject aborted");
        }
        break;
      case EJECT_LOADING:
        if (card.state && card.last_tm < millis() - 50) {
            motor1.reverse();
            motor2.stop();
            state = EJECT_RETRACTING;
            dprintf("load done, retracting");
            loaded_card = current_card;
            current_card = CARD_NULL;
        //} else if (card.state && card.last_tm < millis() - 40 && motor1.goal_speed > 0) {
        //    motor1.reverse();
        //    dprintf("loading, reversing");
        } else if (now > eject_tm + 1000) {
            // maybe retry?
            state = EJECT_FAILED;
            motor1.stop();
            motor2.stop();
            dprintf("load aborted");
        }
        break;
      case EJECT_RETRACTING:
        if (now > eject_tm + 200) {
            motor1.stop();
            motor2.stop();
            state = EJECT_OK;
            dprintf("reversing done");
            captureCard();
        }
        break;
      default:
        if (fan_tm && now > fan_tm + 1000) {
            fan.stop();
            fan_tm = 0;
        }
        break;
    }
}
