idf_component_register(
	SRCS
	"src/uart_link.cpp"
	INCLUDE_DIRS
	"include"

	REQUIRES
	esp_driver_uart
    esp_driver_gpio
)