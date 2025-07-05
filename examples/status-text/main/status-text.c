#include "sdkconfig.h"

#include <stdio.h>
#include <string.h>
#include <ssd1306.h>
#include <ssd1306-log.h>
#include <ssd1306-misc.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_random.h>

#define DEMO_TIMEOUT      15000
#define PAUSE_MILLIS      1500
#define FRAME_MILLIS      100

void fill_with_random(ssd1306_t device)
{
	ssd1306_acquire(device);

	for( uint16_t page = 0; page < device->pages-2; page++ ) {
		esp_fill_random(ssd1306_raster(device, page), device->w);
	}

	ssd1306_update(device);
	ssd1306_release(device);
}

void app_main(void)
{
	TickType_t ticks = xTaskGetTickCount();
	ssd1306_t device = ssd1306_init(NULL);

	ssd1306_bounds_t bnd0 = *ssd1306_status_bounds(device, ssd1306_status_0, NULL);
	ssd1306_bounds_t bnd1 = *ssd1306_status_bounds(device, ssd1306_status_1, NULL);

	LOG_BOUNDS_I("bounds0", &bnd0);
	LOG_BOUNDS_I("bounds1", &bnd1);

	ssd1306_bounds_union(&bnd1, &bnd0);

	LOG_BOUNDS_I("union", &bnd1);

	for( uint16_t status = 0; status < 4; status++ ) {
		ssd1306_status(device, status, "status %u scrolling text", status);
		vTaskDelayUntil(&ticks, 10*pdMS_TO_TICKS(PAUSE_MILLIS));
		ssd1306_status(device, status, NULL);
	}

	char status0[] = "the brown fox jumps over the lazy dog";
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
	ssd1306_status(device, ssd1306_status_0, status0);

	char status1[17] = { };
	strcpy(status1, "long live rock");

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
	ssd1306_status(device, ssd1306_status_1, status1);

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
	ssd1306_clear(device, &device->bounds);

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
	ssd1306_status(device, ssd1306_status_int, status0);

	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(PAUSE_MILLIS));

	ssd1306_bounds_t rnd_bounds = bnd1;

	// flip: 0, [+0+0, +128+16]
	// flip: 1, [+0+48, +128+64]
	if( device->flip ) {
		rnd_bounds.y0 = 0;
		rnd_bounds.y1 = bnd1.y0;
	} else {
		rnd_bounds.y0 = bnd1.y1;
		rnd_bounds.y1 = device->y1;
	}

	LOG_BOUNDS_I("rnd region", &rnd_bounds);

	status1[0] = CONFIG_SSD1306_TEXT_INVERT;

	ssd1306_auto_update(device, false);
	for( unsigned k = 0; k < DEMO_TIMEOUT / FRAME_MILLIS; k++ ) {
		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(FRAME_MILLIS));

		sprintf(status1 + 1, "%8d", k);

		ssd1306_fill_randomly(device, &rnd_bounds);
		ssd1306_status(device, ssd1306_status_ext, status1);
		ssd1306_update(device);
	}
	ssd1306_auto_update(device, true);

	ssd1306_status(device, ssd1306_status_ext, "restarting \x07soon\x07...");
	vTaskDelayUntil(&ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
	esp_restart();
}
