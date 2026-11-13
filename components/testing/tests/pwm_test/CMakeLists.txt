idf_component_register(
    SRCS 
        "src/pwm_test.cpp"
    
    INCLUDE_DIRS  
        "include"

    REQUIRES 
        test_manager
        esp_driver_ledc
        esp_driver_gpio
)