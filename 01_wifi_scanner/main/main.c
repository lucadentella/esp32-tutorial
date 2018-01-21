#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define MAX_APs 20


// From auth_mode code to string
static char* getAuthModeName(wifi_auth_mode_t auth_mode) {
	
	char *names[] = {"OPEN", "WEP", "WPA PSK", "WPA2 PSK", "WPA WPA2 PSK", "MAX"};
	return names[auth_mode];
}

// Empty event handler
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}

// Empty infinite task
void loop_task(void *pvParameter)
{
    while(1) { 
		vTaskDelay(1000 / portTICK_RATE_MS);		
    }
}


void app_main()
{
	// initialize NVS
	ESP_ERROR_CHECK(nvs_flash_init());
	
	// initialize the tcp stack
	tcpip_adapter_init();

	// initialize the wifi event handler
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
	
	// configure, initialize and start the wifi driver
	wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());
	
	// configure and run the scan process in blocking mode
	wifi_scan_config_t scan_config = {
		.ssid = 0,
		.bssid = 0,
		.channel = 0,
        .show_hidden = true
    };
	printf("Start scanning...");
	ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
	printf(" completed!\n");
	printf("\n");
		
	// get the list of APs found in the last scan
	uint16_t ap_num = MAX_APs;
	wifi_ap_record_t ap_records[MAX_APs];
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));
	
	// print the list 
	printf("Found %d access points:\n", ap_num);
	printf("\n");
	printf("               SSID              | Channel | RSSI |   Auth Mode \n");
	printf("----------------------------------------------------------------\n");
	for(int i = 0; i < ap_num; i++)
		printf("%32s | %7d | %4d | %12s\n", (char *)ap_records[i].ssid, ap_records[i].primary, ap_records[i].rssi, getAuthModeName(ap_records[i].authmode));
	printf("----------------------------------------------------------------\n");
	
	// infinite loop
	xTaskCreate(&loop_task, "loop_task", 2048, NULL, 5, NULL);
}
