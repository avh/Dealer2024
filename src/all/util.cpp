// (c)2024, Arthur van Hoff, Artfahrt Inc.

#include "util.h"

#if USE_SERIAL
bool verbose = true;

void dprintf(const char *fmt, ...)
{
  if (verbose) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    Serial.write(buf);
    Serial.write("\n");
  }
}
#endif

//
// InitComponent
//

InitComponent *InitComponent::first = NULL;

void InitComponent::init_all() 
{
    for (InitComponent *p = first ; p != NULL ; p = p->next ) {
      dprintf("init %s", p->name);
      p->init();
    }
}
void InitComponent::halt_all() 
{
    for (InitComponent *p = first ; p != NULL ; p = p->next ) {
      dprintf("halt %s", p->name);
      p->halt();
    }
}

//
// IdleComponent
//
IdleComponent *IdleComponent::first = NULL;

void IdleComponent::idle_all() 
{
    bool handled = false;
    unsigned long now = millis();
    for (IdleComponent *p = first ; p != NULL ; p = p->next) {
      if (p->last_idle_tm + p->interval <= now) {
          p->idle(now);
          p->last_idle_tm = now;
          handled = true;
      }
    }
    if (!handled) {
      delayMicroseconds(10);
    }
}

//
// Helper functions
//

void init_all(const char *name) 
{
#if USE_SERIAL
  // REMIND: check if USB is available
  verbose = true;

  Serial.begin(SERIAL_SPEED);
  if (verbose) {
    for (int tm = millis() ; millis() < tm + 1000 && !Serial ; delay(1));
    // REMIND
    delay(2000);
    dprintf("\n[starting %s]\n", name);
  }
#endif

  InitComponent::init_all();
}

void idle_all() 
{
  IdleComponent::idle_all();
}
