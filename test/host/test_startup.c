#include <stdio.h>
#include "startup.h"

static int fails;

#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
} while (0)

int main(void)
{
    /* OFF: always off */
    CHECK(startup_resolve(STARTUP_OFF, false) == false);
    CHECK(startup_resolve(STARTUP_OFF, true)  == false);
    /* ON: always on */
    CHECK(startup_resolve(STARTUP_ON, false) == true);
    CHECK(startup_resolve(STARTUP_ON, true)  == true);
    /* TOGGLE: inverts last state */
    CHECK(startup_resolve(STARTUP_TOGGLE, false) == true);
    CHECK(startup_resolve(STARTUP_TOGGLE, true)  == false);
    /* PREVIOUS: restores last state */
    CHECK(startup_resolve(STARTUP_PREVIOUS, false) == false);
    CHECK(startup_resolve(STARTUP_PREVIOUS, true)  == true);
    /* Unknown mode value: treated as PREVIOUS */
    CHECK(startup_resolve(0x7A, false) == false);
    CHECK(startup_resolve(0x7A, true)  == true);

    printf("test_startup: %d failure(s)\n", fails);
    return fails ? 1 : 0;
}
