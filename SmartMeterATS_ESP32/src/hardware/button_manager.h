/*
 * button_manager.h — physical button input.
 *
 * Currently one button: emergency OFF on BTN_EMERGENCY, wired to GND.
 * That pin is input-only (no internal pull-up), so an external ~10k
 * pull-up to 3V3 holds it HIGH; LOW means pressed.
 */
#ifndef HARDWARE_BUTTON_MANAGER_H
#define HARDWARE_BUTTON_MANAGER_H

namespace hardware {
namespace buttons {

// Poll and debounce. Call every loop pass; never blocks.
void handleButtons();

}  // namespace buttons
}  // namespace hardware

#endif  // HARDWARE_BUTTON_MANAGER_H
