/* startup.h — power-on state resolver (pure logic, host-testable).
 *
 * Mode values follow the ZCL On/Off StartUpOnOff attribute (0x4003). */
#ifndef ESPSW_STARTUP_H
#define ESPSW_STARTUP_H

#include <stdbool.h>
#include <stdint.h>

#define STARTUP_OFF       0x00
#define STARTUP_ON        0x01
#define STARTUP_TOGGLE    0x02
#define STARTUP_PREVIOUS  0xFF

/* Return the relay state to apply at boot. Unknown modes behave as PREVIOUS. */
bool startup_resolve(uint8_t mode, bool last_state);

#endif /* ESPSW_STARTUP_H */
