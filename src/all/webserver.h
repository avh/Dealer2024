// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include "util.h"
#include <WiFi.h>

#define MAX_HANDLERS 64

enum HTTPState {
    HTTP_IDLE,
    HTTP_DISPATCH,
    HTTP_HEADER,
    HTTP_BODY,
    HTTP_CLOSED,
    HTTP_TIMEOUT,
};

class HTTP {
  public:
    WiFiClient client;
    unsigned long http_tm;
    String line;
    String method;
    String path;
    std::map<String,String> hdrs;
    std::map<String,String> param;
    HTTPState state = HTTP_IDLE;
    void (*handler)(HTTP &) = NULL;
  public:
    HTTP(WiFiClient &client) : client(client), http_tm(millis()) {}
    int idle(class WebServer *server, unsigned long now);
    void header(int code, const char *str);
    void printf(const char *fmt, ...);
    void body();
    int write(unsigned char *buf, int len);
    void close();
};

class WebServer : public IdleComponent {
  public:
    WiFiServer server;
    bool connected = false;
    int state = 0;
    int nactive = 0;
    std::vector<std::unique_ptr<HTTP>> active;

  public:
    WebServer() : IdleComponent("HTTP", 100), server(80) {}
    static void add(const char *path, void (*handler)(HTTP &));
    static void file_get_handler(HTTP &http);
    static void file_put_handler(HTTP &http);

  public:
    virtual void init();
    virtual void idle(unsigned long now);
};
