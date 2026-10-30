set(component_srcs "frame.c" "uart.c")

idf_component_register(
    SRCS "${component_srcs}"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_uart esp_driver_gpio
    PRIV_REQUIRES ramses-led ramses-debug cc1101 message
)
