// (c)2024, Arthur van Hoff, Artfahrt Inc.

#include "eject.h"
#include "bus.h"
#include "deal.h"

extern BusMaster bus;

bool Ejector::captureCard()
{
    unsigned char card = CARD_NULL;
    if (learning) {
        if (learn_card < 52) {
            card = learn_card++;
        } else {
            return true;
        }
    }
    delay(200);
    //dprintf("captureCard learning=%d", learning);
    unsigned char buf[] = {CMD_CAPTURE, card};
    int result = bus.request(CAMERA_ADDR, buf, sizeof(buf));
    //dprintf("captureCard learning=%d, result=%ld", learning, result);
    return result;
}

bool Ejector::identifyCard(bool blocking, int timeout)
{
    if (current_card != CARD_NULL) {
        dprintf("identifyCard=%d, already identified", current_card);
        return true;
    }
    for (unsigned long start_tm = millis() ; millis() < start_tm + timeout ;) {
        unsigned char buf[1] = {CARD_FAIL};
        if (!bus.request(CAMERA_ADDR, (const unsigned char []){CMD_IDENTIFY}, 1, buf, 1)) {
            current_card = CARD_FAIL;
            dprintf("identifyCard: failed");
            return false;
        }
        current_card = buf[0];
        if (current_card != CARD_NULL) {
            if (learn_card < 52) {
                current_card = learn_card;
            }
            switch(current_card) {
              case CARD_FAIL:
              case CARD_EMPTY:
                break;
              default:
                if (current_card < 0 || current_card >= DECKLEN) {
                    dprintf("identifyCard: got invalid card %d", current_card);
                    current_card = CARD_FAIL;
                }
                break;
            }
            //dprintf("identifyCard: card=%d, %s", current_card, full_name(current_card));
            return true;
        }
        if (!blocking) {
            return false;
        }
        delay(1);
        //dprintf("ident: got current_card=%d", current_card);
    }
    current_card = CARD_FAIL;
    dprintf("identifyCard: timeout, CARD_FAIL");
    return false;
}


bool Ejector::load(bool learn)
{
    dprintf(learn ? "load and learn" : "load");
    this->learning = learn;
    this->learn_card = learn ? 0 : CARD_NULL;
    if (card.state) {
        return true;
    }

    // reset card state on camera
    bus.request(CAMERA_ADDR, (const unsigned char []){CMD_CLEAR}, 1, NULL, 0);

    state = EJECT_LOADING;
    current_card = CARD_NULL;
    loaded_card = CARD_NULL;
    motor1.stop();
    motor2.stop();

    captureCard();
    identifyCard();

    if (current_card == CARD_EMPTY) {
        dprintf("hopper empty");
        return true;
    }

    motor1.set_speed(speed);
    motor2.set_speed(speed);
    card_tm = card.last_tm;
    eject_tm = millis();
    dprintf("loading");
    return true;
}

bool Ejector::eject()
{
    if (!card.state) {
        dprintf("eject: failed no card loaded");
        return false;
    }
    state = EJECT_EJECTING;
    motor1.stop();
    motor2.stop();
    if (current_card == CARD_NULL) {
        identifyCard();
    }
    motor1.set_speed(-speed/4);
    motor2.set_speed(speed);
    loaded_card = CARD_NULL;

    card_tm = card.last_tm;
    eject_tm = millis();
    //dprintf("eject: starting eject");
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
            //dprintf("loading after eject");
        } else if (now > eject_tm + 500) {
            state = EJECT_FAILED;
            motor1.stop();
            motor2.stop();
            dprintf("eject aborted, card=%d", card.state);
        }
        break;
      case EJECT_LOADING:
        if (card.state && now > card.last_tm + 70) {
            motor1.reverse();
            motor2.stop();
            state = EJECT_RETRACTING;
            //dprintf("load done, retracting");
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
        if (now > eject_tm + 140) {
            eject_tm = now;
            motor1.stop();
            motor2.stop();
            state = EJECT_FINISH;
            captureCard();
            //dprintf("reversing done");
        }
        break;
      case EJECT_FINISH:
        if (now > eject_tm + 100) {
            state = EJECT_OK;
            motor1.stop();
            motor2.stop();
            //dprintf("eject finish and done, current=%d, loaded=%d", current_card, loaded_card);
        }
        // fall through
      default:
        if (current_card == CARD_NULL) {
            identifyCard(false);
        }
        break;
    }
}
