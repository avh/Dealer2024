// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#pragma once
#include "util.h"

//
// An IC2 bus slave must handle commands that have a response in the
// interrupt handler and must be strictly non-blocking. 
// Commands that are blocking must be handled in the command handler, 
// and therefore can not have a response.
//

class BusSlave : public IdleComponent {
  public:
   typedef std::vector<unsigned char> Buffer;
  public:
    uint8_t addr;
    void (*int_handler)(BusSlave &, Buffer &, Buffer &);
    void (*cmd_handler)(BusSlave &, Buffer &);
    std::vector<Buffer> pending;
    Buffer response;
  public:
    BusSlave(uint8_t addr, void (*int_handler)(BusSlave &, Buffer &, Buffer &), void (*cmd_handler)(BusSlave &, Buffer &)) : 
      IdleComponent("BusClient", 1000), addr(addr), int_handler(int_handler), cmd_handler(cmd_handler) {
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