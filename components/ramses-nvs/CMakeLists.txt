set(component_srcs "ramses_nvs.c")

idf_component_register(
    SRCS "${component_srcs}"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES console nvs_flash command
)
