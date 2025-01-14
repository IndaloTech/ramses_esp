set(component_srcs "ramses_debug.c" "debug_cmd.c")

idf_component_register(
    SRCS "${component_srcs}"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES driver command
)
