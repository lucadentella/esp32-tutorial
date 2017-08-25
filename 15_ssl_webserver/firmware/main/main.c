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

#include "mbedtls/platform.h"
#include "mbedtls/net.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/debug.h"
#include "mbedtls/error.h"


// mbedTLS error check macro
#define MBEDTLS_ERR(x) do { \
  int retval = (x); \
  char errdesc[100]; \
  if (retval != 0) { \
	mbedtls_strerror(retval, errdesc, 100); \
    fprintf(stderr, "mbedTLS error in %s:\n%s (%d) at line %d\n", #x, errdesc, retval, __LINE__); \
    while(1) vTaskDelay(1000 / portTICK_RATE_MS); \
  } \
} while (0)


// HTTP headers and web pages
const static unsigned char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
const static unsigned char http_png_hdr[] = "HTTP/1.1 200 OK\nContent-type: image/png\n\n";
const static unsigned char http_off_hml[] = "<meta content=\"width=device-width,initial-scale=1\"name=viewport><style>div{width:230px;height:300px;position:absolute;top:0;bottom:0;left:0;right:0;margin:auto}</style><div><h1 align=center>Relay is OFF</h1><a href=on.html><img src=on.png></a></div>";
const static unsigned char http_on_hml[] = "<meta content=\"width=device-width,initial-scale=1\"name=viewport><style>div{width:230px;height:300px;position:absolute;top:0;bottom:0;left:0;right:0;margin:auto}</style><div><h1 align=center>Relay is ON</h1><a href=off.html><img src=off.png></a></div>"; 

// embedded binary data
extern const uint8_t ca_cer_start[] 		asm("_binary_ca_cer_start");
extern const uint8_t ca_cer_end[] 			asm("_binary_ca_cer_end");
extern const uint8_t espserver_cer_start[]  asm("_binary_espserver_cer_start");
extern const uint8_t espserver_cer_end[]    asm("_binary_espserver_cer_end");
extern const uint8_t espserver_key_start[]  asm("_binary_espserver_key_start");
extern const uint8_t espserver_key_end[]    asm("_binary_espserver_key_end");
extern const uint8_t on_png_start[] 		asm("_binary_on_png_start");
extern const uint8_t on_png_end[]   		asm("_binary_on_png_end");
extern const uint8_t off_png_start[] 		asm("_binary_off_png_start");
extern const uint8_t off_png_end[]   		asm("_binary_off_png_end");


// mbed TLS variables
mbedtls_ssl_config conf;
mbedtls_ssl_context ssl;
mbedtls_net_context listen_fd, client_fd;
mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
mbedtls_x509_crt srvcert;
mbedtls_x509_crt cachain;
mbedtls_pk_context pkey;

// Event group for inter-task communication
static EventGroupHandle_t event_group;
const int AP_STARTED = BIT0;

// actual relay status
bool relay_status;


// mbedTLS debug function
void my_mbedtls_debug(void *ctx, int level, const char *file, int line, const char *st) {
		
	printf("mbedtls: %s\n", st);
}

// mbedTLS write function with fragment and error management 
int ssl_write(mbedtls_ssl_context *ssl, const unsigned char *buf, size_t len) {

	int ret;
	do {
	
		ret = mbedtls_ssl_write(ssl, buf, len);
		
		// an error occurred? 
		if(ret <= 0) {
		
		if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
			else return -1;
		}
		
		// did it send all the buffer?
		if(ret == len) return 0;
		
		// if not, move the pointer at the end of the data already sent
		else {
			buf = buf + ret;
			len = len - ret;
		}
	} while(1);
}

// AP event handler
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
		
    case SYSTEM_EVENT_AP_START:
		xEventGroupSetBits(event_group, AP_STARTED);
		break;
		
	case SYSTEM_EVENT_AP_STACONNECTED:
		printf("New station connected to the AP\n");
		break;

	case SYSTEM_EVENT_AP_STADISCONNECTED:
		printf("A station disconnected from the AP\n");
		break;		
    
	default:
        break;
    }
   
	return ESP_OK;
}

	  
static void https_serve(mbedtls_net_context *client_fd) {
	
	// return variable
	int ret;
	
	// initialize SSL context
	mbedtls_ssl_init(&ssl);
	MBEDTLS_ERR(mbedtls_ssl_setup(&ssl, &conf));
	//printf("SSL initialized\n");
	
	// configure the input and output functions
	mbedtls_ssl_set_bio(&ssl, client_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
	
	// handshake
	while((ret = mbedtls_ssl_handshake(&ssl)) != 0)
		if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			char errdesc[100];
			mbedtls_strerror(ret, errdesc, 100);
			goto serve_exit;
		};
	//printf("Handshake performed\n");
	
	// read the request from the client
	unsigned char buf[1024];
	do {
		int len = sizeof(buf) - 1;
		memset(buf, 0, sizeof(buf));
		ret = mbedtls_ssl_read(&ssl, buf, len);
		if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
		if(ret <= 0) switch(ret) {
		
			case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
				printf("* peer closed connection gracefully\n");
				goto serve_exit;
				
			case MBEDTLS_ERR_NET_CONN_RESET:
				printf("* connection reset by peer\n");
				goto serve_exit;
				
			default:
				printf("* mbedtls_ssl_read returned -0x%04x\n", ret);
				goto serve_exit;
		}

		if(ret > 0) break;
	} while(1);
	
	buf[ret] = '\0';
	
	// extract the first line, with the request
	char *first_line = strtok((char *)buf, "\n");
	
	if(first_line) {
		
		// default page
		if(strstr(first_line, "GET / ")) {
			mbedtls_ssl_write(&ssl, http_html_hdr, sizeof(http_html_hdr) - 1);
			if(relay_status) {
				printf("* sending default page, relay is ON\n");
				mbedtls_ssl_write(&ssl, http_on_hml, sizeof(http_on_hml) - 1);
			}					
			else {
				printf("* sending default page, relay is OFF\n");
				mbedtls_ssl_write(&ssl, http_off_hml, sizeof(http_off_hml) - 1);
			}
		}
		
		// ON page
		else if(strstr(first_line, "GET /on.html ")) {
			
			if(relay_status == false) {			
				printf("* turning relay ON\n");
				gpio_set_level(CONFIG_RELAY_PIN, 1);
				relay_status = true;
			}
			
			printf("* sending OFF page...\n");
			mbedtls_ssl_write(&ssl, http_html_hdr, sizeof(http_html_hdr) - 1);
			mbedtls_ssl_write(&ssl, http_on_hml, sizeof(http_on_hml) - 1);
		}			

		// OFF page
		else if(strstr(first_line, "GET /off.html ")) {
			
			if(relay_status == true) {			
				printf("* turning relay OFF\n");
				gpio_set_level(CONFIG_RELAY_PIN, 0);
				relay_status = false;
			}
			
			printf("* sending OFF page...\n");
			mbedtls_ssl_write(&ssl, http_html_hdr, sizeof(http_html_hdr) - 1);
			mbedtls_ssl_write(&ssl, http_off_hml, sizeof(http_off_hml) - 1);
		}
		
		// ON image
		else if(strstr(first_line, "GET /on.png ")) {
			printf("* sending ON image...\n");
			ssl_write(&ssl, http_png_hdr, sizeof(http_png_hdr) - 1);
			ssl_write(&ssl, on_png_start, on_png_end - on_png_start);
		}
		
		// OFF image
		else if(strstr(first_line, "GET /off.png ")) {
			printf("* sending OFF image...\n");
			ssl_write(&ssl, http_png_hdr, sizeof(http_png_hdr) - 1);
			ssl_write(&ssl, off_png_start, off_png_end - off_png_start);
		}
		
		else printf("* unkown request: %s\n", first_line);
	}
	else printf("* unknown request\n");
	
	// close the connection and free the buffer
	serve_exit:
	mbedtls_ssl_close_notify(&ssl);
	mbedtls_net_free(client_fd);
	mbedtls_ssl_free(&ssl);
	printf("\n");
}


static void https_server(void *pvParameters) {
	
	// initialize mbedTLS components
	mbedtls_net_init(&listen_fd);
	mbedtls_net_init(&client_fd);
	mbedtls_ssl_config_init(&conf);
	mbedtls_ctr_drbg_init(&ctr_drbg);
	mbedtls_entropy_init(&entropy);	
	mbedtls_x509_crt_init(&srvcert);
	mbedtls_x509_crt_init(&cachain);
	mbedtls_pk_init(&pkey);
	
	// load certificates and private key
	MBEDTLS_ERR(mbedtls_x509_crt_parse(&cachain, ca_cer_start, ca_cer_end - ca_cer_start));
	MBEDTLS_ERR(mbedtls_x509_crt_parse(&srvcert, espserver_cer_start, espserver_cer_end - espserver_cer_start));
	MBEDTLS_ERR(mbedtls_pk_parse_key(&pkey, espserver_key_start, espserver_key_end - espserver_key_start, NULL, 0));

	// seed the random number generator
	MBEDTLS_ERR(mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0));
	
	// prepare the configuration
	MBEDTLS_ERR(mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER,	MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT));
	
	// apply the configuration to the random engine and set the debug function
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
	mbedtls_ssl_conf_dbg(&conf, my_mbedtls_debug, NULL);
	
	// configure CA chain and server certificate
	mbedtls_ssl_conf_ca_chain(&conf, &cachain, NULL);
	MBEDTLS_ERR(mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey));
	
	// require client authentication
	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	
	printf("- mbedTLS configured\n");
	
	// bind to the default port (443)
	MBEDTLS_ERR(mbedtls_net_bind(&listen_fd, NULL, "443", MBEDTLS_NET_PROTO_TCP));
	printf("- bind on port 443 completed\n\n");
	printf("HTTPS Server ready!\n\n");
	
	// accept incoming connections
	while(1) {
		MBEDTLS_ERR(mbedtls_net_accept(&listen_fd, &client_fd, NULL, 0, NULL));
		printf("* new client connected\n");
		
		// serve the connection
		https_serve(&client_fd);
	}
}


// setup and start SoftAP
void ap_setup() {
	
	event_group = xEventGroupCreate();
		
	tcpip_adapter_init();

	ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
	
	tcpip_adapter_ip_info_t info;
    memset(&info, 0, sizeof(info));
	IP4_ADDR(&info.ip, 192, 168, 10, 1);
    IP4_ADDR(&info.gw, 192, 168, 10, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
	printf("- Wifi interface configured\n");
	
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));
	printf("- DHCP server started\n");
	
	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
	
	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

	wifi_config_t ap_config = {
        .ap = {
            .ssid = CONFIG_AP_SSID,
            .password = CONFIG_AP_PASSWORD,
			.ssid_len = 0,
			.channel = 1,
			.authmode = WIFI_AUTH_WPA2_PSK,
			.ssid_hidden = 0,
			.max_connection = 4,
			.beacon_interval = 100,			
        },
    };
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
	printf("- SoftAP mode configured\n");
	
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
	
	printf("HTTPS Server starting...\n\n");

	// initialize the different components
	nvs_flash_init();
	ap_setup();
	gpio_setup();
	
	// wait for the AP to start
	xEventGroupWaitBits(event_group, AP_STARTED, pdFALSE, pdTRUE, portMAX_DELAY);
	printf("- Access point started\n\n");
	
	// start the HTTPS Server task
    xTaskCreate(&https_server, "https_server", 10000, NULL, 5, NULL);
}
