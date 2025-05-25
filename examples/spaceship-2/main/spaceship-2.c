#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_random.h>
#include <esp_timer.h>

#define SPACESHIP_W 16
#define SPACESHIP_H 11

const ssd1306_bitmap_t spaceship1_bmp = {
	width: SPACESHIP_W,
	height: SPACESHIP_H,
	image: {
		// 'spaceship-1', 16x12px
		0x18, 0x24, 0x44, 0x46, 0x8a, 0x92, 0x95, 0x95, 0x95, 0x95, 0x92, 0x8a, 0x46, 0x44, 0x24, 0x18, 
		0x00, 0x00, 0x04, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x00, 0x00, 
	},
};
	
const ssd1306_bitmap_t spaceship2_bmp = {
	width: SPACESHIP_W,
	height: SPACESHIP_H,
	image: {
		// 'spaceship-2', 16x12px
		0x18, 0x24, 0x44, 0x46, 0x8a, 0x93, 0x95, 0x95, 0x95, 0x95, 0x93, 0x8a, 0x46, 0x44, 0x24, 0x18, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
};

inline int signum_of(int x)
{
	return (x > 0) - (x < 0);
}
inline int random_01()
{
	return esp_random() & 1;
}

static void bouncing_bitmap(ssd1306_t device, bool with_status)
{
	const int16_t x_min = 0;
	const int16_t x_max = device->width;
	const int16_t y_min = device->flip || !with_status ? 0 : 16;
	const int16_t y_max = device->height - (with_status ? 16 - y_min : 0);

	ssd1306_bounds_t bounds = {
		x1: SPACESHIP_W,
		y1: SPACESHIP_H,
	};

	ssd1306_bounds_move_to(&bounds, x_min + esp_random() % (x_max - x_min), y_min + esp_random() % (y_max - y_min));

	int16_t speed_x = 1 - 2*random_01();
	int16_t speed_y = 1 - 2*random_01();

	const uint64_t start = esp_timer_get_time();
	uint64_t loops = 0;

	ssd1306_auto_update(device, false);

	while( true ) {
		const ssd1306_bitmap_t* bm = speed_y < 0 ? &spaceship1_bmp : &spaceship2_bmp;

		ssd1306_draw_b(device, &bounds, bm);
		ssd1306_auto_update(device, true);
		ssd1306_auto_update(device, false);
		ssd1306_clear_b(device, &bounds);

		ssd1306_bounds_move_by(&bounds, speed_x, speed_y);

		if( bounds.x0 < x_min || bounds.x1 > x_max ) {
			speed_x = -signum_of(speed_x) * (1 + random_01());

			ssd1306_bounds_move_by(&bounds, speed_x, 0);
		}
		if( bounds.y0 < y_min || bounds.y1 > y_max ) {
			speed_y = -signum_of(speed_y) * (1 + random_01());

			ssd1306_bounds_move_by(&bounds, 0, speed_y);
		}

		const uint64_t time_local = esp_timer_get_time();
		const double time_total = (double)(time_local - start) / 1E6;

		if( time_total >= 1000 ) {
			break;
		}

		const double time_average = (double)(time_local - start) / (++loops) / 1E3;

		if( with_status ) {
			ssd1306_status(device, ssd1306_status_0, "T \x07%7.3fs\x07 X \x07%3d", time_total, (bounds.x0 + bounds.x1) / 2);
			ssd1306_status(device, ssd1306_status_1, "t \x07%6.2fms\x07 Y \x07%3d", time_average, (bounds.y0 + bounds.y1) / 2);
		}
	}
}

void app_main(void)
{
	ssd1306_t device = ssd1306_init(NULL);

	ssd1306_clear_b(device, NULL);
	bouncing_bitmap(device, device->height == 64);

	esp_restart();
}
