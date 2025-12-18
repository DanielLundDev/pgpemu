idf_component_register(
    SRCS "pgpemu.c" "pgp-cert.c" "secrets.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash driver mbedtls
    PRIV_REQUIRES esp_driver_uart
)
