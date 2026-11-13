idf_component_register(
	SRCS
	"main.cpp"
	"app/app.cpp"

	INCLUDE_DIRS
	"."
	"app"
    "config"

	REQUIRES
	uart_link
	protocol
	test_manager
	# gpio_test
	i2c_test
	encoder_test
	uart_test
	pwm_test
)
