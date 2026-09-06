#include "relay.h"

#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "relay";

static int  s_gpio = -1;
static bool s_active_low;

void relay_init(int gpio, bool active_low)
{
    s_gpio = gpio;
    s_active_low = active_low;

    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << gpio,
        .mode = active_low ? GPIO_MODE_OUTPUT_OD : GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    /* Set the idle level BEFORE switching the pin to output so it never glitches ON:
     * active-low -> release (1 = Hi-Z in OD mode); active-high -> 0. */
    gpio_set_level(gpio, active_low ? 1 : 0);
    ESP_ERROR_CHECK(gpio_config(&cfg));
    gpio_set_level(gpio, active_low ? 1 : 0);
    ESP_LOGI(TAG, "GPIO%d %s, relay OFF", gpio, active_low ? "open-drain active-low" : "push-pull active-high");
}

void relay_set(bool on)
{
    if (s_gpio < 0) {
        return;
    }
    /* active-low: ON = sink (0), OFF = Hi-Z (1). active-high: ON = 1, OFF = 0. */
    gpio_set_level(s_gpio, s_active_low ? !on : on);
    ESP_LOGI(TAG, "relay %s", on ? "ON" : "OFF");
}
