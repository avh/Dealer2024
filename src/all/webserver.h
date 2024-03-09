// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include "util.h"
#include <WiFi.h>

#define MAX_HANDLERS 64

class HTTP {
  public:
    WiFiClient &client;
    String &method;
    String &path;
    int state = 0;
  public:
    HTTP(WiFiClient &client, String &method, String &path) : client(client), method(method), path(path) {}
    void begin(int code, const char *str);
    void printf(const char *fmt, ...);
    void end();
    int write(unsigned char *buf, int len);
};

class WebServer : public IdleComponent {
  public:
    WiFiServer server;
    bool connected = false;
    int state = 0;
  public:
    WebServer() : IdleComponent("HTTP", 100), server(80) {}
    static void add(const char *path, void (*handler)(HTTP &));
 
  public:
    virtual void init();
    virtual void idle(unsigned long now);
    static void dispatch(WiFiClient &client, String &method, String &path);
    void handle(WiFiClient &client);
};
