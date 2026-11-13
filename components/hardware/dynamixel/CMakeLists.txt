idf_component_register(
    SRCS
        "src/dynamixel_protocol2.cpp"
        "src/dynamixel_transport.cpp"
        "src/m288t.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        # esp_driver_uart
        esp_driver_gpio
        soft_uart
)
