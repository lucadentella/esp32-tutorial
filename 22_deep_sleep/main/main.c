#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <driver/spi_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include "esp_sleep.h"

#include "u8g2_esp32_hal.h"

#define PIN_SDA 5
#define PIN_SCL 4

#define BUTTON_PIN	12
#define SLEEP_TIME	15000000

static RTC_DATA_ATTR int boot_count;


void app_main() {
	
	// increment boot count
	boot_count++;
	
	// verify the wakeup reason
	char wakeup_reason[20];
	switch (esp_sleep_get_wakeup_cause()) {
		
		case ESP_SLEEP_WAKEUP_TIMER: 
            strcpy(wakeup_reason, "TIMER");
            break;
        
		case ESP_SLEEP_WAKEUP_EXT0: 
			strcpy(wakeup_reason, "BUTTON");
            break;
		
		default:
			strcpy(wakeup_reason, "UNKNOWN");
            break;
	}
	
	printf("Boot count: %d\n", boot_count);
	printf("Wakeup reason: %s\n", wakeup_reason);

	// initialize the u8g2 hal
	u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
	u8g2_esp32_hal.sda = PIN_SDA;
	u8g2_esp32_hal.scl = PIN_SCL;
	u8g2_esp32_hal_init(u8g2_esp32_hal);

	// initialize the u8g2 library
	u8g2_t u8g2;
	u8g2_Setup_ssd1306_128x64_noname_f(
		&u8g2,
		U8G2_R0,
		u8g2_esp32_msg_i2c_cb,
		u8g2_esp32_msg_i2c_and_delay_cb);
	
	// set the display address
	u8x8_SetI2CAddress(&u8g2.u8x8, 0x78);
	
	// initialize the display
	u8g2_InitDisplay(&u8g2);
	
	// wake up the display
	u8g2_SetPowerSave(&u8g2, 0);
	
	// display sleep count and reason
	char display_text[20];
	sprintf(display_text, "Boot count: %d", boot_count);
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_timR14_tf);
	u8g2_DrawStr(&u8g2, 2, 17, (const char *)display_text);
	
	sprintf(display_text, "Wakeup reason:");
	u8g2_DrawStr(&u8g2, 2, 40, (const char *)display_text);
	sprintf(display_text, "%s", wakeup_reason);
	u8g2_DrawStr(&u8g2, 2, 58, (const char *)display_text);
	u8g2_SendBuffer(&u8g2);
	
	// let the user read the display
	vTaskDelay(3000 / portTICK_RATE_MS);
	
	// turn off the display
	u8g2_SetPowerSave(&u8g2, 1);
	
	
	// configure wakeup events
	esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 1);
	esp_sleep_enable_timer_wakeup(SLEEP_TIME);
	
	// enter deep sleep
	printf("sleeping...\n\n");
	rtc_gpio_pulldown_en(BUTTON_PIN);
	esp_deep_sleep_start();	
}