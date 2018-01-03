#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


void loop_task(void *pvParameter)
{
    while(1) {
        
		printf("Hello World!\n");
        vTaskDelay(5000 / portTICK_RATE_MS);
    }
}

void app_main()
{
	
    xTaskCreate(&loop_task, "loop_task", 1024, NULL, 5, NULL);
}
