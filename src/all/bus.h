// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#pragma once
#include "util.h"

class BusSlave : public IdleComponent {
  public:
    uint8_t addr;
    void (*int_handler)(BusSlave &);
    void (*cmd_handler)(BusSlave &);
    unsigned char req[32];
    int reqlen;
    unsigned char res[32];
    volatile int reslen;
  public:
    BusSlave(uint8_t addr, void (*int_handler)(BusSlave &), void (*cmd_handler)(BusSlave &)) : 
      IdleComponent("BusClient", 1000), addr(addr), int_handler(int_handler), cmd_handler(cmd_handler), reqlen(0), reslen(0) {
    } 
    virtual void init();
    virtual void idle(unsigned long now);
};

class BusMaster : public InitComponent {
  public:
    BusMaster() : InitComponent("BusMaster") {}
    virtual void init();

    bool check(uint8_t addr);
    bool request(uint8_t addr, const unsigned char *req, int reqlen, unsigned char *res=NULL, int reslen=0);
};