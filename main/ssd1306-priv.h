#pragma once

#include "os.h"

typedef struct ssd1306_s {
	struct ssd1306_init_s;

	union {
		ssd1306_i2c_t i2c;
		ssd1306_spi_t spi;
	};

	unsigned pages;

	bool volatile active;
	unsigned no_flush;
	SemaphoreHandle_t mutex;
	TaskHandle_t task;
} ssd1306_s;

#define raster_head(device)            ((uint8_t*)((ssd1306_t)(device)+1))
#define raster_page_head(device, page) (raster_head(device) + (page) * (device->width+1))
#define raster_page_data(device, page) (raster_page_head(device, page) + 1)

void ssd1306_task(ssd1306_t device);
void ssd1306_send(ssd1306_t device);
void ssd1306_send_data(ssd1306_t device, const uint8_t* data, size_t size);

#if CONFIG_SSD1306_SPLASH
void ssd1306_show_splash(ssd1306_t device);
#endif

void ssd1306_fill_page(ssd1306_t device, unsigned page, unsigned segments, unsigned width, const uint8_t* data, int bits);

#if CONFIG_SSD1306_LOGGING_LEVEL_VERBOSE
#include <stdarg.h>
void ssd1306_dump(const void* data, size_t size, const char* format, ...);
#else
#define ssd1306_dump(data, size, format, ...) (void)0
#endif
