// (c)2024, Arthur van Hoff, Artfahrt Inc.

#include "eject.h"
#include "bus.h"
#include "deal.h"

extern BusMaster bus;

bool Ejector::captureCard()
{
    delay(200);
    //dprintf("captureCard learning=%d", learning);
    current_card = CARD_NULL;
    unsigned char buf[] = {CMD_CAPTURE};
    return bus.request(CAMERA_ADDR, buf, sizeof(buf));
}

bool Ejector::identifyCard(int timeout)
{
    switch (current_card) {
      case CARD_FAIL:
      case CARD_USED:
      case CARD_EMPTY:
        return true;
      case CARD_NULL:
        for (unsigned long start_tm = millis() ;;) {
            unsigned char buf[1] = {CARD_FAIL};
            if (!bus.request(CAMERA_ADDR, (const unsigned char []){CMD_IDENTIFY}, 1, buf, 1)) {
                current_card = CARD_FAIL;
                dprintf("identifyCard: failed");
                return false;
            }
            current_card = buf[0];
            if (current_card != CARD_NULL) {
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
                dprintf("identifyCard: card=%d, %s", current_card, full_name(current_card));
                return true;
            }
            if (timeout == 0) {
                return false;
            }
            if (millis() > start_tm + timeout) {
                dprintf("identifyCard: timeout=%d, CARD_FAIL", timeout);
                break;
            }
            delay(1);
            //dprintf("ident: got current_card=%d", current_card);
        }
        current_card = CARD_FAIL;
        dprintf("identifyCard: timeout, CARD_FAIL");
        return false;
      default:
        return true;
    }
}


bool Ejector::load(bool learn)
{
    if (card.state) {
        dprintf("load: failed, card already loaded");
        return false;
    }

    // reset card state on camera
    unsigned char buf[] = {CMD_CLEAR, (unsigned char)(learn ? 1 : 0) };
    if (!bus.request(CAMERA_ADDR, buf, sizeof(buf), NULL, 0)) {
        dprintf("load: failed to clear cards");
        return false;
    }
    if (!captureCard()) {
        dprintf("load: failed to capture card");
        return false;
    }

    dprintf(learn ? "load and learn" : "load");
    loaded_card = CARD_NULL;
    if (!identifyCard()) {
        dprintf("load: failed to identify card");
        return false;
    }
    if (current_card == CARD_EMPTY) {
        dprintf("load: hopper empty");
        return false;
    }

    state = EJECT_LOADING;
    learning = learn;
    motor1.stop();
    motor2.stop();

    motor1.set_speed(speed);
    motor2.set_speed(speed);
    card_tm = card.last_tm;
    eject_tm = millis();
    return true;
}

bool Ejector::eject()
{
    if (!card.state) {
        dprintf("eject: failed no card loaded");
        return false;
    }

    dprintf(learning ? "eject and learn" : "eject");
    if (!identifyCard()) {
        dprintf("eject: failed to identify card");
        return false;
    }

    state = EJECT_EJECTING;
    loaded_card = CARD_NULL;
    motor1.stop();
    motor2.stop();
    motor1.set_speed(-speed/4);
    motor2.set_speed(speed);

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
            current_card = CARD_USED;
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
        if (now > eject_tm + 100) {
            eject_tm = now;
            motor1.stop();
            motor2.stop();
            state = EJECT_FINISH;
            //dprintf("reversing done");
        }
        break;
      case EJECT_FINISH:
        if (now > eject_tm + 140) {
            state = EJECT_OK;
            motor1.stop();
            motor2.stop();
            captureCard();
            //dprintf("eject finish and done, current=%d, loaded=%d", current_card, loaded_card);
        }
        // fall through
      default:
        identifyCard(0);
        break;
    }
}
