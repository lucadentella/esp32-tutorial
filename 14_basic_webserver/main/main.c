#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "mdns.h"

#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"


// HTTP headers and web pages
const static char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
const static char http_png_hdr[] = "HTTP/1.1 200 OK\nContent-type: image/png\n\n";
const static char http_off_hml[] = "<meta content=\"width=device-width,initial-scale=1\"name=viewport><style>div{width:230px;height:300px;position:absolute;top:0;bottom:0;left:0;right:0;margin:auto}</style><div><h1 align=center>Relay is OFF</h1><a href=on.html><img src=on.png></a></div>";
const static char http_on_hml[] = "<meta content=\"width=device-width,initial-scale=1\"name=viewport><style>div{width:230px;height:300px;position:absolute;top:0;bottom:0;left:0;right:0;margin:auto}</style><div><h1 align=center>Relay is ON</h1><a href=off.html><img src=off.png></a></div>"; 

// embedded binary data
extern const uint8_t on_png_start[] asm("_binary_on_png_start");
extern const uint8_t on_png_end[]   asm("_binary_on_png_end");
extern const uint8_t off_png_start[] asm("_binary_off_png_start");
extern const uint8_t off_png_end[]   asm("_binary_off_png_end");


// Event group for inter-task communication
static EventGroupHandle_t event_group;
const int WIFI_CONNECTED_BIT = BIT0;

// actual relay status
bool relay_status;


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

	  
static void http_server_netconn_serve(struct netconn *conn) {

	struct netbuf *inbuf;
	char *buf;
	u16_t buflen;
	err_t err;

	err = netconn_recv(conn, &inbuf);

	if (err == ERR_OK) {
	  
		netbuf_data(inbuf, (void**)&buf, &buflen);
		
		// extract the first line, with the request
		char *first_line = strtok(buf, "\n");
		
		if(first_line) {
			
			// default page
			if(strstr(first_line, "GET / ")) {
				netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
				if(relay_status) {
					printf("Sending default page, relay is ON\n");
					netconn_write(conn, http_on_hml, sizeof(http_on_hml) - 1, NETCONN_NOCOPY);
				}					
				else {
					printf("Sending default page, relay is OFF\n");
					netconn_write(conn, http_off_hml, sizeof(http_off_hml) - 1, NETCONN_NOCOPY);
				}
			}
			
			// ON page
			else if(strstr(first_line, "GET /on.html ")) {
				
				if(relay_status == false) {			
					printf("Turning relay ON\n");
					gpio_set_level(CONFIG_RELAY_PIN, 1);
					relay_status = true;
				}
				
				printf("Sending OFF page...\n");
				netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, http_on_hml, sizeof(http_on_hml) - 1, NETCONN_NOCOPY);
			}			

			// OFF page
			else if(strstr(first_line, "GET /off.html ")) {
				
				if(relay_status == true) {			
					printf("Turning relay OFF\n");
					gpio_set_level(CONFIG_RELAY_PIN, 0);
					relay_status = false;
				}
				
				printf("Sending OFF page...\n");
				netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, http_off_hml, sizeof(http_off_hml) - 1, NETCONN_NOCOPY);
			}
			
			// ON image
			else if(strstr(first_line, "GET /on.png ")) {
				printf("Sending ON image...\n");
				netconn_write(conn, http_png_hdr, sizeof(http_png_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, on_png_start, on_png_end - on_png_start, NETCONN_NOCOPY);
			}
			
			// OFF image
			else if(strstr(first_line, "GET /off.png ")) {
				printf("Sending OFF image...\n");
				netconn_write(conn, http_png_hdr, sizeof(http_png_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, off_png_start, off_png_end - off_png_start, NETCONN_NOCOPY);
			}
			
			else printf("Unkown request: %s\n", first_line);
		}
		else printf("Unkown request\n");
	}
	
	// close the connection and free the buffer
	netconn_close(conn);
	netbuf_delete(inbuf);
}

static void http_server(void *pvParameters) {
	
	struct netconn *conn, *newconn;
	err_t err;
	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn, NULL, 80);
	netconn_listen(conn);
	printf("HTTP Server listening...\n");
	do {
		err = netconn_accept(conn, &newconn);
		printf("New client connected\n");
		if (err == ERR_OK) {
			http_server_netconn_serve(newconn);
			netconn_delete(newconn);
		}
		vTaskDelay(1); //allows task to be pre-empted
	} while(err == ERR_OK);
	netconn_close(conn);
	netconn_delete(conn);
	printf("\n");
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


// configure the output PIN
void gpio_setup() {
	
	// configure the relay pin as GPIO, output
	gpio_pad_select_gpio(CONFIG_RELAY_PIN);
    gpio_set_direction(CONFIG_RELAY_PIN, GPIO_MODE_OUTPUT);
	
	// set initial status = OFF
	gpio_set_level(CONFIG_RELAY_PIN, 0);
	relay_status = false;
}


// Main application
void app_main()
{
	// disable the default wifi logging
	esp_log_level_set("wifi", ESP_LOG_NONE);

	nvs_flash_init();
	wifi_setup();
	gpio_setup();
	
	// wait for connection
	printf("Waiting for connection to the wifi network...\n ");
	xEventGroupWaitBits(event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
	printf("Connected\n\n");
	
	// print the local IP address
	tcpip_adapter_ip_info_t ip_info;
	ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
	printf("IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
	printf("Subnet mask: %s\n", ip4addr_ntoa(&ip_info.netmask));
	printf("Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw));	
	
	// run the mDNS daemon
	mdns_server_t* mDNS = NULL;
	ESP_ERROR_CHECK(mdns_init(TCPIP_ADAPTER_IF_STA, &mDNS));
	ESP_ERROR_CHECK(mdns_set_hostname(mDNS, "esp32"));
	ESP_ERROR_CHECK(mdns_set_instance(mDNS, "Basic HTTP Server"));
	printf("mDNS started\n");
	
	// start the HTTP Server task
    xTaskCreate(&http_server, "http_server", 2048, NULL, 5, NULL);
}
