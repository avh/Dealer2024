// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include "util.h"

class SDCard : public InitComponent {
  public:
    bool mounted = false;
  public:
    SDCard() : InitComponent("SD") {}
    virtual void init();
};