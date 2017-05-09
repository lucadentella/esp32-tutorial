#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "esp32_ifttt_maker.h"

#define ESP_INTR_FLAG_DEFAULT 0


// Event group for inter-task communication
static EventGroupHandle_t event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int BUTTON_PRESSED_BIT = BIT1;



// button ISR
void IRAM_ATTR button_isr_handler(void* arg) {
	
	xEventGroupSetBitsFromISR(event_group, BUTTON_PRESSED_BIT, NULL);
}

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
	
	// loop waiting for button press
	for(;;) {
		
		int ret;
		
		xEventGroupWaitBits(event_group, BUTTON_PRESSED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
		printf("Button pressed, sending events...\n");
		
		printf("Sending HELLO event without values... ");
		fflush(stdout);
		if((ret = ifttt_maker_trigger("hello")) == IFTTT_MAKER_OK) printf("success :)\n");
		else printf("error (%d) :(\n", ret);
		
		printf("Sending ALARM event with a value... ");
		fflush(stdout);
		char* value[] = {"door is open"};		
		if((ret = ifttt_maker_trigger_values("alarm", value, 1)) == IFTTT_MAKER_OK) printf("success :)\n");
		else printf("error (%d) :(\n", ret);
	}
}


// initialize the button
void button_setup() {
	
	gpio_pad_select_gpio(CONFIG_BUTTON_PIN);
	gpio_set_direction(CONFIG_BUTTON_PIN, GPIO_MODE_INPUT);
	gpio_set_intr_type(CONFIG_BUTTON_PIN, GPIO_INTR_NEGEDGE);
	
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	gpio_isr_handler_add(CONFIG_BUTTON_PIN, button_isr_handler, NULL);	
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
	button_setup();
	ifttt_maker_init("key");
    xTaskCreate(&main_task, "main_task", 20000, NULL, 5, NULL);
}
