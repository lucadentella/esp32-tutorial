#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

#include "freshen.h"

#define WIFI_SSID			"type_your_wifi_ssid"
#define WIFI_PASS			"type_your_wifi_password"

#define FIRMWARE_VERSION	"1.0"
#define ACCESS_TOKEN 		"type_your_access_token"


// Event group for wifi connection
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

// Wifi event handler
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
		
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    
	case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    
	case SYSTEM_EVENT_STA_DISCONNECTED:
		esp_wifi_connect();
        break;
    
	default:
        break;
    }
   
	return ESP_OK;
}

void freshen_task(void *pvParameter) {

	while(1) {
		
		freshen_loop(FIRMWARE_VERSION, ACCESS_TOKEN);
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
}

void app_main() {
	
	// log only errors
	//esp_log_level_set("*", ESP_LOG_ERROR);
	
	printf("Freshen demo, version %s\n\n", FIRMWARE_VERSION);
	
	// initialize NVS, required for wifi
	ESP_ERROR_CHECK(nvs_flash_init());
	printf("NVS initialized\n");
		
	// connect to the wifi network
	wifi_event_group = xEventGroupCreate();
	tcpip_adapter_init();
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	printf("Connecting to the wifi network\n");
	
	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
	printf("Connected to %s\n", WIFI_SSID);
	
	// start the freshen update task
	xTaskCreate(&freshen_task, "freshen_task", 10000, NULL, 5, NULL);
}