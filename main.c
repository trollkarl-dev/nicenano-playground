#include <stdbool.h>
#include <stdint.h>

#include "nrf_drv_clock.h"
#include "app_timer.h"

#include "lib/my_led.h"

enum { blink_period_ms = 250 };

APP_TIMER_DEF(blinky_timer);

static void blinky_timer_handler(void *ctx)
{
    my_led_invert(0);
    app_timer_start(blinky_timer, APP_TIMER_TICKS(blink_period_ms), NULL);
}

int main(void)
{
    my_led_init(0);
    my_led_off(0);

    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    app_timer_init();
    app_timer_create(&blinky_timer, APP_TIMER_MODE_SINGLE_SHOT, blinky_timer_handler);
    app_timer_start(blinky_timer, APP_TIMER_TICKS(blink_period_ms), NULL);

    while (true)
    {
    }
}
