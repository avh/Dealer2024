// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include "util.h"
#include "bus.h"
#include "light.h"
#include "sdcard.h"
#include "camera.h"
#include "webserver.h"

WebServer www;
//SDCard sdcard;
LEDArray light("camera-light", 8, 200);
Camera cam;

class Idler : public IdleComponent {
  public:
    int i;
  public:
    Idler() : IdleComponent("camera-idler", 10*1000) {
    }

    virtual void idle(unsigned long now) {
      dprintf("%5d: %s, wifi=%d, light=%d, frame=%d", i++, name, www.connected, light.value, cam.frame_nr);
    }
} idler;

BusSlave bus(CAMERA_ADDR, [] (BusSlave &bus) {
  // interrupt handler, NO blocking
  switch (bus.req[0]) {
    case CMD_CAPTURE:
      cam.last_card = CARD_NULL;
      break;
    case CMD_IDENTIFY:
      bus.reqlen = 0;
      bus.res[0] = cam.last_card;
      bus.reslen = 1;
      break;
    default:
      break;
  }
},[] (BusSlave &bus) {
  // command handler, blocking is allowed, but no responses
  switch (bus.req[0]) {
    case CMD_CLEAR:
      cam.clearCard();
      break;
    case CMD_CAPTURE:
      cam.captureCard(bus.req[1]);
      break;
    case CMD_COMMIT:
      cam.commit();
      break;
    default:
      break;
  }
});

void setup() 
{
    init_all("camera");
}

void loop() 
{
    idle_all();
}
