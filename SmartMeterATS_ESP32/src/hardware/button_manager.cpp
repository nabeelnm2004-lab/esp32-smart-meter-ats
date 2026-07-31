#include "button_manager.h"

#include <Arduino.h>

#include "../core/config.h"
#include "../core/meter_controller.h"

namespace hardware {
namespace buttons {

using namespace core;

namespace {
bool          emergLast = HIGH;
unsigned long emergTime = 0;
}  // namespace

void handleButtons() {
  const unsigned long now = millis();
  const bool emergState = digitalRead(config::BTN_EMERGENCY);

  // Falling edge (button pulls to GND), debounced.
  if (emergState == LOW && emergLast == HIGH &&
      (now - emergTime) > config::BTN_DEBOUNCE_MS) {
    emergTime = now;
    controller::requestEmergencyOff("button");
  }
  emergLast = emergState;
}

}  // namespace buttons
}  // namespace hardware
