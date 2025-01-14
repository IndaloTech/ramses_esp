set(component_srcs "gateway.c" "device.c")

idf_component_register(
    SRCS "${component_srcs}"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES console message ramses-mqtt gateway
)
