#include <stdbool.h>
#include <stdint.h>

#include "nrf_gpio.h"

#include "nrf_drv_clock.h"
#include "app_timer.h"

#include "nrfx_spim.h"

enum { blink_period_on_ms = 250 };
enum { blink_period_off_ms = 1000 };

enum { counter_upd_period_ms = 100 };

enum { led_pin = NRF_GPIO_PIN_MAP(0, 15) };
enum { pwr_pin = NRF_GPIO_PIN_MAP(0, 13) };

APP_TIMER_DEF(blinky_timer);
APP_TIMER_DEF(counter_timer);

static const nrfx_spim_t
spim_instance = NRFX_SPIM_INSTANCE(0);

static void blinky_timer_handler(void *ctx)
{
    static bool led_is_active = true;

    nrf_gpio_pin_toggle(led_pin);

    app_timer_start(blinky_timer,
                    APP_TIMER_TICKS(led_is_active
                                    ? blink_period_off_ms
                                    : blink_period_on_ms),
                    NULL);

    led_is_active = !led_is_active;
}

typedef enum {
    spim_init_result_success,
    spim_init_result_fail
} spim_init_result_t;

static spim_init_result_t spim0_init(void)
{
    nrfx_err_t err_code;
    nrfx_spim_config_t config = NRFX_SPIM_DEFAULT_CONFIG;

    config.sck_pin = NRF_GPIO_PIN_MAP(0, 17);
    config.mosi_pin = NRF_GPIO_PIN_MAP(0, 20);
    config.ss_pin = NRF_GPIO_PIN_MAP(0, 22);

    config.frequency = NRF_SPIM_FREQ_1M;

    err_code = nrfx_spim_init(&spim_instance, &config, NULL, NULL);

    return err_code == NRFX_SUCCESS
           ? spim_init_result_success
           : spim_init_result_fail;
}

static void led_init(void)
{
    nrf_gpio_cfg_output(led_pin);
    nrf_gpio_pin_write(led_pin, 1);
}

static void enable_vcc(void)
{
    nrf_gpio_cfg_output(pwr_pin);
    nrf_gpio_pin_write(pwr_pin, 0);
}

typedef enum {
    max7219_no_op = 0x00,
    max7219_digit_0,
    max7219_digit_1,
    max7219_digit_2,
    max7219_digit_3,
    max7219_digit_4,
    max7219_digit_5,
    max7219_digit_6,
    max7219_digit_7,
    max7219_decode_mode,
    max7219_intensity,
    max7219_scan_limit,
    max7219_shutdown,
    max7219_display_test = 0x0f
} max7219_reg_t;

static void max7219_write(max7219_reg_t reg, uint8_t data)
{
    uint8_t tx_buffer[] = { reg, data };
    nrfx_spim_xfer_desc_t tx_desc = NRFX_SPIM_XFER_TX(tx_buffer, sizeof(tx_buffer));
    nrfx_spim_xfer(&spim_instance, &tx_desc, 0);
}

enum { counter_top = 100000000 };
static volatile uint32_t counter = 0;

static void counter_timer_handler(void *ctx)
{
    int i;
    uint32_t data;

    counter %= counter_top;
    data = counter;

    for (i = 0; i < 8; i++)
    {
        if (data)
        {
            max7219_write(max7219_digit_0 + i, data % 10);
            data /= 10;
        }
        else
        {
            max7219_write(max7219_digit_0 + i, 0x0f);
        }
    }

    counter++;
}

int main(void)
{
    int i;

    enable_vcc();
    led_init();

    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    app_timer_init();
    app_timer_create(&blinky_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     blinky_timer_handler);

    app_timer_create(&counter_timer,
                     APP_TIMER_MODE_REPEATED,
                     counter_timer_handler);

    if (spim_init_result_success == spim0_init())
    {
        app_timer_start(blinky_timer,
                        APP_TIMER_TICKS(blink_period_on_ms),
                        NULL);
    }

    max7219_write(max7219_shutdown, 0); /* disable display */
    max7219_write(max7219_decode_mode, 0xff); /* decode all digits */
    max7219_write(max7219_intensity, 0x05); /* 11/32 intensity */
    max7219_write(max7219_scan_limit, 0x07); /* display all digits */

    for (i = 0; i < 8; i++)
    {
        max7219_write(max7219_digit_0 + i, 0x0f);
    }

    max7219_write(max7219_shutdown, 1); /* enable display */

    app_timer_start(counter_timer,
                    APP_TIMER_TICKS(counter_upd_period_ms),
                    NULL);
    while (true)
    {
    }
}
