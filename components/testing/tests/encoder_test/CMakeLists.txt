idf_component_register(
    SRCS
    "src/encoder_test.cpp"

    INCLUDE_DIRS
    "include"

    REQUIRES
    test_manager
    esp_driver_gpio
    esp_driver_mcpwm
)
