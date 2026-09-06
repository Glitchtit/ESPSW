/* store.h — persistent relay state and startup mode (NVS namespace "espsw"). */
#ifndef ESPSW_STORE_H
#define ESPSW_STORE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Initialise NVS (erasing + retrying on version mismatch / no free pages) and open
 * the "espsw" namespace. Must be called before any other store_* function. */
esp_err_t store_init(void);

bool    store_get_state(void);           /* default false */
void    store_set_state(bool on);
uint8_t store_get_startup(void);         /* default STARTUP_PREVIOUS (0xFF) */
void    store_set_startup(uint8_t mode);

#endif /* ESPSW_STORE_H */
