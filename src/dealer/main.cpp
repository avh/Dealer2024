// (c)2024, Arthur van Hoff, Artfahrt Inc.

#include "util.h"
#include "motor.h"
#include "angle.h"
#include "sensor.h"
#include "eject.h"
#include "network.h"
#include <ArduinoBLE.h>

#define BLE_UUID  "19b10000-e8f2-537e-4f6c-d104768a1214"
BLEService ledService(BLE_UUID); // Bluetooth® Low Energy LED Service
BLEByteCharacteristic switchCharacteristic(BLE_UUID, BLERead | BLEWrite);


// Components
Motor motor1("Motor1", M1_PIN1, M1_PIN2, 400, 10000);
Motor motor2("Motor2", M2_PIN1, M2_PIN2, 400, 10000);
//Motor fan("Fan", FN_PIN1, FN_PIN2, 100, 100000, 2000);
Motor rotator("Rotator", MR_PIN1, MR_PIN2, 400, 400);
AngleSensor angle("Angle");
IRSensor card("Card", CARD_PIN, HIGH);
Ejector ejector("Ejector");
WiFiNetwork wifi;

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
      } else {
      //} else if (deck.value()) {
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

class Echo : IdleComponent {
  public:
    Echo() : IdleComponent("Echo", 5*1000) {
    }
    virtual void init() {
    } 
    virtual void idle(unsigned long now) {
      int n = 10;
      Wire.beginTransmission(CAMERA_ADDR);
      for (int i = 0 ; i < n ; i++) {
        Wire.write(i*3);
      }
      int result = Wire.endTransmission();
      if (result != 0) {
        dprintf("command error %d", result);
        return;
      }

      Wire.requestFrom(CAMERA_ADDR, n);
      for (int i = 0 ; i < n ; i++) {
        for (;;) {
          int available = Wire.available();
          if (available > 0) {
            break;
          }
          if (available < 0) {
            dprintf("read error %d", available);
            return;
          }
        }  
        int b = Wire.read();
        if (b != i*3) {
          dprintf("echo read error, expected=%d, got=%d", i*3, b);
          return;
        }
      }
    }
};

Debugger debugger;
Echo echo;

//
// Main Program
//

void setup() 
{
  Wire.begin();
  Wire.setTimeOut(1000);

  init_all("Dealer");
}

bool last_connected = false;
void loop() 
{
  idle_all();
}
