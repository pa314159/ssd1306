#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>

#include <ssd1306.h>
#include <ssd1306-misc.h>
#include <ssd1306-int.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

void app_main(void)
{
#if !CONFIG_SSD1306_SPLASH
# error splash has been disabled
#endif
	ssd1306_t device = ssd1306_init(NULL);
	TickType_t ticks = xTaskGetTickCount();

	const char* text0 = "application has been initialised...";
	const char* text1 = "restart in \x7 %2ds \x7";

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	ssd1306_status(device, ssd1306_status_ext, text0);

	ssd1306_bounds_t bounds;
	ssd1306_status_bounds(device, ssd1306_status_ext, &bounds);
	ssd1306_bounds_move_by(&bounds, (ssd1306_point_t){
		0, (1-2*device->flip)*(device->h - ssd1306_bounds_height(&bounds)),
	});

	for( int k = 60; k >= 0; k-- ) {
		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
		ssd1306_text(device, &bounds, text1, k);
	}

	ssd1306_clear(device, NULL);
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(100));
	ssd1306_fill_randomly(device, NULL);
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));

	esp_restart();
}
