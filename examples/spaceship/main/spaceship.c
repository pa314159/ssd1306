#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_check.h>
#include <esp_random.h>
#include <esp_timer.h>

#define SCREEN_SPLASH_FPS   30
#define SCREEN_SPLASH_TICKS pdMS_TO_TICKS(1000/SCREEN_SPLASH_FPS)

const ssd1306_bitmap_t spaceship1_bmp = {
	width: 16,
	height: 16,
	image: {
		0xc0, 0x20, 0x20, 0x30, 0x50, 0x90, 0xa8, 0xa8,
		0xa8, 0xa8, 0x90, 0x50, 0x30, 0x20, 0x20, 0xc0,

		0x00, 0x01, 0x22, 0x12, 0x0c, 0x04, 0x14, 0x0c,
		0x04, 0x04, 0x04, 0x0c, 0x12, 0x22, 0x01, 0x00,
	},
};

const ssd1306_bitmap_t spaceship2_bmp = {
	width: 16,
	height: 8,
	image: {
		0x18, 0x24, 0x44, 0x46, 0x8a, 0x93, 0x95, 0x95,
		0x95, 0x95, 0x93, 0x8a, 0x46, 0x44, 0x24, 0x18
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

static void bouncing_bitmap(ssd1306_t device, const ssd1306_bitmap_t* bitmap)
{
	TickType_t ticks = xTaskGetTickCount();

	const int x_min = 0;
	const int x_max = device->width - bitmap->width;
	const int y_min = device->flip || device->height == 32 ? 0 : 16;
	const int y_max = device->height - bitmap->height - (device->height == 32 ? 0 :16 - y_min);

	ssd1306_bounds_t bounds = {
		x: x_min + esp_random() % (x_max - x_min),
		y: y_min + esp_random() % (y_max - y_min),
		size: bitmap->size,
	};

	int speed_x = 1 - 2*random_01();
	int speed_y = 1 - 2*random_01();

	const uint64_t start_total = esp_timer_get_time();
	uint64_t start_local = esp_timer_get_time();

	ssd1306_auto_update(device, false);

	while( true ) {
		uint64_t start = esp_timer_get_time();

		if( device->height == 64 ) {
			double diff_total = (start - start_total) % 1000000000ULL / 1E6;
			double diff_local = (start - start_local) / 1E3;

			ssd1306_status(device, ssd1306_status_0, "T \x0f%6.1fms\x0e X \x0f%3d", diff_total, bounds.x);
			ssd1306_status(device, ssd1306_status_1, "t \x0f%6.1fus\x0e Y \x0f%3d", diff_local, bounds.y);
		}

		ssd1306_draw_b(device, &bounds, bitmap);
		ssd1306_auto_update(device, true);
		vTaskDelayUntil(&ticks, SCREEN_SPLASH_TICKS);
		ssd1306_auto_update(device, false);
		ssd1306_clear_b(device, &bounds);

		bounds.x += speed_x;
		bounds.y += speed_y;

		if( bounds.x < x_min || bounds.x >= x_max ) {
			speed_x = -signum_of(speed_x) * (1 + random_01());
			bounds.x += speed_x;
		}
		if( bounds.y < y_min || bounds.y >= y_max ) {
			speed_y = -signum_of(speed_y) * (1 + random_01());
			bounds.y += speed_y;
		}

		start_local = start;
	}
}

void app_main(void)
{
	ssd1306_init_t init = ssd1306_create_init();
	ssd1306_t device = ssd1306_init(init);

	vTaskDelay(pdMS_TO_TICKS(2000));
	ssd1306_clear_b(device, NULL);

	// bouncing_bitmap(device, &spaceship1_bmp);
	bouncing_bitmap(device, &spaceship2_bmp);
}
