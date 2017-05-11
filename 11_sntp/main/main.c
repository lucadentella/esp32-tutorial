#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "apps/sntp/sntp.h"


// Event group for inter-task communication
static EventGroupHandle_t event_group;
const int WIFI_CONNECTED_BIT = BIT0;


// Wifi event handler
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
		
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    
	case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
        break;
    
	case SYSTEM_EVENT_STA_DISCONNECTED:
		xEventGroupClearBits(event_group, WIFI_CONNECTED_BIT);
        break;
    
	default:
        break;
    }  
	return ESP_OK;
}


// Main task
void main_task(void *pvParameter)
{
	// wait for connection
	printf("Waiting for connection to the wifi network...\n ");
	xEventGroupWaitBits(event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
	printf("Connected\n\n");
	
	// initialize the SNTP service
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	sntp_setservername(0, CONFIG_SNTP_SERVER);
	sntp_init();
	
	// wait for the service to set the time
	time_t now;
	struct tm timeinfo;
	time(&now);
	localtime_r(&now, &timeinfo);
	while(timeinfo.tm_year < (2016 - 1900)) {
		
		printf("Time not set, waiting...\n");
		vTaskDelay(5000 / portTICK_PERIOD_MS);
		time(&now);
        localtime_r(&now, &timeinfo);
	}
	
	// print the actual time with different formats
	char buffer[100];
	printf("Actual UTC time:\n");
	strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
	printf("- %s\n", buffer);
	strftime(buffer, sizeof(buffer), "%A, %d %B %Y", &timeinfo);
	printf("- %s\n", buffer);
	strftime(buffer, sizeof(buffer), "Today is day %j of year %Y", &timeinfo);
	printf("- %s\n", buffer);
	printf("\n");
	
	// change the timezone to Italy
	setenv("TZ", "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", 1);
	tzset();
   	
	// print the actual time in Italy
	printf("Actual time in Italy:\n");
	localtime_r(&now, &timeinfo);
	strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
	printf("%s\n", buffer);
	
	while(1) {
		vTaskDelay(1000 / portTICK_RATE_MS);
	}	
}


// setup and start the wifi connection
void wifi_setup() {
	
	event_group = xEventGroupCreate();
		
	tcpip_adapter_init();

	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}


// Main application
void app_main()
{
	// disable the default wifi logging
	esp_log_level_set("wifi", ESP_LOG_NONE);

	nvs_flash_init();
	wifi_setup();
    xTaskCreate(&main_task, "main_task", 20000, NULL, 5, NULL);
}
