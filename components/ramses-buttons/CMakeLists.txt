set(component_srcs "ramses_buttons.c")

idf_component_register(
    SRCS "${component_srcs}"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_gpio
)
