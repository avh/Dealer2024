// (c)2024, Arthur van Hoff, Artfahrt Inc.
#include "sensor.h"

void IRSensor::handle_interrupt()
{
  bool state = digitalRead(card.pin) == card.active_state;
  if (state != card.state) {
    card.state = state;
    card.last_tm = millis();
  }
}