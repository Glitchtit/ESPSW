#include "startup.h"

bool startup_resolve(uint8_t mode, bool last_state)
{
    switch (mode) {
    case STARTUP_OFF:    return false;
    case STARTUP_ON:     return true;
    case STARTUP_TOGGLE: return !last_state;
    case STARTUP_PREVIOUS:
    default:             return last_state;
    }
}
