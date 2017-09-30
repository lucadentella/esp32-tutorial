// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// VFS and SPIFFS includes
#include "esp_vfs.h"
#include "spiffs_vfs.h"

// error library include
#include "esp_log.h"
#include <string.h>

// max buffer length
#define LINE_MAX	50

// global variables
char actual_path[256];


void ls(char* path) {

	printf("\r\nListing folder %s\r\n", path);
	
	// open the specified folder
	DIR *dir;
	dir = opendir(path);
    if (!dir) {
        printf("Error opening folder\r\n");
        return;
    }
	
	// list the files and folders
	struct dirent *direntry;
	while ((direntry = readdir(dir)) != NULL) {
		
		// do not print the root folder (/spiffs)
		if(strcmp(direntry->d_name, "/spiffs") == 0) continue;
		
		if(direntry->d_type == DT_DIR) printf("DIR\t");
		else if(direntry->d_type == DT_REG) printf("FILE\t");
		else printf("???\t");
		printf("%s\r\n", direntry->d_name);
	}
	
	// close the folder
	closedir(dir);
}

void cat(char* filename) {

	printf("\r\nContent of the file %s\r\n", filename);
	
	// open the specified file
	char file_path[300];
	strcpy(file_path, actual_path);
	strcat(file_path, "/");
	strcat(file_path, filename);

	FILE *file;
	file = fopen(file_path, "r");
    if (!file) {
        printf("Error opening file %s\r\n", file_path);
        return;
    }
	
	
	// display the file content
	int filechar;
	while((filechar = fgetc(file)) != EOF)
		putchar(filechar);
	
	
	// close the folder
	fclose(file);
}

void cd(char* path) {

	printf("\r\nMoving to directory %s\r\n", path);
	
	// backup the actual path
	char previous_path[256];
	strcpy(previous_path, actual_path);
	
	// if the new path is ".." return to the previous
	if(strcmp(path, "..") == 0) {
		
		// locate the position of the last /
		char* pos = strrchr(actual_path, '/');
		if(pos != actual_path) pos[0] = '\0';
	}
	
	// if the new path starts with /, append to the root folder
	else if(path[0] == '/') {
	
		strcpy(actual_path, "/spiffs");
		if(strlen(path) > 1) strcat(actual_path, path);
	}
	
	// else add it to the actual path
	else {
		strcat(actual_path, "/");
		strcat(actual_path, path);
	}
	
	// verify that the new path exists
	DIR *dir;
	dir = opendir(actual_path);
    
	// if not, rever to the previous path
	if (!dir) {
        printf("Folder does not exists\r\n");
        strcpy(actual_path, previous_path);
		return;
    }
	
	closedir(dir);
}

// parse command
void parse_command(char* command) {
	
	// split the command and the arguments
	char* token;
	token = strtok(command, " ");
	
	if(!token) {
		
		printf("\r\nNo command provided!\r\n");
		return;
	}
	
	// LS command, list the content of the actual folder
	if(strcmp(token, "ls") == 0) {
	
		ls(actual_path);
	}
	
	// CAT command, display the content of a file
	else if(strcmp(token, "cat") == 0) {
	
		char* filename = strtok(NULL, " ");
		if(!filename) {
			
			printf("\r\nNo file specified!\r\n");
			return;
		}
		cat(filename);
	}
	
	// CD command, move to the specified directory
	else if(strcmp(token, "cd") == 0) {
	
		char* path = strtok(NULL, " ");
		if(!path) {
			
			printf("\r\nNo directory specified!\r\n");
			return;
		}
		cd(path);
	}
	
	// UNKNOWN command
	else printf("\r\nUnknown command!\r\n");
}


// print the command prompt, including the actual path
void print_prompt() {
	
	printf("\r\nesp32 (");
	printf(actual_path);
	printf(") > ");
	fflush(stdout);
}


// main task
void main_task(void *pvParameter) {

	// buffer to store the command	
	char line[LINE_MAX];
	int line_pos = 0;

	print_prompt();
	
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
			print_prompt();
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
				print_prompt();
			}
		}
	}	
}


// Main application
void app_main() {

	printf("SPIFFS example\r\n\r\n");
	
	// register SPIFFS with VFS
	vfs_spiffs_register();
	
	// the partition was mounted?
	if(spiffs_is_mounted) {
		
		printf("Partition correctly mounted!\r\n");
	} 
	
	else {
		printf("Error while mounting the SPIFFS partition");
		while(1) vTaskDelay(1000 / portTICK_RATE_MS);
	}
	
	// initial path
	strcpy(actual_path, "/spiffs");
	
	// start the loop task
	xTaskCreate(&main_task, "main_task", 8192, NULL, 5, NULL);
}
