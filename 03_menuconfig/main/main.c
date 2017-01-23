#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// Main task
void main_task(void *pvParameter)
{
	while(1) {
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}


// Main application
void app_main()
{
	
	// print the config values
	printf("Custom configuration:\n\n");
	printf("- String value:\t %s\n", CONFIG_StringValue);
	printf("- Int value:\t %d\n", CONFIG_IntValue);
	printf("- Hex value:\t %#04x\n", CONFIG_HexValue);
	printf("- Bool value:\t %s\n", CONFIG_BoolValue ? "true" : "false");
	printf("- Choice:\t");
	#ifdef CONFIG_CHOICE_1 
		printf("choice 1\n"); 
	#endif
	#ifdef CONFIG_CHOICE_2 
		printf("choice 2\n"); 
	#endif
	#ifdef CONFIG_CHOICE_3 
		printf("choice 3\n"); 
	#endif	
	
	// start the main task
    xTaskCreate(&main_task, "main_task", 2048, NULL, 5, NULL);
}
