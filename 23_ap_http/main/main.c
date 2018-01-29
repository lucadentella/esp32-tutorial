#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "spiffs_vfs.h"


// set AP CONFIG values
#ifdef CONFIG_AP_HIDE_SSID
	#define CONFIG_AP_SSID_HIDDEN 1
#else
	#define CONFIG_AP_SSID_HIDDEN 0
#endif	
#ifdef CONFIG_WIFI_AUTH_OPEN
	#define CONFIG_AP_AUTHMODE WIFI_AUTH_OPEN
#endif
#ifdef CONFIG_WIFI_AUTH_WEP
	#define CONFIG_AP_AUTHMODE WIFI_AUTH_WEP
#endif
#ifdef CONFIG_WIFI_AUTH_WPA_PSK
	#define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA_PSK
#endif
#ifdef CONFIG_WIFI_AUTH_WPA2_PSK
	#define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA2_PSK
#endif
#ifdef CONFIG_WIFI_AUTH_WPA_WPA2_PSK
	#define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA_WPA2_PSK
#endif
#ifdef CONFIG_WIFI_AUTH_WPA2_ENTERPRISE
	#define CONFIG_AP_AUTHMODE WIFI_AUTH_WPA2_ENTERPRISE
#endif

// static headers for HTTP responses
const static char http_html_hdr[] = "HTTP/1.1 200 OK\n\n";
const static char http_404_hdr[] = "HTTP/1.1 404 NOT FOUND\n\n";

// Event group
static EventGroupHandle_t event_group;
const int STA_CONNECTED_BIT = BIT0;

// prototypes
static esp_err_t event_handler(void *ctx, system_event_t *event);
void ap_monitor_task(void *pvParameter);
static void http_server(void *pvParameters);
static void http_server_netconn_serve(struct netconn *conn);
void spiffs_serve(char* resource, struct netconn *conn);


// AP event handler
static esp_err_t event_handler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
		
    case SYSTEM_EVENT_AP_START:
	
		printf("- Wifi adapter started\n\n");
		
		// create and configure the mDNS service
		mdns_server_t* mDNS = NULL;
		ESP_ERROR_CHECK(mdns_init(TCPIP_ADAPTER_IF_AP, &mDNS));
		ESP_ERROR_CHECK(mdns_set_hostname(mDNS, "esp32web"));
		ESP_ERROR_CHECK(mdns_set_instance(mDNS, "ESP32 webserver"));
		printf("- mDNS service started\n");
		
		// start the HTTP server task
		xTaskCreate(&http_server, "http_server", 20000, NULL, 5, NULL);
		printf("- HTTP server started\n");

		break;
		
	case SYSTEM_EVENT_AP_STACONNECTED:
	
		xEventGroupSetBits(event_group, STA_CONNECTED_BIT);
		break;		
    
	default:
        break;
    }
   
	return ESP_OK;
}


// AP monitor task, receive Wifi AP events
void ap_monitor_task(void *pvParameter) {
	
	while(1) {
		
		xEventGroupWaitBits(event_group, STA_CONNECTED_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
		printf("New station connected\n");
	}
}


// HTTP server task
static void http_server(void *pvParameters) {
	
	struct netconn *conn, *newconn;
	err_t err;
	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn, NULL, 80);
	netconn_listen(conn);
	printf("* HTTP Server listening\n");
	do {
		err = netconn_accept(conn, &newconn);
		if (err == ERR_OK) {
			http_server_netconn_serve(newconn);
			netconn_delete(newconn);
		}
		vTaskDelay(10);
	} while(err == ERR_OK);
	netconn_close(conn);
	netconn_delete(conn);
}

static void http_server_netconn_serve(struct netconn *conn) {

	struct netbuf *inbuf;
	char *buf;
	u16_t buflen;
	err_t err;

	err = netconn_recv(conn, &inbuf);

	if (err == ERR_OK) {
	  
		// get the request and terminate the string
		netbuf_data(inbuf, (void**)&buf, &buflen);
		buf[buflen] = '\0';
		
		// get the request body and the first line
		char* body = strstr(buf, "\r\n\r\n");
		char *request_line = strtok(buf, "\n");
		
		if(request_line) {
			
			// default page -> redirect to index.html
			if(strstr(request_line, "GET / ")) {
				spiffs_serve("/index.html", conn);
			}
			// static content, get it from SPIFFS
			else {
				
				// get the requested resource
				char* method = strtok(request_line, " ");
				char* resource = strtok(NULL, " ");
				spiffs_serve(resource, conn);
			}
		}
	}
}

// serve static content from SPIFFS
void spiffs_serve(char* resource, struct netconn *conn) {
						
	// check if it exists on SPIFFS
	char full_path[100];
	sprintf(full_path, "/spiffs%s", resource);
	printf("+ Serving static resource: %s\n", full_path);
	struct stat st;
	if (stat(full_path, &st) == 0) {
		netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
		
		// open the file for reading
		FILE* f = fopen(full_path, "r");
		if(f == NULL) {
			printf("Unable to open the file %s\n", full_path);
			return;
		}
		
		// send the file content to the client
		char buffer[500];
		int size = 0;
		/*while(fgets(buffer, 500, f)) {
			netconn_write(conn, buffer, strlen(buffer), NETCONN_NOCOPY);
		}*/
		while(!feof(f)) {
			
			size_t char_read;
			char_read = fread(buffer, sizeof(char), 500, f);
			size += char_read / sizeof(char);
			netconn_write(conn, buffer, char_read / sizeof(char), NETCONN_NOCOPY);	
		}
		fclose(f);
		fflush(stdout);
		printf("+ served %d bytes\n", size);
	}
	else {
		netconn_write(conn, http_404_hdr, sizeof(http_404_hdr) - 1, NETCONN_NOCOPY);
	}
}

// Main application
void app_main()
{	
	
	// disable the default wifi logging
	esp_log_level_set("wifi", ESP_LOG_NONE);
	
	printf("ESP32 SoftAP HTTP Server\n\n");
	
	// create the event group to handle wifi events
	event_group = xEventGroupCreate();
	
	// initialize NVS
	ESP_ERROR_CHECK(nvs_flash_init());
	printf("- NVS initialized\n");
	
	// initialize SPIFFS
	vfs_spiffs_register();
	printf("- SPIFFS VFS module registered\n");
		
	// initialize the tcp stack
	tcpip_adapter_init();
	printf("- TCP adapter initialized\n");

	// stop DHCP server
	ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
	printf("- DHCP server stopped\n");
	
	// assign a static IP to the network interface
	tcpip_adapter_ip_info_t info;
    memset(&info, 0, sizeof(info));
	IP4_ADDR(&info.ip, 192, 168, 1, 1);
    IP4_ADDR(&info.gw, 192, 168, 1, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
	printf("- TCP adapter configured with IP 192.168.1.1/24\n");
	
	// start the DHCP server   
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
	printf("- DHCP server started\n");
	
	// initialize the wifi event handler
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
	printf("- Event loop initialized\n");
	
	// initialize the wifi stack in AccessPoint mode with config in RAM
	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
	printf("- Wifi adapter configured in SoftAP mode\n");

	// configure the wifi connection and start the interface
	wifi_config_t ap_config = {
        .ap = {
            .ssid = CONFIG_AP_SSID,
            .password = CONFIG_AP_PASSWORD,
			.ssid_len = 0,
			.channel = CONFIG_AP_CHANNEL,
			.authmode = CONFIG_AP_AUTHMODE,
			.ssid_hidden = CONFIG_AP_SSID_HIDDEN,
			.max_connection = CONFIG_AP_MAX_CONNECTIONS,
			.beacon_interval = CONFIG_AP_BEACON_INTERVAL,			
        },
    };
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
	printf("- Wifi network settings applied\n");
	
    
	// start the wifi interface
	ESP_ERROR_CHECK(esp_wifi_start());
	printf("- Wifi adapter starting...\n");
	
	// start the tasks
    xTaskCreate(&ap_monitor_task, "ap_monitor_task", 2048, NULL, 5, NULL);
}
