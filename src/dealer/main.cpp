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
Motor motor2("Motor2", M2_PIN1, M2_PIN2, 400, 5000);
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
  DEALER_VERIFYING,
};

extern class Dealer dealer;


class Dealer : public IdleComponent {
  public:
    DealerState state = DEALER_IDLE;
    Deal deal;
    int deal_count = 0;
    float deal_position = -1;
    int card_count[DECKLEN];
    int card_hist[DECKLEN];
    unsigned long last_tm = 0;
  public:
    Dealer() : IdleComponent("Dealer", 1) {
    }

    virtual void init()
    {
      www.add("/learn", [] (HTTP &http) {
        if (card.state || dealer.state != DEALER_IDLE) {
          http.header(404, "Invalid State, Card Present or Busy");
          http.close();
          return;
        }
        // load with learn=true
        if (!ejector.load(true)) {
          http.header(404, "Load failed");
          http.close();
          return;
        }
        http.header(200, "Learning Started");
        http.close();
        dealer.reset(DEALER_LEARNING);
      });
      www.add("/verify", [] (HTTP &http) {
        if (card.state || dealer.state != DEALER_IDLE) {
          http.header(404, "Invalid State, Card Present or Busy");
          http.close();
          return;
        }
        if (!ejector.load()) {
          http.header(404, "Load failed");
          http.close();
          return;
        }
        http.header(200, "Verification Started");
        http.close();
        dealer.reset(DEALER_VERIFYING);
      });

      www.add("/deal", [] (HTTP &http) {
        String cards = http.param["deal"];
        if (!dealer.deal.parse(cards.c_str())) {
          http.header(200, "Cards not Parsed Correctly");
          http.close();
          return;
        }
        dealer.deal.debug();
        if (card.state || dealer.state != DEALER_IDLE) {
          http.header(404, "Invalid State, Card Present or Busy");
          http.close();
          return;
        }

        // load first card
        if (!ejector.load()) {
          http.header(404, "Load Failed");
          http.close();
          return;
        }

        http.header(200, "Dealing Started");
        http.close();

        // start dealing
        dealer.reset(DEALER_DEALING);

        // start turning to the correct position for the first card
        dealer.deal_position = dealer.owner_position(ejector.loaded_card);
        angle.turnTo(dealer.deal_position);
      });
    }

    void reset(DealerState state = DEALER_IDLE)
    {
      if (state != DEALER_IDLE) {
        for (int i = 0 ; i < DECKLEN ; i++) {
          card_count[i] = 0;
          card_hist[i] = CARD_NULL;
        }
        deal_count = 0;
        deal_position = -1;
      }
      this->state = state;
      this->last_tm = millis();
    }

    int owner_position(int card)
    {
      return deal.owner[card] * 20;
    }

    bool card_ready(int card)
    {
      // do something for this card
      dprintf("dealer: card %d, %s", card, full_name(card));
      return true;
    }

    void collate()
    {
        if (state == DEALER_LEARNING && deal_count > 0) {
          dprintf("dealer: collating %d cards", deal_count);
          bus.request(CAMERA_ADDR, (const unsigned char []){CMD_COLLATE}, 1, NULL, 0);
        }
    }

    void deal_done()
    {
        deal_summary();
        dprintf("dealer: done after %d cards", deal_count);
        collate();
        reset(DEALER_IDLE);
  }

    void deal_failed(const char *reason)
    {
        deal_summary();
        dprintf("dealer: failed after %d cards, %s", deal_count, reason);
        collate();
        reset(DEALER_IDLE);
    }

    void deal_summary()
    {
      dprintf("dealer: dealt %d cards", deal_count);
      for (int i = 0 ; i < deal_count ; i++) {
        dprintf("dealer: card %d, %s%s", i, full_name(card_hist[i]), card_count[card_hist[i]] > 1 ? " (DUPLICATE)" : "");
      }
      int order_count = 0;
      for (int i = 0 ; i < DECKLEN-1 ; i++) {
        if (card_hist[i]+1 == card_hist[i+1]) {
          order_count++;
        }
      }
      int missing_count = 0;
      int duplicate_count = 0;
      for (int i = 0 ; i < DECKLEN ; i++) {
        if (card_count[i] == 0) {
          if (deal_count > 40) {
            dprintf("dealer: missing card %d, %s", i, full_name(i));
          }
          missing_count++;
        } else if (card_count[i] > 1) {
          dprintf("dealer: duplicate card %d, %s", i, full_name(i));
          duplicate_count++;
        }
      }
      if (missing_count == 0 && duplicate_count == 0 && order_count == DECKLEN-1) {
        dprintf("dealer: all cards present, sorted, and accounted for. Nice!");
      } else if (missing_count == 0 && duplicate_count == 0) {
        dprintf("dealer: all cards present and accounted for, not in order though");
      } else {
        dprintf("dealer: missing %d cards, %d duplicates", missing_count, duplicate_count);
      }
    }

    virtual void idle(unsigned long now) 
    {
      switch (state) {
        case DEALER_IDLE:
          break;
        case DEALER_DEALING:
        case DEALER_LEARNING:
        case DEALER_VERIFYING:
          if (now > last_tm + 10000) {
            dprintf("STATE=%d, %d/%d", ejector.state, ejector.current_card, ejector.loaded_card);
            deal_failed("timeout");
            return;
          }
          switch (ejector.state) {
            case EJECT_OK:
            case EJECT_IDLE:
              switch (ejector.loaded_card) {
                case CARD_NULL:
                  break;
                case CARD_EMPTY:
                  deal_done();
                  break;
                case CARD_FAIL:
                  if (deal_count == 52) {
                    deal_done();
                  } else {
                    deal_failed("eject card failed");
                  }
                  break;
                default:
                  if (ejector.loaded_card >= 0 && ejector.loaded_card < DECKLEN) {
                    if (deal_count == DECKLEN) {
                      deal_failed("too many cards");
                      return;
                    }
                    if (card_hist[deal_count] == CARD_NULL) {
                      if (card_count[ejector.loaded_card] > 0) {
                        dprintf("dealer: duplicate card %d, %s", ejector.loaded_card, full_name(ejector.loaded_card));
                      }
                      card_count[ejector.loaded_card] += 1;
                      card_hist[deal_count] = ejector.loaded_card;
                      last_tm = millis();
                    }
                    if (card_ready(ejector.loaded_card)) {
                      if (!ejector.eject()) {
                        deal_failed("eject failed");
                        return;
                      }
                      deal_count += 1;
                      last_tm = millis();
                    }
                  }
              }
              break;
            case EJECT_FAILED:
              deal_failed("eject failed");
              return;
          }
          
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
