idf_component_register(
    SRCS "pgpemu.c"
         "pgp-cert.c"
         "pgp-display.c"
         "pgp-events.c"
         "secrets.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash mbedtls esp_lcd esp_timer
    PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_driver_uart
)
