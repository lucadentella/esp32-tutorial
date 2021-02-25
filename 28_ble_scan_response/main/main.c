#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"

static esp_ble_adv_params_t ble_adv_params = {
	
	.adv_int_min = 0x20,
	.adv_int_max = 0x40,
	.adv_type = ADV_TYPE_SCAN_IND,
	.own_addr_type  = BLE_ADDR_TYPE_PUBLIC,
	.channel_map = ADV_CHNL_ALL,
	.adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t adv_data = {
	
	.include_name = true,
	.flag = ESP_BLE_ADV_FLAG_LIMIT_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
};

static uint8_t manufacturer_data[6] = {0xE5,0x02,0x01,0x01,0x01,0x01};
static esp_ble_adv_data_t scan_rsp_data = {
	
	.set_scan_rsp = true,
	.manufacturer_len = 6,
	.p_manufacturer_data = manufacturer_data,
};

static uint8_t adv_raw_data[10] = {0x09,0x09,0x4c,0x75,0x6b,0x45,0x53,0x50,0x33,0x32};
static uint8_t scan_rsp_raw_data[8] = {0x07,0xFF,0xE5,0x02,0x01,0x01,0x01,0x01};

bool adv_data_set = false;
bool scan_rsp_data_set = false;
								   
// GAP callback
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
			
		case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: 
				
			printf("ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT\n");
			adv_data_set = true;
			if(scan_rsp_data_set) esp_ble_gap_start_advertising(&ble_adv_params);
			break;
			
		case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: 
				
			printf("ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT\n");
			adv_data_set = true;
			if(scan_rsp_data_set) esp_ble_gap_start_advertising(&ble_adv_params);
			break;

		case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
		
			printf("ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT\n");
			scan_rsp_data_set = true;
			if(adv_data_set) esp_ble_gap_start_advertising(&ble_adv_params);
			break;
			
		case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
		
			printf("ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT\n");
			scan_rsp_data_set = true;
			if(adv_data_set) esp_ble_gap_start_advertising(&ble_adv_params);
			break;
		
		case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
			
			printf("ESP_GAP_BLE_ADV_START_COMPLETE_EVT\n");
			if(param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
				printf("Advertising started\n\n");
			}
			else printf("Unable to start advertising process, error code %d\n\n", param->scan_start_cmpl.status);
			break;
	
		default:
		
			printf("Event %d unhandled\n\n", event);
			break;
	}
}


void app_main() {
	
	printf("BT broadcast\n\n");
	
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
	printf("- GAP callback registered\n");
	
	// configure the adv data
	ESP_ERROR_CHECK(esp_ble_gap_set_device_name("ESP32_ScanRsp"));
	ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));
	//ESP_ERROR_CHECK(esp_ble_gap_config_adv_data_raw(adv_raw_data, 10));
	printf("- ADV data configured\n");
	
	// configure the scan response data
	ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&scan_rsp_data));
	//ESP_ERROR_CHECK(esp_ble_gap_config_scan_rsp_data_raw(scan_rsp_raw_data, 8));
	printf("- Scan response data configured\n\n");
}
