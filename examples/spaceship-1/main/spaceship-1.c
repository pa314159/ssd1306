#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>
#include <ssd1306-log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_random.h>
#include <esp_timer.h>

static const ssd1306_bitmap_t spaceship_bmp = {
	w: 16, h: 8,

	image: {
		// 'spaceship-2', 16x8px
		0x18, 0x24, 0x44, 0x46, 0x8a, 0x93, 0x95, 0x95, 0x95, 0x95, 0x93, 0x8a, 0x46, 0x44, 0x24, 0x18,

		// room from some experiments
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
};

static inline int signum_of(int x)
{
	return (x > 0) - (x < 0);
}
static inline int random_01()
{
	return esp_random() & 1;
}

static void bouncing_bitmap(ssd1306_t device, bool with_status)
{
	const ssd1306_bounds_t limits = {
		x0: 0,
		y0: ((device->flip || !with_status) ? 0 : 16),
		x1: device->w,
		y1: device->h - (with_status ? 16 - limits.y0 : 0),
	};

	LOG_BOUNDS_I("Universe", &limits);

	ssd1306_bounds_t bounds = limits;
	ssd1306_bounds_resize(&bounds, spaceship_bmp.size);
	ssd1306_bounds_move_by(&bounds, (ssd1306_point_t){
		esp_random() % ssd1306_bounds_width(&limits),
		esp_random() % ssd1306_bounds_height(&limits),
	});

	ssd1306_point_t speed = { 1 - 2*random_01(), 1 - 2*random_01() };

	const uint64_t start = esp_timer_get_time();
	uint64_t frames = 0;

	ssd1306_auto_update(device, false);
	while( true ) {
		ssd1306_draw(device, &bounds, &spaceship_bmp);
		ssd1306_update(device);

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

		if( with_status ) {
			frames++;

			const double time_delta = esp_timer_get_time() - start;
			const double time_average = time_delta / frames / 1E3;
			const double fps_average = 1E6 * frames / time_delta;

			const ssd1306_point_t position = ssd1306_bounds_center(&bounds);

			ssd1306_status(device, ssd1306_status_0,
				"F \x07%6.2f/s\x07 X \x07%3d", fps_average, position.x);
			ssd1306_status(device, ssd1306_status_1,
				"T \x07%6.2fms\x07 Y \x07%3d", time_average, position.y);
		}

		vTaskDelay(pdMS_TO_TICKS(2000));
	}
}

void app_main(void)
{
	ssd1306_t device = ssd1306_init(NULL);

	ssd1306_clear(device, &device->bounds);
	bouncing_bitmap(device, false);

	esp_restart();
}
