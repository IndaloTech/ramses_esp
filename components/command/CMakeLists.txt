set(component_srcs "cmd.c")

idf_component_register(
    SRCS "${component_srcs}"
    INCLUDE_DIRS "include"
    REQUIRES console
    PRIV_REQUIRES driver
)
