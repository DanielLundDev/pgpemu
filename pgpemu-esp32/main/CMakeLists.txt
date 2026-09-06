if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/secrets.c")
    message(FATAL_ERROR "Copy main/secrets.example.c to main/secrets.c and configure your device credentials before building.")
endif()

idf_component_register(
    SRCS "pgpemu.c"
         "pgp-cert.c"
         "pgp-auto-mode.c"
         "pgp-display.c"
         "pgp-events.c"
         "pgp-mood-light.c"
         "secrets.c"
    INCLUDE_DIRS "."
    REQUIRES bt nvs_flash mbedtls esp_lcd esp_timer
    PRIV_REQUIRES esp_driver_gpio esp_driver_rmt esp_driver_spi esp_driver_uart
)
