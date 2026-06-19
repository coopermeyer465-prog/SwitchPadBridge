idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "." "../../.."
    PRIV_REQUIRES esp_http_server esp_netif esp_wifi nvs_flash driver esp_tinyusb
)
