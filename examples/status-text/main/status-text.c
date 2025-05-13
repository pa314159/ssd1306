#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_check.h>

void app_main(void)
{
	ssd1306_init_t init = ssd1306_create_init();
	ssd1306_t device = ssd1306_init(init);

	ssd1306_clear_b(device, NULL);
	ssd1306_status(device, ssd1306_status_int, "running...");

	char text[18] = {};

	text[0] = init->text_invert.on;

	for( int k = 0; k < 16; k++ ) {
		ssd1306_status(device, ssd1306_status_ext, text);

		vTaskDelay(pdMS_TO_TICKS(1000));

		text[k+1] = ' ';
	}

	ssd1306_status(device, ssd1306_status_int, "restart...");
	vTaskDelay(pdMS_TO_TICKS(2500));

	esp_restart();
}
