// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include <WiFi.h>
#include "util.h"
#include "bus.h"
#include "light.h"
#include "image.h"
#include "camera.h"
#include "webserver.h"

WebServer www;
LightArray<LIGHT_PIN> light("camera-light", 8, 60);
Camera cam(light);;
extern Image cards;
extern Image suits;

class Idler : public IdleComponent {
  public:
    int i;
  public:
    Idler() : IdleComponent("camera-idler", 10*1000) {
    }

    virtual void idle(unsigned long now) {
      dprintf("%5d: %s, wifi=%d, cam=%d, light=%d, frame=%d", i++, name, www.connected, cam.active, light.value, cam.frame_nr);
    }
} idler;

BusSlave bus(CAMERA_ADDR, [] (BusSlave &bus, BusSlave::Buffer &req, BusSlave::Buffer &res) {
  // interrupt handler, NO blocking
  switch (req[0]) {
    case CMD_CAPTURE:
      cam.last_card = CARD_NULL;
      break;
    case CMD_IDENTIFY:
      res.resize(1);
      res[0] = cam.last_card;
      break;
    case CMD_STATUS:
      res.resize(6);
      bzero(res.data(), res.size());
      res[0] = cam.last_card;
      res[1] = light.value ? 1 : 0;
      if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        res[2] = ip[0];
        res[3] = ip[1]; 
        res[4] = ip[2];
        res[5] = ip[3];
      }
      break;
    default:
      break;
  }
},[] (BusSlave &bus, BusSlave::Buffer &req) {
  // command handler, blocking is allowed, but no responses
  switch (req[0]) {
    case CMD_CLEAR:
      cam.clearCard();
      break;
    case CMD_CAPTURE:
      cam.captureCard();
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
