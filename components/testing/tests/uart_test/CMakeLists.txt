idf_component_register(
    SRCS
        "src/uart_test.cpp"

    INCLUDE_DIRS
        "include"
    
    REQUIRES
        test_manager
        esp_driver_uart
        esp_driver_gpio
)