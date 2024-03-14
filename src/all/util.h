// (c)2024, Arthur van Hoff, Artfahrt Inc.
#pragma once

#include <Arduino.h>
#include <stdarg.h>
#include <cstdio>
#include <vector>
#include <map>
#include "defs.h"

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#if USE_SERIAL
extern bool verbose;
extern void dprintf(const char *fmt, ...);
#else
#define dprintf(...)
#endif

class InitComponent {
  public:
    const char *name;
    InitComponent *next = NULL;
    static InitComponent *first;

  public:
    InitComponent(const char *name) : name(name) 
    {
      InitComponent **p = &first;
      for (; *p != NULL ; p = &(*p)->next);
      *p = this;
    }

    virtual void init() {}
    virtual void halt() {}

    static void init_all();
    static void halt_all();
};

class IdleComponent : public InitComponent {
  public:
    int interval;
    unsigned long last_idle_tm;
    IdleComponent *next = NULL;
    static IdleComponent *first;

  public:
    IdleComponent(const char *name, int interval = 1) : InitComponent(name), interval(interval), last_idle_tm(0) {
      IdleComponent **p = &first;
      for (; *p != NULL ; p = &(*p)->next);
      *p = this;
    }

    virtual void idle(unsigned long now) {}

    static void idle_all();
};

extern void init_all(const char *name);
extern void idle_all();
