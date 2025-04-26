#include <stdbool.h>
#include <stdint.h>

#include "nrf_gpio.h"
#include "nrf_atfifo.h"

#include "nrf_drv_clock.h"
#include "app_timer.h"

#include "nrfx_spim.h"

enum { blink_period_on_ms = 250 };
enum { blink_period_off_ms = 1000 };

enum { counter_upd_period_ms = 250 };

enum { led_pin = NRF_GPIO_PIN_MAP(0, 15) };
enum { pwr_pin = NRF_GPIO_PIN_MAP(0, 13) };

enum { spim0_fifo_length = 128 };

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

typedef struct {
    max7219_reg_t reg;
    uint8_t data;
} max7219_data_portion_t;

APP_TIMER_DEF(blinky_timer);
APP_TIMER_DEF(counter_timer);

NRF_ATFIFO_DEF(spim0_fifo, max7219_data_portion_t, spim0_fifo_length);

enum { spim0_tx_buflen = 2 };
static volatile uint8_t spim0_tx_buffer[spim0_tx_buflen];

static volatile bool spim0_busy = false;

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

static void spim0_evt_handler(nrfx_spim_evt_t const * p_event, void *ctx);

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

static void max7219_put_to_queue(max7219_reg_t reg, uint8_t data)
{
    nrf_atfifo_item_put_t item_put_ctx;
    max7219_data_portion_t *data_portion;

    data_portion = nrf_atfifo_item_alloc(spim0_fifo, &item_put_ctx);

    if (data_portion != NULL)
    {
        data_portion->reg = reg;
        data_portion->data = data;

        nrf_atfifo_item_put(spim0_fifo, &item_put_ctx);
    }
}

static void max7219_write_unsafe(max7219_reg_t reg, uint8_t data)
{
    spim0_tx_buffer[0] = reg;
    spim0_tx_buffer[1] = data;

    nrfx_spim_xfer_desc_t tx_desc = NRFX_SPIM_XFER_TX(spim0_tx_buffer, spim0_tx_buflen);
    nrfx_spim_xfer(&spim_instance, &tx_desc, 0);
}

static void max7219_write(max7219_reg_t reg, uint8_t data)
{
    if (spim0_busy)
    {
        return max7219_put_to_queue(reg, data);
    }

    spim0_busy = true;
    
    max7219_write_unsafe(reg, data);
}

static void spim0_evt_handler(nrfx_spim_evt_t const * p_event, void *ctx)
{
    nrf_atfifo_item_get_t item_get_ctx;
    max7219_data_portion_t *data_portion_ptr;
    max7219_data_portion_t data_portion;

    data_portion_ptr = nrf_atfifo_item_get(spim0_fifo, &item_get_ctx);

    if (data_portion_ptr != NULL)
    {
        data_portion = *data_portion_ptr;
        nrf_atfifo_item_free(spim0_fifo, &item_get_ctx);

        max7219_write_unsafe(data_portion.reg, data_portion.data);
    }
    else
    {
        spim0_busy = false;
    }
}

enum { counter_top = 10000 };
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

    NRF_ATFIFO_INIT(spim0_fifo);

    if (spim_init_result_success == spim0_init())
    {
        app_timer_start(blinky_timer,
                        APP_TIMER_TICKS(blink_period_on_ms),
                        NULL);

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
    }

    while (true)
    {
    }
}
