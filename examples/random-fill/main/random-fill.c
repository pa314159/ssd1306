#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_random.h>

#define SCREEN_SPLASH_FPS   30
#define SCREEN_SPLASH_TICKS pdMS_TO_TICKS(1000/SCREEN_SPLASH_FPS)

inline int mini(int a, int b)
{
	return a < b ? a : b;
}

void fill_with_random(ssd1306_t device)
{
	ssd1306_acquire(device);
	for( unsigned page = 0; page < device->pages; page++ ) {
		esp_fill_random(ssd1306_raster(device, page), device->width);
	}
	ssd1306_update(device);
	ssd1306_release(device);
}


void expand_black_rectangle(ssd1306_t device)
{
	const unsigned dim = mini(device->width, device->height) / 2 - 1;

	ssd1306_bounds_t bounds = {
		x: dim, y: dim, 
		width: device->width - 2*dim,
		height: device->height - 2*dim,
	};

	TickType_t ticks = xTaskGetTickCount();

	while( bounds.x >= 0 && bounds.y >= 0 ) {
		ssd1306_clear_b(device, &bounds);

		vTaskDelayUntil(&ticks, SCREEN_SPLASH_TICKS);

		bounds.x--;
		bounds.y--;
		bounds.width += 2;
		bounds.height += 2;
	}
}

void app_main(void)
{
	ssd1306_init_t init = ssd1306_create_init();
	ssd1306_t device = ssd1306_init(init);

	vTaskDelay(pdMS_TO_TICKS(1000));
	fill_with_random(device);

	vTaskDelay(pdMS_TO_TICKS(1000));
	expand_black_rectangle(device);

	vTaskDelay(pdMS_TO_TICKS(1000));
	esp_restart();
}
