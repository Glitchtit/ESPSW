idf_component_register(
    SRCS
        "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        driver
        app_update
        espressif__esp-zigbee-lib
        espressif__esp-zboss-lib
)
