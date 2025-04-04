#include <stdbool.h>
#include <stdint.h>

#include "nrf_gpio.h"

#include "nrf_drv_clock.h"
#include "app_timer.h"

enum { blink_period_on_ms = 250 };
enum { blink_period_off_ms = 1000 };
enum { led_pin = NRF_GPIO_PIN_MAP(0, 15) };

APP_TIMER_DEF(blinky_timer);

static volatile bool led_is_active = true;

static void blinky_timer_handler(void *ctx)
{
    nrf_gpio_pin_toggle(led_pin);
    app_timer_start(blinky_timer,
                    APP_TIMER_TICKS(led_is_active
                                    ? blink_period_off_ms
                                    : blink_period_on_ms),
                    NULL);
    led_is_active = !led_is_active;
}

int main(void)
{
    nrf_gpio_cfg_output(led_pin);
    nrf_gpio_pin_write(led_pin, 1);

    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    app_timer_init();
    app_timer_create(&blinky_timer, APP_TIMER_MODE_SINGLE_SHOT, blinky_timer_handler);
    app_timer_start(blinky_timer, APP_TIMER_TICKS(blink_period_on_ms), NULL);

    while (true)
    {
    }
}
