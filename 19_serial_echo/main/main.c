// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// UART driver
#include "driver/uart.h"

// Error library
#include "esp_err.h"


// Main application
void app_main() {

	printf("UART echo\r\n\r\n");
	
	// configure the UART1 controller
	uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, 4, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0);
	
	
	uart_write_bytes(UART_NUM_1, "Ready!\r\n", 8);
	uint8_t *data = (uint8_t *) malloc(1024);

    while (1) {
        
		// Read data from the UART
        int len = uart_read_bytes(UART_NUM_1, data, 1, 20 / portTICK_RATE_MS);
        
		// Write data back to the UART
        if(len > 0) {
			data[len] = '\0';
			printf("%s", data);
			fflush(stdout);
		}
    }
}
