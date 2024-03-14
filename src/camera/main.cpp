// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include "util.h"
#include "bus.h"
#include "light.h"
#include "sdcard.h"
#include "camera.h"
#include "webserver.h"

WebServer www;
SDCard sdcard;
LEDArray light("camera-light", 8, 200);
Camera cam;

class Idler : public IdleComponent {
  public:
    int i;
  public:
    Idler() : IdleComponent("camera-idler", 1000) {
    }

    virtual void idle(unsigned long now) {
      dprintf("%5d: %s, wifi=%d, light=%d, frame=%d", i++, name, www.connected, light.value, cam.frame_nr);
    }
} idler;

BusSlave bus(CAMERA_ADDR, [] (BusSlave &bus) {
  switch (bus.req[0]) {
    case CMD_CAPTURE:
      bus.res[0] = 0x13;
      bus.reslen = 1;
      return;
    default:
      return;
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
