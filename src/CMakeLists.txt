idf_component_register(SRCS "main.cpp" "state.cpp" "transport.cpp" "wifi.cpp" "api.cpp" "self_test_api.cpp" "usb_console.cpp" "boiler.cpp" "protocol.cpp" "panel.cpp" "crash_record.cpp" "pellet.cpp" "panel_history.cpp" "history_store.cpp" "device_access.cpp" "updates.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_uart esp_driver_gpio esp_driver_usb_serial_jtag esp_wifi esp_netif nvs_flash esp_http_server esp_http_client
             app_update esp_timer esp_system json mbedtls lwip mdns littlefs esp_partition spi_flash vfs bootloader_support)
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_panic_handler")

