/* relay.h — relay module GPIO driver. No Zigbee or NVS knowledge. */
#ifndef ESPSW_RELAY_H
#define ESPSW_RELAY_H

#include <stdbool.h>

/* active_low: module turns ON when IN is pulled to GND. The GPIO is then driven
 * open-drain (sink = ON, Hi-Z = OFF). Otherwise push-pull, HIGH = ON. The relay is
 * left OFF after init. */
void relay_init(int gpio, bool active_low);
void relay_set(bool on);
bool relay_get(void);

#endif /* ESPSW_RELAY_H */
