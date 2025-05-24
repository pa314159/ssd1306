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
	ssd1306_update(device, NULL);
	ssd1306_release(device);
}

void expand_black_rectangle(ssd1306_t device, TickType_t* ticks)
{
	const unsigned dim = mini(device->width, device->height) / 2 - 1;

	ssd1306_bounds_t bounds = {
		x: dim, y: dim, 
		width: device->width - 2*dim,
		height: device->height - 2*dim,
	};

	while( bounds.x >= 0 && bounds.y >= 0 ) {
		ssd1306_clear_b(device, &bounds);

		vTaskDelayUntil(ticks, SCREEN_SPLASH_TICKS);

		bounds.x--;
		bounds.y--;
		bounds.width += 2;
		bounds.height += 2;
	}
}

void shrink_black_rectangle(ssd1306_t device, TickType_t* ticks)
{
	const unsigned dim = mini(device->width, device->height) / 2 - 1;

	ssd1306_bounds_t bounds = device->bounds;

	ssd1306_bitmap_t* temp = ssd1306_create_bitmap(device->width, device->height);

	ssd1306_grab_b(device, NULL, temp);

	while( bounds.x <= dim && bounds.y <= dim ) {
		ssd1306_auto_update(device, false);
		ssd1306_draw_b(device, NULL, temp);
		ssd1306_clear_b(device, &bounds);
		ssd1306_auto_update(device, true);

		vTaskDelayUntil(ticks, SCREEN_SPLASH_TICKS);

		bounds.x++;
		bounds.y++;
		bounds.width -= 2;
		bounds.height -= 2;
	}
}

void app_main(void)
{
	TickType_t ticks = xTaskGetTickCount();

	ssd1306_init_t init = ssd1306_create_init();
	ssd1306_t device = ssd1306_init(init);

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	fill_with_random(device);
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	expand_black_rectangle(device, &ticks);

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	fill_with_random(device);
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	shrink_black_rectangle(device, &ticks);

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	for( int16_t ct = 255; ct > 0; ct -= 4 ) {
		ssd1306_contrast(device, ct);
		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(100));
	}

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(1000));
	ssd1306_contrast(device, CONFIG_SSD1306_CONTRAST);

	for( int16_t k = 0; k < 16; k++ ) {
		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(250));
		ssd1306_on(device, k & 1);
	}

	for( int16_t k = 0; k < 16; k++ ) {
		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(250));
		ssd1306_invert(device, !(k & 1));
	}

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(30000));
	esp_restart();
}
