#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>
#include <os.h>

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

void scroll_bitmap(ssd1306_t device, const ssd1306_bitmap_t* bitmap)
{
	TickType_t ticks = xTaskGetTickCount();

	ssd1306_bitmap_t* moving = ssd1306_create_bitmap(bitmap->width, bitmap->height);

	ssd1306_bounds_t bounds = {
		size: bitmap->size,
	};

	const unsigned posX = ((int)device->width - (int)bitmap->width) / 2;
	const unsigned posY = ((int)device->height - (int)bitmap->height) / 2;

	bounds.x = 0;
	bounds.y = posY;
	ssd1306_clear_b(device, NULL);
	ssd1306_draw_b(device, &bounds, bitmap);
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(2500));

	while( bounds.x <  (int)device->width ) {
		ssd1306_auto_update(device, false);

		ssd1306_grab_b(device, &bounds, moving);
		ssd1306_clear_b(device, &bounds);

		bounds.x++;

		ssd1306_draw_b(device, &bounds, moving);
		ssd1306_status(device, ssd1306_status_ext, "x = %+d, y = %+d", bounds.x, bounds.y);

		ssd1306_auto_update(device, true);

		vTaskDelayUntil(&ticks, SCREEN_SPLASH_TICKS);
	}
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(2500));

	bounds.x = posX;
	bounds.y = 0;
	ssd1306_clear_b(device, NULL);
	ssd1306_draw_b(device, &bounds, bitmap);
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(2500));

	while( bounds.y < (int)device->height ) {
		ssd1306_auto_update(device, false);

		ssd1306_grab_b(device, &bounds, moving);
		ssd1306_clear_b(device, &bounds);

		bounds.y++;

		ssd1306_draw_b(device, &bounds, moving);
		ssd1306_status(device, ssd1306_status_ext, "x = %+d, y = %+d", bounds.x, bounds.y);

		ssd1306_auto_update(device, true);

		vTaskDelayUntil(&ticks, SCREEN_SPLASH_TICKS);
	}
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(2500));

	free(moving);
}

void app_main(void)
{
	ssd1306_init_t init = ssd1306_create_init();
	ssd1306_t device = ssd1306_init(init);

	vTaskDelay(pdMS_TO_TICKS(1000));
	scroll_bitmap(device, &ugly_bitmap);

	vTaskDelay(pdMS_TO_TICKS(1000));
	esp_restart();
}
