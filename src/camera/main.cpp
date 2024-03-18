// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include <WiFi.h>
#include "util.h"
#include "bus.h"
#include "light.h"
#include "storage.h"
#include "image.h"
#include "camera.h"
#include "webserver.h"

WebServer www;
Storage storage;
LEDArray light("camera-light", 8, 200);
Camera cam;
extern Image cards;
extern Image suits;

class Idler : public IdleComponent {
  public:
    int i;
  public:
    Idler() : IdleComponent("camera-idler", 10*1000) {
    }

    virtual void idle(unsigned long now) {
      dprintf("%5d: %s, wifi=%d, store=%d, light=%d, frame=%d", i++, name, www.connected, storage.mounted, light.value, cam.frame_nr);
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
    case CMD_STATUS:
      bus.reqlen = 0;
      bus.reslen = 5;
      bzero(bus.res, bus.reslen);
      bus.res[0] = cards.data != NULL && suits.data != NULL;
      if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        bus.res[1] = ip[0];
        bus.res[2] = ip[1]; 
        bus.res[3] = ip[2];
        bus.res[4] = ip[3];
      }
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
    case CMD_COLLATE:
      cam.collate();
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
