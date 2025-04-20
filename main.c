#include <stdbool.h>
#include <stdint.h>

#include "nrf_gpio.h"

#include "nrf_drv_clock.h"
#include "app_timer.h"

#include "nrfx_spim.h"

enum { blink_period_on_ms = 250 };
enum { blink_period_off_ms = 1000 };

enum { pwr_toggle_period_ms = 1000 };

enum { led_pin = NRF_GPIO_PIN_MAP(0, 15) };
enum { pwr_pin = NRF_GPIO_PIN_MAP(0, 13) };

APP_TIMER_DEF(blinky_timer);
APP_TIMER_DEF(pwr_ctrl_timer);

static const nrfx_spim_t
spim_instance = NRFX_SPIM_INSTANCE(0);

static volatile bool led_is_active = true;

static void pwr_ctrl_timer_handler(void *ctx)
{
        nrf_gpio_pin_toggle(pwr_pin);
}

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

typedef enum {
    spim_init_result_success,
    spim_init_result_fail
} spim_init_result_t;

static void spim0_evt_handler(nrfx_spim_evt_t const * p_event, void *ctx)
{

}

static spim_init_result_t spim0_init(void)
{
    nrfx_err_t err_code;
    nrfx_spim_config_t config = NRFX_SPIM_DEFAULT_CONFIG;

    config.sck_pin = NRF_GPIO_PIN_MAP(0, 17);
    config.mosi_pin = NRF_GPIO_PIN_MAP(0, 20);
    config.ss_pin = NRF_GPIO_PIN_MAP(0, 22);

    config.frequency = NRF_SPIM_FREQ_1M;

    err_code = nrfx_spim_init(&spim_instance, &config, spim0_evt_handler, NULL);

    return err_code == NRFX_SUCCESS
           ? spim_init_result_success
           : spim_init_result_fail;
}

static uint8_t spim_tx_buffer[] = { 0xDE, 0xAD, 0xBE, 0xEF,
                                    0xCA, 0xFE, 0xBA, 0xBE };

int main(void)
{
    nrf_gpio_cfg_output(led_pin);
    nrf_gpio_pin_write(led_pin, 1);

    nrf_gpio_cfg_output(pwr_pin);
    nrf_gpio_pin_write(pwr_pin, 1);

    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    app_timer_init();
    app_timer_create(&blinky_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     blinky_timer_handler);

    app_timer_create(&pwr_ctrl_timer,
                     APP_TIMER_MODE_REPEATED,
                     pwr_ctrl_timer_handler);

    app_timer_start(pwr_ctrl_timer,
                    APP_TIMER_TICKS(pwr_toggle_period_ms),
                    NULL);

    if (spim0_init() == spim_init_result_success)
    {
        nrfx_spim_xfer_desc_t
        spim_xfer_desc = NRFX_SPIM_XFER_TX(spim_tx_buffer,
                                           sizeof(spim_tx_buffer));

        nrfx_err_t err_code = nrfx_spim_xfer(&spim_instance,
                                             &spim_xfer_desc,
                                             0);
        if (err_code == NRFX_SUCCESS)
        {
            app_timer_start(blinky_timer,
                            APP_TIMER_TICKS(blink_period_on_ms),
                            NULL);
        }
    }

    while (true)
    {
    }
}
