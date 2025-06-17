#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_random.h>

void app_main(void)
{
#if !CONFIG_SSD1306_SPLASH
# error splash has been disabled
#endif
	TickType_t ticks = xTaskGetTickCount();
	ssd1306_t device = ssd1306_init(NULL);

	ssd1306_bounds_t bounds;

	ssd1306_center_bounds(device, &bounds, splash_bmp);
	ssd1306_clear(device, &bounds);

	const ssd1306_bounds_t* bounds0 = ssd1306_status_bounds(device, ssd1306_status_0);
	const ssd1306_bounds_t* bounds1 = ssd1306_status_bounds(device, ssd1306_status_1);

	const char* status0 = "initialised...";
	const char* status1 = "restarting in %+d seconds...";

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	ssd1306_text(device, bounds0, status0);

	for( int k = 9; k >= 0; k-- ) {
		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
		ssd1306_text(device, bounds1, status1, k);
	}

	esp_restart();
}
