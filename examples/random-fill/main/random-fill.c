#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>
#include <ssd1306-misc.h>

#include "ssd1306-int.h" // to reuse "mini"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define PAUSE_MILLIS      1500
#define FRAME_MILLIS      50

#if CONFIG_DISPLAY0_TYPE + CONFIG_DISPLAY1_TYPE == 0
#error no display installed
#endif

void expand_black_rectangle(ssd1306_t device, TickType_t* ticks)
{
	ssd1306_fill_randomly(device, &device->bounds);
	vTaskDelayUntil(ticks, pdMS_TO_TICKS(PAUSE_MILLIS));

	const int16_t dim = mini(device->w, device->h) / 2 - 1;

	ssd1306_bounds_t bounds = {
		x0: dim, y0: dim, 
		x1: device->w - dim,
		y1: device->h - dim,
	};

	while( bounds.x0 >= 0 && bounds.y0 >= 0 ) {
		ssd1306_clear(device, &bounds);

		vTaskDelayUntil(ticks, pdMS_TO_TICKS(FRAME_MILLIS));

		bounds.x0--;
		bounds.y0--;
		bounds.x1++;
		bounds.y1++;
	}

	vTaskDelayUntil(ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
}

void shrink_black_rectangle(ssd1306_t device, TickType_t* ticks)
{
	ssd1306_fill_randomly(device, &device->bounds);
	vTaskDelayUntil(ticks, pdMS_TO_TICKS(PAUSE_MILLIS));

	const unsigned dim = mini(device->w, device->h) / 2;
	ssd1306_bitmap_t* bitmap = ssd1306_create_bitmap((ssd1306_size_t){ device->w, device->h });
	ssd1306_bounds_t bounds = device->bounds;

	ssd1306_grab(device, &bounds, bitmap);

	while( bounds.x0 <= dim && bounds.y0 <= dim ) {
		ssd1306_auto_update(device, false);
		ssd1306_draw(device, &device->bounds, bitmap);
		ssd1306_clear(device, &bounds);
		ssd1306_auto_update(device, true);

		bounds.x0++;
		bounds.y0++;
		bounds.x1--;
		bounds.y1--;

		vTaskDelayUntil(ticks, pdMS_TO_TICKS(FRAME_MILLIS));
	}

	free(bitmap);
	vTaskDelayUntil(ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
}

void expand_random_rectangle(ssd1306_t device, TickType_t* ticks)
{
	ssd1306_clear(device, &device->bounds);
	vTaskDelayUntil(ticks, pdMS_TO_TICKS(PAUSE_MILLIS));

	const int16_t dim = mini(device->w, device->h) / 2 - 1;

	ssd1306_bounds_t bounds = {
		x0: dim, y0: dim, 
		x1: device->w - dim,
		y1: device->h - dim,
	};

	while( bounds.x0 >= 0 && bounds.y0 >= 0 ) {
		ssd1306_fill_randomly(device, &bounds);
		vTaskDelayUntil(ticks, pdMS_TO_TICKS(FRAME_MILLIS));

		bounds.x0--;
		bounds.y0--;
		bounds.x1++;
		bounds.y1++;
	}

	vTaskDelayUntil(ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
}

void run_demo(ssd1306_t device)
{
	TickType_t ticks = xTaskGetTickCount();

	bool invert = false;

	while( true ) {
		expand_black_rectangle(device, &ticks);
		shrink_black_rectangle(device, &ticks);
		expand_random_rectangle(device, &ticks);

		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
		ssd1306_invert(device, invert = !invert);
	}
}

void app_main(void)
{
	ssd1306_init_t init[2] = {
#if CONFIG_DISPLAY0_TYPE
		ssd1306_create_init(CONFIG_DISPLAY0_TYPE),
#else
		NULL,
#endif
#if CONFIG_DISPLAY1_TYPE
		ssd1306_create_init(CONFIG_DISPLAY1_TYPE),
#else
		NULL,
#endif
	};

#if CONFIG_DISPLAY0_TYPE
	init[0]->panel = CONFIG_DISPLAY0_PANEL_TYPE;
	init[0]->flip = CONFIG_DISPLAY0_FLIP;
	init[0]->invert = CONFIG_DISPLAY0_INVERT;
	init[0]->contrast = CONFIG_DISPLAY0_CONTRAST;
#endif
#if CONFIG_DISPLAY0_IIC
	init[0]->connection.address = CONFIG_DISPLAY0_IIC_ADDRESS;
	init[0]->connection.port = CONFIG_DISPLAY0_IIC_PORT;
	init[0]->connection.rst = CONFIG_DISPLAY0_IIC_RST_PIN;
#endif
#if CONFIG_DISPLAY0_SPI
	init[0]->connection.cs = CONFIG_DISPLAY0_SPI_CS_PIN;
	init[0]->connection.rst = CONFIG_DISPLAY0_SPI_RST_PIN;
#endif

#if CONFIG_DISPLAY1_TYPE
	init[0]->panel = CONFIG_DISPLAY1_PANEL_TYPE;
	init[1]->flip = CONFIG_DISPLAY1_FLIP;
	init[1]->invert = CONFIG_DISPLAY1_INVERT;
	init[1]->contrast = CONFIG_DISPLAY1_CONTRAST;
#endif
#if CONFIG_DISPLAY1_IIC
	init[1]->connection.address = CONFIG_DISPLAY1_IIC_ADDRESS;
	init[1]->connection.port = CONFIG_DISPLAY1_IIC_PORT;
	init[1]->connection.rst = CONFIG_DISPLAY1_IIC_RST_PIN;
#endif
#if CONFIG_DISPLAY1_SPI
	init[1]->connection.cs = CONFIG_DISPLAY1_SPI_CS_PIN;
	init[1]->connection.rst = CONFIG_DISPLAY1_SPI_RST_PIN;
#endif

	ssd1306_t devs[2] = {
#if CONFIG_DISPLAY0_TYPE
		ssd1306_init(init[0]),
#else
		NULL,
#endif
#if CONFIG_DISPLAY1_TYPE
		ssd1306_init(init[1]),
#else
		NULL,
#endif
	};

#if CONFIG_DISPLAY0_TYPE
	xTaskCreate((TaskFunction_t)run_demo, "task-0", 1024 * CONFIG_SSD1306_STACK_SIZE, (void*)devs[0], 1, NULL);
#endif
#if CONFIG_DISPLAY1_TYPE
	xTaskCreate((TaskFunction_t)run_demo, "task-1", 1024 * CONFIG_SSD1306_STACK_SIZE, (void*)devs[1], 1, NULL);
#endif
}
