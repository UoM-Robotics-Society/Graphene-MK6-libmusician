#include "callbacks.h"

/**
 * The following callbacks are fairly optional, and it is likely many instruments will not opt to
 * utilise them, so we provide a weak default implementation.
 */

void __attribute__((weak))
lm_do_light(uint32_t time, uint8_t channel, uint8_t light, uint8_t value){};

bool __attribute__((weak))
lm_do_control(uint32_t time, uint8_t channel, uint8_t control, uint8_t value) {
    return false;
};
