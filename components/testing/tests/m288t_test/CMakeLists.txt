idf_component_register(
    SRCS
        "src/m288t_test.cpp"

    INCLUDE_DIRS
        "include"

    REQUIRES
        test_manager
        dynamixel
)
