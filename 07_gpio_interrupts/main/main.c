#include <stdio.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define ESP_INTR_FLAG_DEFAULT 0

SemaphoreHandle_t xSemaphore = NULL;
bool led_status = false;


// interrupt service routine, called when the button is pressed
void IRAM_ATTR button_isr_handler(void* arg) {
	
    // notify the button task
	xSemaphoreGiveFromISR(xSemaphore, NULL);
}

// task that will react to button clicks
void button_task(void* arg) {
	
	// infinite loop
	for(;;) {
		
		// wait for the notification from the ISR
		if(xSemaphoreTake(xSemaphore,portMAX_DELAY) == pdTRUE) {
			printf("Button pressed!\n");
			led_status = !led_status;
			gpio_set_level(CONFIG_LED_PIN, led_status);
		}
	}
}

void app_main()
{
	
	// create the binary semaphore
	xSemaphore = xSemaphoreCreateBinary();
	
	// configure button and led pins as GPIO pins
	gpio_pad_select_gpio(CONFIG_BUTTON_PIN);
	gpio_pad_select_gpio(CONFIG_LED_PIN);
	
	// set the correct direction
	gpio_set_direction(CONFIG_BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(CONFIG_LED_PIN, GPIO_MODE_OUTPUT);
	
	// enable interrupt on falling (1->0) edge for button pin
	gpio_set_intr_type(CONFIG_BUTTON_PIN, GPIO_INTR_NEGEDGE);
	
	// start the task that will handle the button
	xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
	
	// install ISR service with default configuration
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	
	// attach the interrupt service routine
	gpio_isr_handler_add(CONFIG_BUTTON_PIN, button_isr_handler, NULL);
}
