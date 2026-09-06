/* Zigbee router transport (see zigbee.h). Built on esp-zigbee-sdk. */
#include "zigbee.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_zigbee_core.h"
#include "sdkconfig.h"

#include "proto.h"
#include "relay.h"
#include "startup.h"
#include "store.h"

static const char *TAG = "zigbee";

#define ZB_TASK_STACK        6144
#define STEER_BACKOFF_MAX_MS 60000
#define LED_BLINK_PERIOD_US  250000 /* 2 Hz blink -> toggle every 250 ms */

static bool     s_initial_on_off;
static uint8_t  s_startup_attr;     /* ZCL StartUpOnOff storage */
static bool     s_joined;
static uint32_t s_steer_backoff_ms = 1000;

/* --- Identify LED --- */

static esp_timer_handle_t s_led_timer;
static bool s_led_level;

static void led_write(bool on)
{
#if CONFIG_ESPSW_LED_GPIO >= 0
    gpio_set_level(CONFIG_ESPSW_LED_GPIO, on ? 0 : 1); /* active-low */
#else
    (void)on;
#endif
}

static void led_init(void)
{
#if CONFIG_ESPSW_LED_GPIO >= 0
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << CONFIG_ESPSW_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
    led_write(false);
#endif
}

static void led_blink_cb(void *arg)
{
    (void)arg;
    s_led_level = !s_led_level;
    led_write(s_led_level);
}

/* Called by the stack when the Identify cluster's identify_time becomes non-zero (1)
 * or returns to zero (0). */
static void identify_notify(uint8_t identify_on)
{
    if (identify_on) {
        ESP_LOGI(TAG, "identify: blinking LED");
        esp_timer_start_periodic(s_led_timer, LED_BLINK_PERIOD_US);
    } else {
        ESP_LOGI(TAG, "identify: done");
        esp_timer_stop(s_led_timer);
        s_led_level = false;
        led_write(false);
    }
}

/* --- inbound attribute writes (On/Off cluster) --- */

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t cb_id, const void *msg)
{
    switch (cb_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID: {
        const esp_zb_zcl_set_attr_value_message_t *m = msg;
        if (m->info.dst_endpoint != ESPSW_ENDPOINT ||
            m->info.cluster != ESP_ZB_ZCL_CLUSTER_ID_ON_OFF || !m->attribute.data.value) {
            break;
        }
        if (m->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID &&
            m->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
            bool on = *(const bool *)m->attribute.data.value;
            relay_set(on);
            store_set_state(on);
        } else if (m->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF) {
            uint8_t mode = *(const uint8_t *)m->attribute.data.value;
            ESP_LOGI(TAG, "startup mode -> 0x%02x", mode);
            store_set_startup(mode);
        }
        break;
    }
    /* OTA: Task 6 adds ESP_ZB_CORE_OTA_UPGRADE_VALUE_CB_ID -> ota_handle_value(msg) here. */
    default:
        ESP_LOGD(TAG, "unhandled action 0x%x", cb_id);
        break;
    }
    return ESP_OK;
}

/* --- endpoint construction --- */

/* Build a ZCL char string ([len][bytes]) into dst (>= 1 + strlen(src), max 32). */
static void zcl_string(uint8_t *dst, const char *src)
{
    size_t n = strlen(src);
    if (n > 32) {
        n = 32;
    }
    dst[0] = (uint8_t)n;
    memcpy(dst + 1, src, n);
}

static esp_zb_cluster_list_t *build_clusters(void)
{
    esp_zb_basic_cluster_cfg_t basic_cfg = {
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,
        .power_source = 0x01, /* mains (single phase) */
    };
    esp_zb_attribute_list_t *basic = esp_zb_basic_cluster_create(&basic_cfg);
    static uint8_t manuf[1 + 32];
    static uint8_t model[1 + 32];
    zcl_string(manuf, ESPSW_MANUF_NAME);
    zcl_string(model, ESPSW_MODEL);
    esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, manuf);
    esp_zb_basic_cluster_add_attr(basic, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, model);

    esp_zb_identify_cluster_cfg_t identify_cfg = {.identify_time = 0};
    esp_zb_attribute_list_t *identify = esp_zb_identify_cluster_create(&identify_cfg);

    esp_zb_on_off_cluster_cfg_t on_off_cfg = {.on_off = s_initial_on_off};
    esp_zb_attribute_list_t *on_off = esp_zb_on_off_cluster_create(&on_off_cfg);
    s_startup_attr = store_get_startup();
    ESP_ERROR_CHECK(esp_zb_on_off_cluster_add_attr(on_off, ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF,
                                                   &s_startup_attr));

    esp_zb_cluster_list_t *cl = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(cl, basic, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(cl, identify, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(cl, on_off, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    /* OTA: Task 6 adds esp_zb_cluster_list_add_ota_cluster(cl, ota_cluster_create(), CLIENT_ROLE). */
    return cl;
}

/* --- stack lifecycle --- */

static void on_joined(void)
{
    s_joined = true;
    s_steer_backoff_ms = 1000;
    ESP_LOGI(TAG, "on network: pan 0x%04hx, channel %d, short 0x%04hx",
             esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
    /* OTA: Task 6 adds ota_mark_valid(); ota_start(ESPSW_ENDPOINT); here. */
}

static void start_steering(uint8_t mode_mask)
{
    esp_zb_bdb_start_top_level_commissioning(mode_mask);
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "stack started, begin commissioning");
        start_steering(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "factory new; starting network steering");
                start_steering(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "rejoined existing network");
                on_joined();
            }
        } else {
            ESP_LOGW(TAG, "commissioning init failed (%s); retrying", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)start_steering,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "joined network successfully");
            on_joined();
        } else {
            ESP_LOGW(TAG, "steering failed; retrying in %u ms", (unsigned)s_steer_backoff_ms);
            esp_zb_scheduler_alarm((esp_zb_callback_t)start_steering,
                                   ESP_ZB_BDB_MODE_NETWORK_STEERING, s_steer_backoff_ms);
            s_steer_backoff_ms *= 2;
            if (s_steer_backoff_ms > STEER_BACKOFF_MAX_MS) {
                s_steer_backoff_ms = STEER_BACKOFF_MAX_MS;
            }
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_LEAVE: {
        /* Coordinator removed us ("Remove device" in Z2M) or a local reset finished.
         * Wipe zb_storage and reboot; the next boot is factory-new and steers again.
         * The relay keeps its stored state across this. */
        const esp_zb_zdo_signal_leave_params_t *p = esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGW(TAG, "left network (type %u); factory reset + reboot", p ? p->leave_type : 0);
        s_joined = false;
        esp_zb_factory_reset(); /* erases zb_storage and restarts */
        break;
    }
    default:
        ESP_LOGD(TAG, "signal 0x%x status %s", sig_type, esp_err_to_name(err_status));
        break;
    }
}

static void zb_task(void *arg)
{
    (void)arg;
    esp_zb_platform_config_t platform = {
        .radio_config = {.radio_mode = ZB_RADIO_MODE_NATIVE},
        .host_config = {.host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE},
    };
    ESP_ERROR_CHECK(esp_zb_platform_config(&platform));

    esp_zb_cfg_t zb_cfg = {
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,
        .install_code_policy = false,
        .nwk_cfg.zczr_cfg = {.max_children = 10},
    };
    esp_zb_init(&zb_cfg);

    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t ep_cfg = {
        .endpoint = ESPSW_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID,
        .app_device_version = 0,
    };
    esp_zb_ep_list_add_ep(ep_list, build_clusters(), ep_cfg);
    esp_zb_device_register(ep_list);

    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_identify_notify_handler_register(ESPSW_ENDPOINT, identify_notify);
    esp_zb_set_primary_network_channel_set(ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK);

    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

void zigbee_start(bool initial_on_off)
{
    s_initial_on_off = initial_on_off;

    led_init();
    const esp_timer_create_args_t targs = {
        .callback = led_blink_cb,
        .name = "espsw_led",
    };
    ESP_ERROR_CHECK(esp_timer_create(&targs, &s_led_timer));

    xTaskCreate(zb_task, "espsw_zb", ZB_TASK_STACK, NULL, 5, NULL);
}
