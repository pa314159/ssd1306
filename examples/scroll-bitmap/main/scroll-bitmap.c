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
	w: 16, h: 24,

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

	ssd1306_bitmap_t* moving = ssd1306_create_bitmap((ssd1306_size_t){w: bitmap->w, h: bitmap->h});
	ssd1306_bounds_t bounds;

	ssd1306_center_bounds(device, &bounds, bitmap);

	const int16_t posX = bounds.x0;
	const int16_t posY = bounds.y0;

	ssd1306_clear(device, &device->bounds);
	ssd1306_bounds_move_to(&bounds, (ssd1306_point_t){ 0, posY });
	ssd1306_draw(device, &bounds, bitmap);
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(2500));

	ssd1306_auto_update(device, false);
	while( bounds.x0 <  (int)device->w ) {
		ssd1306_grab(device, &bounds, moving);
		ssd1306_clear(device, &bounds);

		ssd1306_bounds_move_by(&bounds, (ssd1306_point_t){ 1, 0 });

		ssd1306_draw(device, &bounds, moving);
		ssd1306_status(device, ssd1306_status_ext, "x: %+4d, y: %+3d", bounds.x0, bounds.y0);

		ssd1306_update(device);
		vTaskDelayUntil(&ticks, SCREEN_SPLASH_TICKS);
	}
	ssd1306_auto_update(device, true);

	ssd1306_clear(device, &device->bounds);
	ssd1306_bounds_move_to(&bounds, (ssd1306_point_t){ posX, 0 });
	ssd1306_draw(device, &bounds, bitmap);
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(2500));

	ssd1306_auto_update(device, false);
	while( bounds.y0 < (int)device->h ) {
		ssd1306_grab(device, &bounds, moving);
		ssd1306_clear(device, &bounds);

		ssd1306_bounds_move_by(&bounds, (ssd1306_point_t){ 0, 1 });

		ssd1306_draw(device, &bounds, moving);
		ssd1306_status(device, ssd1306_status_ext, "x: %+4d, y: %+3d", bounds.x0, bounds.y0);

		ssd1306_update(device);
		vTaskDelayUntil(&ticks, SCREEN_SPLASH_TICKS);
	}
	ssd1306_auto_update(device, true);

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(2500));

	free(moving);
}

void app_main(void)
{
	ssd1306_t device = ssd1306_init(NULL);

	vTaskDelay(pdMS_TO_TICKS(1000));
	scroll_bitmap(device, &ugly_bitmap);

	vTaskDelay(pdMS_TO_TICKS(1000));
	scroll_bitmap(device, splash_bmp);

	vTaskDelay(pdMS_TO_TICKS(1000));
	esp_restart();
}
