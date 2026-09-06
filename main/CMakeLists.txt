idf_component_register(
    SRCS
        "main.c"
        "startup.c"
        "relay.c"
        "store.c"
        "zigbee.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        driver
        esp_timer
        app_update
        espressif__esp-zigbee-lib
        espressif__esp-zboss-lib
)
