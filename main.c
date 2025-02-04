#include <stdbool.h>
#include <stdint.h>
#include "nrf_delay.h"

#include "lib/my_led.h"

enum {
    blink_period_ms = 500
};

int main(void)
{

    my_led_init(0);
    my_led_off(0);

    while (true)
    {
        my_led_invert(0);
        nrf_delay_ms(blink_period_ms);
    }
}

/**
 *@}
 **/
