/*
 * serial_reporter.h — periodic status dump on the debug UART.
 *
 * Diagnostics only: nothing here affects behaviour, and no other
 * module depends on it. Output is rate-limited to one block per
 * SERIAL_PRINT_INTERVAL so it cannot flood the port or stall loop().
 */
#ifndef UTILITIES_SERIAL_REPORTER_H
#define UTILITIES_SERIAL_REPORTER_H

namespace utilities {
namespace serialreport {

// Print a status block if the interval has elapsed. Call every loop
// pass; returns immediately in between.
void printStatus();

}  // namespace serialreport
}  // namespace utilities

#endif  // UTILITIES_SERIAL_REPORTER_H
