// (c)2024, Arthur van Hoff, Artfahrt Inc.

#include "util.h"
#include "deal.h"
#include "bus.h"
#include "motor.h"
#include "angle.h"
#include "sensor.h"
#include "eject.h"
#include "storage.h"
#include "webserver.h"

// Components
Storage storage;
BusMaster bus;
Motor motor1("Motor1", M1_PIN1, M1_PIN2, 400, 5000);
Motor motor2("Motor2", M2_PIN2, M2_PIN2, 400, 5000);
//Motor fan("Fan", FN_PIN1, FN_PIN2, 100, 100000, 2000);
Motor rotator("Rotator", MR_PIN2, MR_PIN1, 400, 400);
AngleSensor angle("Angle", rotator);
IRSensor card("Card", CARD_PIN, HIGH);
Ejector ejector("Ejector");
WebServer www;

// REMIND: Power Button
class Button : public IdleComponent {
  public:
    int but;
    int active_state = LOW;
    unsigned long last_tm = 0;
    bool state = false;
    int clicks = 0;
    int duration = 0;
    int tmp_card = 0;
  public:
    Button(const char *name, int but, int active_state = LOW) : IdleComponent(name), but(but), active_state(active_state) {
    }
    virtual void init() {
      pinMode(but, active_state == LOW ? INPUT_PULLDOWN : INPUT_PULLUP);
    }
    bool down() {
      return digitalRead(but) == active_state;
    }
    virtual void idle(unsigned long now) {
      if (down()) {
        if (!state) {
          state = true;
          duration = now - last_tm;
          if (duration < 500) {
            clicks += 1;
          } else {
            clicks = 1;
          }
          last_tm = now;
          dprintf("Button %s pressed, clicks=%d", name, clicks);
          pressed();
        } else if (now - last_tm > 3000) {
          duration = now - last_tm;
          last_tm = now;
          dprintf("Button %s hold, duration=%dms", name, duration);
          hold();
        }
      } else {
        if (state) {
          state = false;
          duration = now - last_tm;
          last_tm = now;
          dprintf("Button %s released, duration=%dms", name, duration);
          released();
        }
      }
    }
    virtual void pressed() {
      if (card.state) {
        ejector.eject();
      } else {
        ejector.load();
      }
    }
    virtual void hold() {}
    virtual void released() {}
};

Button button("Button", BUTTON_PIN, HIGH);

class Idler : IdleComponent {
  public:
    int cnt = 0;
  public:
    Idler() : IdleComponent("Idler", 10*1000) {
    }
    virtual void idle(unsigned long now) {
      unsigned char res[6];
      if (!bus.request(CAMERA_ADDR, (const unsigned char []){CMD_STATUS}, 1, res, sizeof(res))) {
        dprintf("cam failed");
      }

      IPAddress ip = WiFi.localIP();

      dprintf("%5d: dealer, detect=%d, card=%d/%d, ang=%d, wifi=%d.%d.%d.%d, cam=%d,%d, camwifi=%d.%d.%d.%d", cnt, card.state, ejector.current_card, ejector.loaded_card, int(angle.value()), ip[0], ip[1], ip[2], ip[3], res[0], res[1], res[2], res[3], res[4], res[5]);
      cnt += 1;
    }
};


Idler idler;

//
// Dealer
//
enum DealerState {
  DEALER_IDLE,
  DEALER_DEALING,
  DEALER_LEARNING,
};

extern class Dealer dealer;

const char *deals[] = {
  "2C3C4C5C6C7C9C1CJCQCAC8C", "2D3D4D5D6D7D9D1DJDQDAC8D", "2H3H4H5H6H7H9H1HJHQHA8H", "2S3S4S5S6S7S9S1SJSQSAS8S",
  NULL,
};

class Dealer : public IdleComponent {
  public:
    DealerState state = DEALER_IDLE;
  public:
    Dealer() : IdleComponent("Dealer", 1) {
    }

    virtual void init()
    {
      www.add("/learn", [] (HTTP &http) {
        if (card.state || dealer.state != DEALER_IDLE) {
          http.header(404, "Invalid State");
          http.close();
          return;
        }
        if (!ejector.load(true)) {
          http.header(404, "Load failed");
          http.close();
          return;
        }
        http.header(200, "Learning Started");
        http.close();
        dealer.state = DEALER_LEARNING;
      });

      www.add("/deal", [] (HTTP &http) {
        String cards = http.param["deal"];
        dprintf("GOT a DEAL: %s", cards.c_str()); 
        Deal deal;
        if (!deal.parse(cards.c_str())) {
          http.header(200, "Cards not Parsed");
          http.close();
          return;
        }
        deal.debug();
        http.header(200, "Dealing Started");
        http.close();
      });
    }

    virtual void idle(unsigned long now) 
    {
      switch (state) {
        case DEALER_IDLE:
          break;
        case DEALER_DEALING:
          // REMIND
          break;
        case DEALER_LEARNING:
          switch (ejector.state) {
            case EJECT_IDLE:
            case EJECT_OK:
              if (ejector.loaded_card != CARD_EMPTY) {
                dprintf("learning learn=%d current=%d loaded=%d", ejector.learn_card, ejector.current_card, ejector.loaded_card);
                ejector.eject();
              } else if (ejector.loaded_card == CARD_EMPTY) {
                if (ejector.learn_card == 52) {
                  dprintf("learning succeeded");
                } else {
                  dprintf("learning failed, deck is ncards=%d", ejector.learn_card-1);
                }
                if (ejector.learn_card > 0) {
                  bus.request(CAMERA_ADDR, (const unsigned char []){CMD_COLLATE}, 1, NULL, 0);
                }
                state = DEALER_IDLE;
                ejector.learning = false;
              }
              break;
            case EJECT_FAILED:
              dprintf("learning failed, state=%d",  ejector.state);
              state = DEALER_IDLE;
              ejector.learning = false;
              if (ejector.learn_card > 0) {
                  bus.request(CAMERA_ADDR, (const unsigned char []){CMD_COLLATE}, 1, NULL, 0);
              }
              break;
            default:
              break;
          }
          break;
      }
    }
};

Dealer dealer;

//
// Main Program
//

void setup() 
{
  init_all("Dealer");
}

void loop() 
{
  idle_all();
}
