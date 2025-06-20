#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>
#include <ssd1306-misc.h>
#include "os.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_check.h>

#define SCREEN_IMAGE_FPS   10
#define SCREEN_IMAGE_TICKS pdMS_TO_TICKS(1000/SCREEN_IMAGE_FPS)

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

void translate_bitmap(ssd1306_t device, const ssd1306_bitmap_t* bitmap)
{
	TickType_t ticks = xTaskGetTickCount();

	ssd1306_bounds_t bounds;

	ssd1306_center_bounds(device, &bounds, bitmap);

	const int16_t posX = bounds.x0;
	const int16_t posY = bounds.y0;

	const int16_t xmin = -(int)bitmap->w - 1;
	const int16_t xmax = device->w + bitmap->w + 1;
	const int16_t ymin = -(int)bitmap->h - 1;
	const int16_t ymax = device->h + bitmap->h + 1;

	ssd1306_clear(device, &device->bounds);
	ssd1306_bounds_move_to(&bounds, (ssd1306_point_t){ xmin, posY });
	ssd1306_auto_update(device, false);
	while( bounds.x0 >= xmin && bounds.x1 <= xmax ) {
		ssd1306_draw(device, &bounds, bitmap);
		ssd1306_auto_update(device, true);
		vTaskDelayUntil(&ticks, SCREEN_IMAGE_TICKS);
		ssd1306_auto_update(device, false);
		ssd1306_clear(device, &bounds);

		ssd1306_bounds_move_by(&bounds, (ssd1306_point_t){ 1, 0 });
	}
	ssd1306_auto_update(device, true);

	ssd1306_clear(device, &device->bounds);
	ssd1306_bounds_move_to(&bounds, (ssd1306_point_t){ posX, ymin });
	ssd1306_auto_update(device, false);
	while( bounds.y0 >= ymin && bounds.y1 <= ymax ) {
		ssd1306_draw(device, &bounds, bitmap);
		ssd1306_auto_update(device, true);
		vTaskDelayUntil(&ticks, SCREEN_IMAGE_TICKS);
		ssd1306_auto_update(device, false);
		ssd1306_clear(device, &bounds);

		ssd1306_bounds_move_by(&bounds, (ssd1306_point_t){ 0, 1 });
	}
	ssd1306_auto_update(device, true);

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(2500));
	ssd1306_clear(device, &device->bounds);
	ssd1306_draw_c(device, bitmap);
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(2500));
}

void app_main(void)
{
	ssd1306_t device = ssd1306_init(NULL);

	uint16_t count = 0;

	while( true ) {
		ssd1306_bounds_t bounds = {
			x0: 64, 
			y0: device->h - ugly_bitmap.h + count % ugly_bitmap.h,
			x1: 64 + ugly_bitmap.w,
			y1: device->h
		};

		ssd1306_auto_update(device, false);

		ssd1306_clear(device, &device->bounds);

		while( bounds.y1 > bounds.y0 ) {
			uint16_t pos = 0;
			ssd1306_text(device, &(ssd1306_bounds_t){ x0: 0, y0: 8*pos, x1: 128, y1: 8*(pos+1) }, 
					"P0: %+3d%+3d", bounds.x0, bounds.y0);
			pos++;
			ssd1306_text(device, &(ssd1306_bounds_t){ x0: 0, y0: 8*pos, x1: 128, y1: 8*(pos+1) },
					"P1: %+3d%+3d", bounds.x1, bounds.y1);
			pos++;
			ssd1306_text(device, &(ssd1306_bounds_t){ x0: 0, y0: 8*pos, x1: 128, y1: 8*(pos+1) },
					"SZ:  %2ux%2u",  ssd1306_bounds_width(&bounds), ssd1306_bounds_height(&bounds));

			ssd1306_draw(device, &bounds, &ugly_bitmap);
			ssd1306_auto_update(device, true);
			vTaskDelay(pdMS_TO_TICKS(100));
			ssd1306_auto_update(device, false);
			ssd1306_clear(device, &bounds);

			bounds.y1--;
		}

		ssd1306_auto_update(device, true);

		count++;
	}
	// vTaskDelay(pdMS_TO_TICKS(2000));
	// translate_bitmap(device, splash_bmp);
	// vTaskDelay(pdMS_TO_TICKS(2000));
	// translate_bitmap(device, &ugly_bitmap);

	// vTaskDelay(pdMS_TO_TICKS(2000));
	// esp_restart();

	// ssd1306_fill_randomly(device, &device->bounds);

	// vTaskDelay(pdMS_TO_TICKS(1500));

	// for( int k = 1; k < 3 ; k++ ) {
	// 	ssd1306_auto_update(device, false);

	// 	ssd1306_bounds_t bounds = device->bounds;

	// 	bounds.x0 += 1;
	// 	bounds.y0 += 1;
	// 	bounds.x1 -= 1;
	// 	bounds.y1 -= 1;

	// 	ssd1306_clear(device, &bounds);

	// 	bounds.x0 += 1;
	// 	bounds.x1 -= 1;
	// 	bounds.y1 -= 1;
	// 	bounds.y0 = bounds.y1 - ugly_bitmap.h + k;

	// 	// ssd1306_fill_randomly(device, &bounds);

	// 	bounds.x0 = 64;

	// 	char text[16];

	// 	snprintf(text, 16, "%d: %d %d %d", k, bounds.y0 % 64, bounds.y1 % 64, ssd1306_bounds_height(&bounds) % 64);

	// 	ssd1306_text(device, 4, 32, 124, 8, text);

	// 	ssd1306_log_set_level(ESP_LOG_VERBOSE);
	// 	ssd1306_draw(device, &bounds, &ugly_bitmap);
	// 	ssd1306_auto_update(device, true);
	// 	ssd1306_log_set_level(CONFIG_SSD1306_LOGGING_LEVEL);

	// 	vTaskDelay(pdMS_TO_TICKS(500));
	// }
}
