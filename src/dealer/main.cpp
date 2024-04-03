// (c)2024, Arthur van Hoff, Artfahrt Inc.

#include "util.h"
#include "deal.h"
#include "bus.h"
#include "motor.h"
#include "angle.h"
#include "sensor.h"
#include "light.h"
#include "eject.h"
#include "storage.h"
#include "webserver.h"

// Components
Storage storage;
BusMaster bus;
Motor motor1("Motor1", M1_PIN1, M1_PIN2, 400, 5000);
Motor motor2("Motor2", M2_PIN1, M2_PIN2, 400, 5000);
//Motor fan("Fan", FN_PIN1, FN_PIN2, 100, 100000, 2000);
Motor rotator("Rotator", MR_PIN2, MR_PIN1, 400, 200);
AngleSensor angle("Angle", rotator);
IRSensor card("Card", CARD_PIN, HIGH);
Ejector ejector("Ejector");
LightRing<RING_PIN> ring("Ring", 9, 16, 75, 2);
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

//
// Dealer
//
enum DealerState {
  DEALER_IDLE,
  DEALER_DEALING,
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
      return deal.owner[card] * 35;
    }

    bool card_ready(int card)
    {
      // do something for this card
      switch (state) {
        case DEALER_DEALING:
          if (deal_position < 0) {
            deal_position = owner_position(card);
            angle.turnTo(deal_position);
          }
          if (!angle.near(deal_position)) {
            // keep turning towards the owner position
            return false;
          }
      }
      //dprintf("dealer: card %d, %s", card, full_name(card));
      return true;
    }

    void deal_done()
    {
        deal_summary();
        dprintf("dealer: done after %d cards", deal_count);
        reset(DEALER_IDLE);
  }

    void deal_failed(const char *reason)
    {
        deal_summary();
        dprintf("dealer: failed after %d cards, %s", deal_count, reason);
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
      } else if (deal_count == 0) {
        dprintf("dealer: got no cards");
      } else {
        dprintf("dealer: got %d cards, %d missing, %d duplicates", deal_count, missing_count, duplicate_count);
      }
    }

    virtual void idle(unsigned long now) 
    {
      switch (state) {
        case DEALER_IDLE:
          break;
        case DEALER_DEALING:
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
                      if (state == DEALER_DEALING) {
                        dprintf("dealer: eject %s to %d", full_name(ejector.loaded_card), (int)angle.value());
                      } else {
                        dprintf("dealer: eject %s", full_name(ejector.loaded_card));
                      }
                      if (!ejector.eject()) {
                        deal_failed("eject failed");
                        return;
                      }
                      deal_count += 1;
                      deal_position = -1;
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
// Animation
//
class LightAnimation : public IdleComponent {
  public:
    unsigned long eject_tm = 0;
    bool ejecting = false;
    unsigned long status_tm = 0;
    unsigned long report_tm = 0;
    int report_cnt = 0;
    unsigned char cam_res[5];
    bool cam_connected = false;
    unsigned long active_tm = 0;
  public:
    LightAnimation() : IdleComponent("LightAnimation", 30) 
    {
        ring.set_brightness(10, 1000);
    }
    virtual void idle(unsigned long now) 
    {
        // check and report status
        if (status_tm + 1000 <= now) {
            status_tm = now;
            cam_connected = bus.request(CAMERA_ADDR, (const unsigned char []){CMD_STATUS}, 1, cam_res, sizeof(cam_res));

            // report status
            if (report_tm + 10*1000 <= now) {
                report_tm = now;
                IPAddress ip = WiFi.localIP();

                dprintf("%5d: dealer, www=%d, detect=%d, card=%d/%d, ang=%d, wifi=%d.%d.%d.%d, cam=%d, camwifi=%d.%d.%d.%d", report_cnt, www.connected, card.state, ejector.current_card, ejector.loaded_card, int(angle.value()), ip[0], ip[1], ip[2], ip[3], cam_res[0], cam_res[1], cam_res[2], cam_res[3], cam_res[4]);
                report_cnt += 1;
            }
      }

      // check active state
      if (button.state || motor1.current_speed != 0 || motor2.current_speed != 0) {
          if (active_tm == 0) {
              ring.set_brightness(100, 500);
          }
          active_tm = now;
      } else if (active_tm != 0 && active_tm + 2*1000 < now) {
          active_tm = 0;
          ring.set_brightness(5, 5*1000);
      }

      // start/stop eject animation
      if (motor2.current_speed > 0) {
          if (!ejecting) {
              eject_tm = now;
              ejecting = true;
          }
      } else {
          ejecting = false;
          eject_tm = 0;
      }

      ring.clear();
      int a = round(angle.value());
      int b = ring.bearing(360 - a);
      // north
      ring.setOne(b, 0, 0, 100);
      // south
      ring.setOne(b + ring.width + ring.height, 100, 100, 0);

      // eject
      if (eject_tm > 0) {
          int i = ((now - eject_tm)  * ring.height) / 400;
          if (i >= ring.height) {
              eject_tm = 0;
          } else {
              ring.setOne(ring.width + ring.height - i - 1, 50, 50, 50);
              ring.setOne(ring.width*2 + ring.height + i, 50, 50, 50);
          }
      }

      // camera
      if (cam_connected) {
          ring.setOne(0, 0, 50, 0);
      } else {
          ring.setOne(0, 50, 0, 0);
      }

      // button
      if (button.state) {
          ring.setOne(-2, 100, 0, 0);
          ring.setOne(ring.width+1, 100, 0, 0);
      }

      // wifi
      if (www.connected) {
          ring.setOne(ring.width-1, 0, 50, 0);
      } else {
          ring.setOne(ring.width-1, 50, 0, 0);
      }
      if (card.state) {
          ring.setOne(ring.width-2, 50, 0, 0);
      }
      
      // show
      ring.show();
    }
};
LightAnimation animation;

//
// Main Program
//

void setup() 
{
  // first turn on the power
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
  pinMode(M_ENABLE, OUTPUT);
  digitalWrite(M_ENABLE, LOW);
  init_all("Dealer2024");
  digitalWrite(M_ENABLE, HIGH);
}

void loop() 
{
  idle_all();
}
