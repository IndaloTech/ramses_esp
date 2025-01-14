set(component_srcs "frame.c" "uart.c")

idf_component_register(
    SRCS "${component_srcs}"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES driver ramses-led ramses-debug cc1101 message
)
