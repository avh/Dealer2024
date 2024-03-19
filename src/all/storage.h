// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include "util.h"

class Storage : public InitComponent {
  public:
    bool mounted = false;
  public:
    Storage() : InitComponent("Storage") {}
    virtual void init();
};