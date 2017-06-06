#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_partition.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"

// max buffer length
#define LINE_MAX	50

// NVS handler
nvs_handle my_handle;


// parse command
void parse_command(char* command) {
	
	// split the command and the arguments
	char* token;
	token = strtok(command, " ");
	
	if(!token) {
		
		printf("\nNo command provided!\n");
		return;
	}
	
	// ERASE command, erase all the partition content
	if(strcmp(token, "erase") == 0) {
	
		printf("\nErasing all the VFS partition...");
		fflush(stdout);
		
		esp_err_t err = nvs_erase_all(my_handle);
		if(err != ESP_OK) {
			printf(" error! (%04X)\n", err);
			return;
		}
		err = nvs_commit(my_handle);	
		if(err != ESP_OK) {
			printf(" error in commit! (%04X)\n", err);
			return;
		}
		printf(" done!\n");
	}
	
	// GETINT command, return an int value based on the key
	else if(strcmp(token, "getint") == 0) {
	
		char* parameter = strtok(NULL, " ");
		if(!parameter) {
			
			printf("\nNo key provided!\n");
			return;
		}
		int32_t value = 0;
        esp_err_t err = nvs_get_i32(my_handle, parameter, &value);
		if(err != ESP_OK) {
			if(err == ESP_ERR_NVS_NOT_FOUND) printf("\nKey %s not found\n", parameter);
			else printf("\nError in nvs_get_i32! (%04X)\n", err);
			return;
		}
		printf("\nValue stored in NVS for key %s is %d\n", parameter, value);
	}
	
	// GETSTRING command, return a string based on the key
	else if(strcmp(token, "getstring") == 0) {
	
		char* parameter = strtok(NULL, " ");
		if(!parameter) {
			
			printf("\nNo key provided!\n");
			return;
		}
		size_t string_size;
		esp_err_t err = nvs_get_str(my_handle, parameter, NULL, &string_size);
		if(err != ESP_OK) {
			printf("\nError in nvs_get_str to get string size! (%04X)\n", err);
			return;
		}
		char* value = malloc(string_size);
		err = nvs_get_str(my_handle, parameter, value, &string_size);
		if(err != ESP_OK) {
			if(err == ESP_ERR_NVS_NOT_FOUND) printf("\nKey %s not found\n", parameter);
			printf("\nError in nvs_get_str to get string! (%04X)\n", err);
			return;
		}
		printf("\nValue stored in NVS for key %s is %s\n", parameter, value);
	}
	
	// SETINT command, store an int value with the given key
	else if(strcmp(token, "setint") == 0) {
	
		char* parameter1 = strtok(NULL, " ");
		char* parameter2 = strtok(NULL, " ");
		if(!parameter1 || !parameter2) {
			
			printf("\nNo key or value provided!\n");
			return;
		}
		int32_t value = atoi(parameter2);
        esp_err_t err = nvs_set_i32(my_handle, parameter1, value);
		if(err != ESP_OK) {
			printf("\nError in nvs_set_i32! (%04X)\n", err);
			return;
		}
		err = nvs_commit(my_handle);
		if(err != ESP_OK) {
			printf("\nError in commit! (%04X)\n", err);
			return;
		}
		printf("\nValue %d stored in NVS with key %s\n", value, parameter1);
	}
	
	// SETSTRING command, store a string value with the given key
	else if(strcmp(token, "setstring") == 0) {
	
		char* parameter1 = strtok(NULL, " ");
		char* parameter2 = strtok(NULL, " ");
		if(!parameter1 || !parameter2) {
			
			printf("\nNo key or value provided!\n");
			return;
		}
        esp_err_t err = nvs_set_str(my_handle, parameter1, parameter2);
		if(err != ESP_OK) {
			printf("\nError in nvs_set_str! (%04X)\n", err);
			return;
		}
		err = nvs_commit(my_handle);
		if(err != ESP_OK) {
			printf("\nError in commit! (%04X)\n", err);
			return;
		}
		printf("\nValue %s stored in NVS with key %s\n", parameter2, parameter1);
	}
	
	// UNKNOWN command
	else printf("\nUnknown command!\n");
}


// main task
void main_task(void *pvParameter) {

	// buffer to store the command	
	char line[LINE_MAX];
	int line_pos = 0;
	
	// print the command prompt
	printf("esp32> ");
	fflush(stdout);
	
	// read the command from stdin
	while(1) {
	
		int c = getchar();
		
		// nothing to read, give to RTOS the control
		if(c < 0) {
			vTaskDelay(10 / portTICK_PERIOD_MS);
			continue;
		}
		if(c == '\r') continue;
		if(c == '\n') {
		
			// terminate the string and parse the command
			line[line_pos] = '\0';
			parse_command(line);
			
			// reset the buffer and print the prompt
			line_pos = 0;
			printf("\nesp32> ");
			fflush(stdout);
		}
		else {
			putchar(c);
			line[line_pos] = c;
			line_pos++;
			
			// buffer full!
			if(line_pos == LINE_MAX) {
				
				printf("\nCommand buffer full!\n");
				
				// reset the buffer and print the prompt
				line_pos = 0;
				printf("\nesp32> ");
				fflush(stdout);
			}
		}
	}
}


// Main application
void app_main() {

	printf("NVS Demo, esp32-tutorial\n\n");
	
	// initialize NVS flash
	esp_err_t err = nvs_flash_init();
	
	// if it is invalid, try to erase it
    if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
		
		printf("Got NO_FREE_PAGES error, trying to erase the partition...\n");
		
		// find the NVS partition
        const esp_partition_t* nvs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);      
		if(!nvs_partition) {
			
			printf("FATAL ERROR: No NVS partition found\n");
			while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
		}
		
		// erase the partition
        err = (esp_partition_erase_range(nvs_partition, 0, nvs_partition->size));
		if(err != ESP_OK) {
			printf("FATAL ERROR: Unable to erase the partition\n");
			while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
		}
		printf("Partition erased!\n");
		
		// now try to initialize it again
		err = nvs_flash_init();
		if(err != ESP_OK) {
			
			printf("FATAL ERROR: Unable to initialize NVS\n");
			while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
		}
	}
	printf("NVS init OK!\n");
	
	// open the partition in RW mode
	err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
		
		printf("FATAL ERROR: Unable to open NVS\n");
		while(1) vTaskDelay(10 / portTICK_PERIOD_MS);
	}
	printf("NVS open OK\n");
	
	// start the main task
	xTaskCreate(&main_task, "main_task", 2048, NULL, 5, NULL);
}
