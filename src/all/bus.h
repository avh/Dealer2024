// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#pragma once
#include "util.h"

class BusSlave : public IdleComponent {
  public:
    uint8_t addr;
    void (*handler)(BusSlave &bus);
    unsigned char req[32];
    int reqlen;
    unsigned char res[32];
    int reslen;
    bool result_requested;
  public:
    BusSlave(uint8_t addr, void (*handler)(BusSlave &bus)) : IdleComponent("BusClient"), addr(addr), handler(handler), reqlen(0), reslen(0), result_requested(false) {
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