#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <ssd1306.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_check.h>

void app_main(void)
{
	TickType_t ticks = xTaskGetTickCount();

	ssd1306_t device = ssd1306_init(ssd1306_create_init());

	char* status0 = "the brown fox jumps over the lazy dog";
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	ssd1306_status(device, ssd1306_status_0, status0);

	char status1[33] = { };
	strcpy(status1, "long live rock");

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	ssd1306_status(device, ssd1306_status_1, status1);

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	ssd1306_clear_b(device, NULL);

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	ssd1306_status(device, ssd1306_status_int, status0);

	memset(status1, 0, sizeof(status1));
	status1[0] = device->text_invert.on;

	for( unsigned k = 0; k < 16; k++ ) {
		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(500));
		status1[k+1] = ' ';
		ssd1306_status(device, ssd1306_status_ext, status1);
	}

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(60000));
	esp_restart();
}
