// (c)2024, Arthur van Hoff, Artfahrt Inc.

#include "util.h"
#include "bus.h"
#include "motor.h"
#include "angle.h"
#include "sensor.h"
#include "eject.h"
#include "webserver.h"

// Components
BusMaster bus;
Motor motor1("Motor1", M1_PIN1, M1_PIN2, 400, 10000);
Motor motor2("Motor2", M2_PIN1, M2_PIN2, 400, 10000);
Motor fan("Fan", FN_PIN1, FN_PIN2, 100, 100000, 2000);
Motor rotator("Rotator", MR_PIN1, MR_PIN2, 400, 400);
AngleSensor angle("Angle");
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
        unsigned char buf[] = {CMD_CAPTURE};
        bus.request(CAMERA_ADDR, buf, sizeof(buf));
      } else {
        ejector.load();
      }
    }
    virtual void hold() {}
    virtual void released() {}
};

Button button("Button", BUTTON_PIN, HIGH);

class Debugger : IdleComponent {
  public:
    int cnt = 0;
  public:
    Debugger() : IdleComponent("Debugger", 1000) {
    }
    virtual void idle(unsigned long now) {
      dprintf("%5d: wifi=%d, but=%d, card=%d, ang=%d, m1=%d", cnt, 0, button.state, card.state, int(angle.value()), int(motor1.value()));
      cnt += 1;
    }
};


Debugger debugger;
//Echo echo;

//
// Main Program
//

void setup() 
{
  init_all("Dealer");
}

bool last_connected = false;
void loop() 
{
  idle_all();
}
