idf_component_register(
    SRCS "image_fetcher.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client esp_wifi esp_event esp_netif mbedtls
    EMBED_TXTFILES "cert/cert.pem"
)
