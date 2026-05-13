idf_component_register(SRCS "app_main.cpp" "tcp_server.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES khoa_wifi_connect
                    PRIV_REQUIRES esp_driver_gpio)
