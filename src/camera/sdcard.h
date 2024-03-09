// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include "util.h"

class SDCard : public InitComponent {
  public:
    SDCard() : InitComponent("SD") {}
    virtual void init();

    static void handle_listdir(HTTP &http, const char *path, int depth = 0);

};