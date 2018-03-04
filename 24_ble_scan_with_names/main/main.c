#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"


// array of found devices
#define MAX_DISCOVERED_DEVICES 50
esp_bd_addr_t discovered_devices[MAX_DISCOVERED_DEVICES];
int discovered_devices_num = 0;

// scan parameters
static esp_ble_scan_params_t ble_scan_params = {
		.scan_type              = BLE_SCAN_TYPE_ACTIVE,
		.own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
		.scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
		.scan_interval          = 0x50,
		.scan_window            = 0x30
	};

// check if the device was already discovered
bool alreadyDiscovered(esp_bd_addr_t address) {

	bool found = false;
	
	for(int i = 0; i < discovered_devices_num; i++) {
		
		for(int j = 0; j < ESP_BD_ADDR_LEN; j++)
			found = (discovered_devices[i][j] == address[j]);
		
		if(found) break;
	}
	
	return found;
}

// add a new device to the list
void addDevice(esp_bd_addr_t address) {
	
	discovered_devices_num++;
	if(discovered_devices_num > MAX_DISCOVERED_DEVICES) return;

	for(int i = 0; i < ESP_BD_ADDR_LEN; i++)
		discovered_devices[discovered_devices_num - 1][i] = address[i];
}

// GAP callback
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
		
		case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: 
				
			printf("ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT\n");
			if(param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS) {
				printf("Scan parameters set, start scanning for 10 seconds\n\n");
				esp_ble_gap_start_scanning(10);
			}
			else printf("Unable to set scan parameters, error code %d\n\n", param->scan_param_cmpl.status);
			break;
		
		case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
			
			printf("ESP_GAP_BLE_SCAN_START_COMPLETE_EVT\n");
			if(param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
				printf("Scan started\n\n");
			}
			else printf("Unable to start scan process, error code %d\n\n", param->scan_start_cmpl.status);
			break;
		
		case ESP_GAP_BLE_SCAN_RESULT_EVT:
			
			if(param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
				
				if(!alreadyDiscovered(param->scan_rst.bda)) {
					
					printf("ESP_GAP_BLE_SCAN_RESULT_EVT\n");
					printf("Device found: ADDR=");
					for(int i = 0; i < ESP_BD_ADDR_LEN; i++) {
						printf("%02X", param->scan_rst.bda[i]);
						if(i != ESP_BD_ADDR_LEN -1) printf(":");
					}
					
					// try to read the complete name
					uint8_t *adv_name = NULL;
					uint8_t adv_name_len = 0;
					adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
					if(adv_name) {
						printf("\nFULL NAME=");
						for(int i = 0; i < adv_name_len; i++) printf("%c", adv_name[i]);
					}
					
					printf("\n\n");
					addDevice(param->scan_rst.bda);
				}
				
			}
			else if(param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT)
				printf("Scan complete\n\n");
			break;
		
		default:
		
			printf("Event %d unhandled\n\n", event);
			break;
	}
}


void app_main() {
	
	printf("BT scan\n\n");
	
	// set components to log only errors
	esp_log_level_set("*", ESP_LOG_ERROR);
	
	// initialize nvs
	ESP_ERROR_CHECK(nvs_flash_init());
	printf("- NVS init ok\n");
	
	// release memory reserved for classic BT (not used)
	ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
	printf("- Memory for classic BT released\n");
	
	// initialize the BT controller with the default config
	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
	printf("- BT controller init ok\n");
	
	// enable the BT controller in BLE mode
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
	printf("- BT controller enabled in BLE mode\n");
	
	// initialize Bluedroid library
	esp_bluedroid_init();
    esp_bluedroid_enable();
	printf("- Bluedroid initialized and enabled\n");
	
	// register GAP callback function
	ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_gap_cb));
	printf("- GAP callback registered\n\n");
	
	// configure scan parameters
	esp_ble_gap_set_scan_params(&ble_scan_params);
}