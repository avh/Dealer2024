// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once
#include "util.h"
#include <WiFi.h>

class WiFiNetwork : public IdleComponent {
  public:
    WiFiServer server;
    bool connected = false;
  public:
    WiFiNetwork() : IdleComponent("WiFi", 1000), server(80) {
    }
    virtual void init();
    virtual void idle(unsigned long now);
};
