#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_check.h>

#define SCREEN_SPLASH_FPS   30
#define SCREEN_SPLASH_TICKS pdMS_TO_TICKS(1000/SCREEN_SPLASH_FPS)

const ssd1306_bitmap_t ugly_bitmap = {
	width: 16,
	height: 24,

	image: {
		0x00, 0x00, 0x00, 0x06, 0x03, 0xfe, 0xfc, 0xf8,
		0xf0, 0xe0, 0xc0, 0x81, 0x02, 0x00, 0x00, 0x00,
		
		0x00, 0x00, 0x00, 0x18, 0x99, 0x00, 0x00, 0x00,
		0xc0, 0x30, 0x0c, 0xf3, 0x01, 0x00, 0x00, 0x00,
		
		0x00, 0x00, 0x00, 0x60, 0xc0, 0x30, 0x0c, 0x03,
		0x00, 0xc0, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
	}
};

void translate_bitmap(ssd1306_t device, const ssd1306_bitmap_t* bitmap)
{
	TickType_t ticks = xTaskGetTickCount();

	const unsigned posX = abs((int)device->width - (int)bitmap->width) / 2;
	const unsigned posY = abs((int)device->height - (int)bitmap->height) / 2;

	ssd1306_bounds_t bounds = {
		x: posX,
		y: posY,
		width: bitmap->width,
		height: bitmap->height,
	};

	const int x0 = -(int)bitmap->width - 1;
	const int x1 = device->width + + 1;

	ssd1306_clear_b(device, NULL);
	ssd1306_auto_update(device, false);
	for( bounds.x = x1, bounds.y = posY; bounds.x >= x0; bounds.x-- ) {
		ssd1306_draw_b(device, &bounds, bitmap);
		ssd1306_auto_update(device, true);
		vTaskDelayUntil(&ticks, SCREEN_SPLASH_TICKS);
		ssd1306_auto_update(device, false);
		ssd1306_clear_b(device, &bounds);
		ssd1306_clear(device, 0, device->height - 8, device->width, 8);
	}
	ssd1306_auto_update(device, true);

	const int y0 = -(int)bitmap->height - 1;
	const int y1 = device->height + 1;

	ssd1306_clear_b(device, NULL);
	ssd1306_auto_update(device, false);
	for( bounds.x = posX, bounds.y = y0; bounds.y <= y1; bounds.y++ ) {
		ssd1306_draw_b(device, &bounds, bitmap);
		ssd1306_auto_update(device, true);
		vTaskDelayUntil(&ticks, SCREEN_SPLASH_TICKS);
		ssd1306_auto_update(device, false);
		ssd1306_clear_b(device, &bounds);
	}
	ssd1306_auto_update(device, true);

	ssd1306_clear_b(device, NULL);
	ssd1306_draw(device, posX, posY, device->width, device->height, bitmap);
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(2500));
}

void app_main(void)
{
	ssd1306_t device = ssd1306_init(ssd1306_create_init());

	vTaskDelay(pdMS_TO_TICKS(2000));
	translate_bitmap(device, splash_bmp);
	vTaskDelay(pdMS_TO_TICKS(2000));
	translate_bitmap(device, &ugly_bitmap);

	vTaskDelay(pdMS_TO_TICKS(2000));
	esp_restart();
}
