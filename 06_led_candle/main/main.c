#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "soc/wdev_reg.h"
#include "xtensa/core-macros.h"

#define BLINK_GPIO CONFIG_BLINK_GPIO

uint32_t IRAM_ATTR esp_random(void) {
	
    static uint32_t last_ccount = 0;
    uint32_t ccount;
    uint32_t result = 0;
    do {
        ccount = XTHAL_GET_CCOUNT();
        result ^= REG_READ(WDEV_RND_REG);
    } while (ccount - last_ccount < XT_CLOCK_FREQ / APB_CLK_FREQ * 16);
    last_ccount = ccount;
    return result ^ REG_READ(WDEV_RND_REG);
}

uint32_t getRandomDelay() {
	
	uint32_t random = esp_random();
	return 50 + random / 9544371;
}


void blink_task(void *pvParameter) {
	
    gpio_pad_select_gpio(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    while(1) {
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(getRandomDelay() / portTICK_RATE_MS);
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(getRandomDelay() / portTICK_RATE_MS);
    }
}


void app_main()
{
    xTaskCreate(&blink_task, "blink_task", 512, NULL, 5, NULL);
}
