/*
 * HTU21D Component
 *
 * esp-idf component to interface with HTU21D humidity and temperature sensor
 * by TE Connectivity (http://www.te.com/usa-en/product-CAT-HSC0004.html)
 *
 * Luca Dentella, www.lucadentella.it
 */
 
 
// Component header file
#include "htu21d.h"

int htu21d_init(i2c_port_t port, int sda_pin, int scl_pin,  gpio_pullup_t sda_internal_pullup,  gpio_pullup_t scl_internal_pullup) {
	
	esp_err_t ret;
	_port = port;
	
	// setup i2c controller
	i2c_config_t conf;
	conf.mode = I2C_MODE_MASTER;
	conf.sda_io_num = sda_pin;
	conf.scl_io_num = scl_pin;
	conf.sda_pullup_en = sda_internal_pullup;
	conf.scl_pullup_en = scl_internal_pullup;
	conf.master.clk_speed = 100000;
	ret = i2c_param_config(port, &conf);
	if(ret != ESP_OK) return HTU21D_ERR_CONFIG;
	
	// install the driver
	ret = i2c_driver_install(port, I2C_MODE_MASTER, 0, 0, 0);
	if(ret != ESP_OK) return HTU21D_ERR_INSTALL;
	
	// verify if a sensor is present
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_stop(cmd);
	if(i2c_master_cmd_begin(port, cmd, 1000 / portTICK_RATE_MS) != ESP_OK)
		return HTU21D_ERR_NOTFOUND;
	
	return HTU21D_ERR_OK;
}

float ht21d_read_temperature() {

	// get the raw value from the sensor
	uint16_t raw_temperature = read_value(TRIGGER_TEMP_MEASURE_NOHOLD);
	if(raw_temperature == 0) return -999;
	
	// return the real value, formula in datasheet
	return (raw_temperature * 175.72 / 65536.0) - 46.85;
}

float ht21d_read_humidity() {

	// get the raw value from the sensor
	uint16_t raw_humidity = read_value(TRIGGER_HUMD_MEASURE_NOHOLD);
	if(raw_humidity == 0) return -999;
	
	// return the real value, formula in datasheet
	return (raw_humidity * 125.0 / 65536.0) - 6.0;
}

uint8_t ht21d_get_resolution() {

	uint8_t reg_value = ht21d_read_user_register();
	return reg_value & 0b10000001;
}

int ht21d_set_resolution(uint8_t resolution) {
	
	// get the actual resolution
	uint8_t reg_value = ht21d_read_user_register();
	reg_value &= 0b10000001;
	
	// update the register value with the new resolution
	resolution &= 0b10000001;
	reg_value |= resolution;
	
	return ht21d_write_user_register(reg_value);
}

int htu21d_soft_reset() {
	
	esp_err_t ret;

	// send the command
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, SOFT_RESET, true);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	
	switch(ret) {
		
		case ESP_ERR_INVALID_ARG:
			return HTU21D_ERR_INVALID_ARG;
			
		case ESP_FAIL:
			return HTU21D_ERR_FAIL;
		
		case ESP_ERR_INVALID_STATE:
			return HTU21D_ERR_INVALID_STATE;
		
		case ESP_ERR_TIMEOUT:
			return HTU21D_ERR_TIMEOUT;
	}
	return HTU21D_ERR_OK;
}

uint8_t ht21d_read_user_register() {
	
	esp_err_t ret;
	
	// send the command
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, READ_USER_REG, true);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	if(ret != ESP_OK) return 0;
	
	// receive the answer
	uint8_t reg_value;
	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_READ, true);
	i2c_master_read_byte(cmd, &reg_value, 0x01);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	if(ret != ESP_OK) return 0;
	
	return reg_value;
}

int ht21d_write_user_register(uint8_t value) {
	
	esp_err_t ret;
	
	// send the command
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, WRITE_USER_REG, true);
	i2c_master_write_byte(cmd, value, true);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	
	switch(ret) {
		
		case ESP_ERR_INVALID_ARG:
			return HTU21D_ERR_INVALID_ARG;
			
		case ESP_FAIL:
			return HTU21D_ERR_FAIL;
		
		case ESP_ERR_INVALID_STATE:
			return HTU21D_ERR_INVALID_STATE;
		
		case ESP_ERR_TIMEOUT:
			return HTU21D_ERR_TIMEOUT;
	}
	return HTU21D_ERR_OK;
}

uint16_t read_value(uint8_t command) {
	
	esp_err_t ret;
	
	// send the command
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, command, true);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	if(ret != ESP_OK) return 0;
	
	// wait for the sensor (50ms)
	vTaskDelay(50 / portTICK_RATE_MS);
	
	// receive the answer
	uint8_t msb, lsb, crc;
	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (HTU21D_ADDR << 1) | I2C_MASTER_READ, true);
	i2c_master_read_byte(cmd, &msb, 0x00);
	i2c_master_read_byte(cmd, &lsb, 0x00);
	i2c_master_read_byte(cmd, &crc, 0x01);
	i2c_master_stop(cmd);
	ret = i2c_master_cmd_begin(_port, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	if(ret != ESP_OK) return 0;
	
	uint16_t raw_value = ((uint16_t) msb << 8) | (uint16_t) lsb;
	if(!is_crc_valid(raw_value, crc)) printf("CRC invalid\r\n");
	return raw_value & 0xFFFC;
}

// verify the CRC, algorithm in the datasheet (see comments below)
bool is_crc_valid(uint16_t value, uint8_t crc) {
	
	// line the bits representing the input in a row (first data, then crc)
	uint32_t row = (uint32_t)value << 8;
	row |= crc;
	
	// polynomial = x^8 + x^5 + x^4 + 1 
	// padded with zeroes corresponding to the bit length of the CRC
	uint32_t divisor = (uint32_t)0x988000;
	
	for (int i = 0 ; i < 16 ; i++) {
		
		// if the input bit above the leftmost divisor bit is 1, 
		// the divisor is XORed into the input
		if (row & (uint32_t)1 << (23 - i)) row ^= divisor;
		
		// the divisor is then shifted one bit to the right
		divisor >>= 1;
	}
	
	// the remainder should equal zero if there are no detectable errors
	return (row == 0);
}

