idf_component_register(
    SRCS
        "src/gpio_test.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        test_manager
        esp_driver_gpio
)