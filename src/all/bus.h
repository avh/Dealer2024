// (c)2024, Arthur van Hoff, Artfahrt Inc.
//
#pragma once
#include "util.h"

class Bus : public IdleComponent {
  public:
    Bus(const char *name) : IdleComponent(name) {
    } 
};

class BusMaster : Bus {
  public:
    void request(int addr, int cmd, void (*handle)(in));

};

class BusSlave : Bus {
  public:
    int addr;
  public:
    BusSlave(const char *name, int addr) : Bus(name), addr(addr) {
    }
};