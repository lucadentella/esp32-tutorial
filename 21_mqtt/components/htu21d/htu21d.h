/*
 * HTU21D Component
 *
 * esp-idf component to interface with HTU21D humidity and temperature sensor
 * by TE Connectivity (http://www.te.com/usa-en/product-CAT-HSC0004.html)
 *
 * Luca Dentella, www.lucadentella.it
 */

 
 // Error library
#include "esp_err.h"
 
// I2C driver
#include "driver/i2c.h"

// FreeRTOS (for delay)
#include "freertos/task.h"

 
#ifndef __ESP_HTU21D_H__
#define __ESP_HTU21D_H__

// sensor address
#define HTU21D_ADDR		0x40

// HTU21D commands
#define TRIGGER_TEMP_MEASURE_HOLD  		0xE3
#define TRIGGER_HUMD_MEASURE_HOLD  		0xE5
#define TRIGGER_TEMP_MEASURE_NOHOLD  	0xF3
#define TRIGGER_HUMD_MEASURE_NOHOLD  	0xF5
#define WRITE_USER_REG  				0xE6
#define READ_USER_REG  					0xE7
#define SOFT_RESET  					0xFE

// return values
#define HTU21D_ERR_OK				0x00
#define HTU21D_ERR_CONFIG			0x01
#define HTU21D_ERR_INSTALL			0x02
#define HTU21D_ERR_NOTFOUND			0x03
#define HTU21D_ERR_INVALID_ARG		0x04
#define HTU21D_ERR_FAIL		 		0x05
#define HTU21D_ERR_INVALID_STATE	0x06
#define HTU21D_ERR_TIMEOUT	 		0x07

// variables
i2c_port_t _port;

// functions
int htu21d_init(i2c_port_t port, int sda_pin, int scl_pin, gpio_pullup_t sda_internal_pullup, gpio_pullup_t scl_internal_pullup);
float ht21d_read_temperature();
float ht21d_read_humidity();
uint8_t ht21d_get_resolution();
int ht21d_set_resolution(uint8_t resolution);
int htu21d_soft_reset();

// helper functions
uint8_t ht21d_read_user_register();
int ht21d_write_user_register(uint8_t value);
uint16_t read_value(uint8_t command);
bool is_crc_valid(uint16_t value, uint8_t crc);


#endif  // __ESP_HTU21D_H__
