set(component_srcs "ramses-mqtt.c")

idf_component_register(
    SRCS "${component_srcs}"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES mqtt json app_update ramses-network gateway message command
)
