idf_component_register(
    SRCS
        "src/i2c_test.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        test_manager
        esp_driver_i2c
        esp_driver_gpio
)