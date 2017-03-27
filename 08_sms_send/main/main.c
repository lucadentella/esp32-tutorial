#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#define ESP_INTR_FLAG_DEFAULT 0

#define WEB_SERVER "bulksms.vsms.net"
#define WEB_PORT "80"
#define WEB_URL "http://bulksms.vsms.net/eapi/submission/send_sms/2/2.0"


// Event group for inter-task communication
static EventGroupHandle_t event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int BUTTON_PRESSED_BIT = BIT1;

// tx and rx buffers
char request_body[500];
char request[700];
char recv_buf[200];


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
	printf("Connected to %s\n\n", CONFIG_WIFI_SSID);
	
	// loop waiting for button press
	for(;;) {
		
		xEventGroupWaitBits(event_group, BUTTON_PRESSED_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
		printf("Button pressed, sending SMS...\n");
		
		// resolve the IP of the web server
		const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
		};
		struct addrinfo *res;
		int result = getaddrinfo(WEB_SERVER, WEB_PORT, &hints, &res);		
		if(result != 0 || res == NULL) {
			printf("Unable to resolve the IP of the web server\n");
			continue;
		}
		//struct in_addr *addr;	
		//addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        //printf("Web server IP = %s\n", inet_ntoa(*addr));
				
		// create a new socket
		int s = socket(res->ai_family, res->ai_socktype, 0);
		if(s < 0) {
			printf("Unable to create a socket\n");
			continue;
		}
		//printf("Socket created, ID = %d\n", s);
		
		// connect to the web server
		result = connect(s, res->ai_addr, res->ai_addrlen);
		if(result != 0) {
			printf("Unable to connect to the web server\n");
			continue;
		}
		//printf("Connected to the web server\n");
		
		// prepare the POST request (http://developer.bulksms.com/eapi/submission/send_sms/)
		sprintf(request_body, "username=%s&password=%s&message=%s&msisdn=%s", 
			CONFIG_BULKSMS_USER, CONFIG_BULKSMS_PASSWORD, CONFIG_SMS_TEXT, CONFIG_SMS_RECIPIENTS);
		sprintf(request, "POST %s HTTP/1.1\nHost: %s\nUser-Agent: ESP32\nContent-Type: application/x-www-form-urlencoded\nContent-Length: %d\n\n%s",
			WEB_URL, WEB_SERVER, strlen(request_body), request_body);
		
		// send the POST request (static + content length + body)
		//printf("Sending the request:\n%s\n", request);
		result = write(s, request, strlen(request));
		if(result < 0) {
			printf("Unable to send the POST request\n");
			continue;
		}
		//printf("Sent %d bytes\n", result);
		
		// get the response
		bzero(recv_buf, sizeof(recv_buf));
		int r;
		do {			
			r = read(s, recv_buf, sizeof(recv_buf) - 1);
		} while(r > 0);
		//printf("Response: %s\n", recv_buf);
		
		// parse the response to find return code and return message
		char *body = strstr(recv_buf, "\r\n\r\n") + 4;
		char* ret_code_string = strtok(body, "|");
		int return_code = atoi(ret_code_string);
		char *return_message = strtok(NULL, "|");
		
		// print the result
		if(return_code == 0) printf("SMS sent successfully!");
		else if(return_code == 1) printf("SMS scheduled");
		else printf("SMS send failed, error code = %d - %s\n", return_code, return_message);
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
	printf("Application started\n\n");
	
	nvs_flash_init();
	wifi_setup();
	button_setup();
    xTaskCreate(&main_task, "main_task", 2048, NULL, 5, NULL);
}
