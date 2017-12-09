// system components
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"

// espmqtt library
#include "mqtt.h"

// HTU21D component include
#include "htu21d.h"

// wifi settings
#define WIFI_SSID "DntCrlWlaN"
#define WIFI_PASS "Sary<3Luky"

// Event group
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
		xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    
	default:
        break;
    }
   
	return ESP_OK;
}


// MQTT connected callback
void mqtt_connected_callback(mqtt_client *client, mqtt_event_data_t *event_data)
{
	printf(" connected!\n");
	
	// send data every 5 seconds
	while(1) {
		
		float temperature = ht21d_read_temperature();
		float humidity = ht21d_read_humidity();
		
		char temp_string[10];
		char hum_string[10];
		sprintf(temp_string, "%.1f", temperature);
		sprintf(hum_string, "%.0f", humidity);
		printf("sending %s°C - %s%%\r\n", temp_string, hum_string);
		mqtt_publish(client, "/room/temperature", temp_string, strlen(temp_string), 0, 0);
		mqtt_publish(client, "/room/humidity", hum_string, strlen(hum_string), 0, 0);
		vTaskDelay(5000 / portTICK_RATE_MS);
	}
}

// MQTT client configuration
mqtt_settings settings = {
	.host = "192.168.1.10",
	.port = 8883,
	.client_id = "espmqtt",
	.clean_session = 0,
	.keepalive = 120,
	.connected_cb = mqtt_connected_callback
};


// Main application
void app_main() {
	
	// log only errors
	esp_log_level_set("*", ESP_LOG_ERROR);

	printf("MQTT temperature and humidity sensor\r\n\r\n");
	
	// setup the sensor
	int ret = htu21d_init(I2C_NUM_0, 4, 16, GPIO_PULLUP_ENABLE, GPIO_PULLUP_ENABLE);
	if(ret != HTU21D_ERR_OK) {	
		printf("Error %d when initializing HTU21D component\r\n", ret);
		while(1);
	}
	printf("HTU21D component initialized\r\n");
	
	// connect to the wifi network
	nvs_flash_init();
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
	printf("waiting for wifi network...");
	xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
	printf(" connected!\n");
	
	// start the MQTT client
	printf("Connecting to the MQTT server... ");
	mqtt_start(&settings);
}
