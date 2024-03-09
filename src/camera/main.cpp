#include <Arduino.h>
#include <Wire.h>
#include "util.h"
#include "webserver.h"
#include "light.h"
#include "sdcard.h"

LEDArray<LIGHT_PIN> light("camera-light", 8, 16);
WebServer www;
SDCard sdcard;

class Idler : public IdleComponent {
  public:
    int i;
  public:
    Idler() : IdleComponent("camera-idler", 1000) {
    }

    virtual void idle(unsigned long now) {
      if (light.value) {
        light.off();
      } else {
        light.on(100);
      }
      dprintf("%5d: %s, wifi=%d, light=%d", i++, name, www.connected, light.value);
    }
} idler;

int nbytes = 0;
byte data[32];

void handleReceiveCommand(int len) {
  nbytes = 0;
  for (int i = 0 ; i < len ; i++) {
    byte b = Wire.read();
    data[nbytes++] = b;
    //dprintf("%4d:  %02x", i, b);
  }
  dprintf("received command %d bytes", len);
}

void handleRequestData() {
  Wire.write(data, nbytes);
  dprintf("request data, sending %d bytes", nbytes);
  nbytes = 0;
}

void setup() 
{
    Wire.begin(CAMERA_ADDR);
    Wire.setBufferSize(2*1024);
    Wire.onReceive(handleReceiveCommand);
    Wire.onRequest(handleRequestData);

    init_all("camera");
}

void loop() 
{
    idle_all();
}
