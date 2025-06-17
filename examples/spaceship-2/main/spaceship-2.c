#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>

#define SCREEN_IMAGE_FPS   25U
#define SCREEN_IMAGE_TICKS pdMS_TO_TICKS(1000U / SCREEN_IMAGE_FPS)

static const ssd1306_bitmap_t spaceship_asc = {
	w: 16, h: 11,

	image: {
		// 'spaceship-asc', 16x11px
		0x18, 0x24, 0x44, 0x46, 0x8a, 0x92, 0x95, 0x95, 0x95, 0x95, 0x92, 0x8a, 0x46, 0x44, 0x24, 0x18, 
		0x00, 0x00, 0x04, 0x02, 0x01, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x00, 0x00, 
	},
};

static const ssd1306_bitmap_t spaceship_dsc = {
	w: 16, h: 8,

	image: {
		// 'spaceship-dsc', 16x8px
		0x18, 0x24, 0x44, 0x46, 0x8a, 0x93, 0x95, 0x95, 0x95, 0x95, 0x93, 0x8a, 0x46, 0x44, 0x24, 0x18, 
		// room from some experiments
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

static void bouncing_bitmap(ssd1306_t device)
{
	const ssd1306_bounds_t limits = {
		x0: 8,
		y0: 5,
		x1: device->w - 8,
		y1: device->h - 6,
	};

	ESP_LOGI("MAIN", "Universe [+%d+%d, %+d%+d]", limits.x0, limits.y0, limits.x1, limits.y1);

	ssd1306_bounds_t bounds = limits;
	ssd1306_bounds_resize(&bounds, spaceship_asc.size);
	ssd1306_bounds_move_by(&bounds, (ssd1306_point_t){
		esp_random() % ssd1306_bounds_width(&limits),
		esp_random() % ssd1306_bounds_height(&limits),
	});

	ssd1306_point_t speed = { 1 - 2*random_01(), 1 - 2*random_01() };
	TickType_t ticks = xTaskGetTickCount();

	ssd1306_auto_update(device, false);

	while( true ) {
		const ssd1306_bitmap_t* bm = speed.y < 0 ? &spaceship_asc : &spaceship_dsc;

		ssd1306_draw(device, &bounds, bm, NULL);
		ssd1306_auto_update(device, true);
		ssd1306_auto_update(device, false);
		ssd1306_clear(device, &bounds);

		ssd1306_bounds_move_by(&bounds, speed);

		if( (speed.x < 0 && bounds.x0 < limits.x0) || (speed.x > 0 && bounds.x1 > limits.x1) ) {
			ssd1306_bounds_move_by(&bounds, (ssd1306_point_t){ -speed.x, 0 });

			speed.x = -signum_of(speed.x) * (1 + random_01());
		}
		if( (speed.y < 0 && bounds.y0 < limits.y0) || (speed.y > 0 && bounds.y1 > limits.y1) ) {
			ssd1306_bounds_move_by(&bounds, (ssd1306_point_t){ 0, -speed.y });

			speed.y = -signum_of(speed.y) * (1 + random_01());
		}

		vTaskDelayUntil(&ticks, SCREEN_IMAGE_TICKS);
	}
}

void app_main(void)
{
	ssd1306_t device = ssd1306_init(NULL);

	ssd1306_clear(device, &device->bounds);
	bouncing_bitmap(device);

	esp_restart();
}
