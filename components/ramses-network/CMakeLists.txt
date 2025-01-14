set( component_srcs 
        "ramses_network.c" "network_cmd.c" 
        "ramses_wifi.c" "wifi_cmd.c" 
        "ramses_ota.c" "ota_cmd.c" 
        "ramses_sntp.c")

idf_component_register(
    SRCS "${component_srcs}"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES command nvs_flash esp_wifi esp_netif console mbedtls app_update esp_partition esp_https_ota esp_http_client
)