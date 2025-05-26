#include "sdkconfig.h"

#include <stdio.h>
#include <ssd1306.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_random.h>

#define PAUSE_MILLIS      1500
#define FRAME_MILLIS      100

#if CONFIG_DISPLAY0_TYPE + CONFIG_DISPLAY1_TYPE == 0
#error no display installed
#endif

inline int mini(int a, int b)
{
	return a < b ? a : b;
}

void fill_with_random(ssd1306_t device)
{
	ssd1306_acquire(device);
	for( unsigned page = 0; page < device->pages; page++ ) {
		esp_fill_random(ssd1306_raster(device, page), device->width);
	}
	ssd1306_update(device, NULL);
	ssd1306_release(device);
}

void expand_black_rectangle(ssd1306_t device, TickType_t* ticks)
{
	const int16_t dim = mini(device->width, device->height) / 2 - 1;

	ssd1306_bounds_t bounds = {
		x0: dim, y0: dim, 
		x1: device->width - dim,
		y1: device->height - dim,
	};

	while( bounds.x0 >= 0 && bounds.y0 >= 0 ) {
		ssd1306_clear_b(device, &bounds);

		vTaskDelayUntil(ticks, pdMS_TO_TICKS(FRAME_MILLIS));

		bounds.x0--;
		bounds.y0--;
		bounds.x1++;
		bounds.y1++;
	}
}

void shrink_black_rectangle(ssd1306_t device, TickType_t* ticks)
{
	const unsigned dim = mini(device->width, device->height) / 2 - 1;

	ssd1306_bounds_t bounds = device->bounds;

	ssd1306_bitmap_t* temp = ssd1306_create_bitmap(device->width, device->height);

	ssd1306_grab_b(device, NULL, temp);

	while( bounds.x0 <= dim && bounds.y0 <= dim ) {
		ssd1306_auto_update(device, false);
		ssd1306_draw_b(device, NULL, temp);
		ssd1306_clear_b(device, &bounds);
		ssd1306_auto_update(device, true);

		vTaskDelayUntil(ticks, pdMS_TO_TICKS(FRAME_MILLIS));

		bounds.x0++;
		bounds.y0++;
		bounds.x1--;
		bounds.y1--;
	}
}

void run_demo(ssd1306_t device)
{
	TickType_t ticks = xTaskGetTickCount();

	while( true ) {
		fill_with_random(device);
		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
		expand_black_rectangle(device, &ticks);

		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
		fill_with_random(device);
		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
		shrink_black_rectangle(device, &ticks);

		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(PAUSE_MILLIS));

		for( uint8_t ct = 0xff; ; ct >>= 1 ) {
			ssd1306_contrast(device, ct);
			vTaskDelayUntil(&ticks, pdMS_TO_TICKS(250));

			if( ct == 0 ) {
				break;
			}
		}

		vTaskDelayUntil(&ticks, pdMS_TO_TICKS(PAUSE_MILLIS));
		ssd1306_contrast(device, CONFIG_SSD1306_CONTRAST);

		for( int16_t k = 0; k < 16; k++ ) {
			vTaskDelayUntil(&ticks, pdMS_TO_TICKS(250));
			ssd1306_on(device, k & 1);
		}

		for( int16_t k = 0; k < 16; k++ ) {
			vTaskDelayUntil(&ticks, pdMS_TO_TICKS(250));
			ssd1306_invert(device, !(k & 1));
		}
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
	init[0]->panel = CONFIG_DISPLAY0_TYPE;
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
	init[0]->panel = CONFIG_DISPLAY1_TYPE;
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
