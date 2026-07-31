#include "config.h"

namespace core {
namespace config {

// Meter index -> GPIO. Fixed for the life of the product; changing a
// row here silently rewires every installed unit on the next update.
const uint8_t RELAY_PINS[MAX_METERS] = {
  25,  // Meter 1
  26,  // Meter 2
  27,  // Meter 3
  33,  // Meter 4
  32,  // Meter 5
  13,  // Meter 6
   4,  // Meter 7
  18,  // Meter 8
  19,  // Meter 9
  23   // Meter 10
};

const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

}  // namespace config
}  // namespace core
