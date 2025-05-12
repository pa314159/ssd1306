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
	unsigned no_update;

#if CONFIG_SSD1306_OPTIMIZE
	QueueHandle_t queue;
#else
	TaskHandle_t task;
#endif

	SemaphoreHandle_t mutex;

	uint8_t head[1];
	uint8_t data[];
} ssd1306_s;

#if CONFIG_SSD1306_OPTIMIZE
typedef struct {
	const uint8_t* buffer;
	ssd1306_bounds_t bounds;
} ssd1306_message_t;
#endif

#define raster_head(device)       ((device)->head)
#define raster_page(device, page) ((device)->data + (page) * device->width)

void ssd1306_task(ssd1306_t device);
void ssd1306_send_data(ssd1306_t device, const uint8_t* data, size_t size);

#if CONFIG_SSD1306_SPLASH
void ssd1306_show_splash(ssd1306_t device);
#endif

inline unsigned minu(unsigned a, unsigned b)
{
	return a < b ? a : b;
}
inline int mini(int a, int b)
{
	return a < b ? a : b;
}

inline uint8_t shift_bits(uint8_t value, int bits)
{
	return (bits < 0) ? (value << -bits) : (bits > 0) ? (value >> bits) : value;
}

inline uint8_t set_bits(int bits)
{
	if( bits < 0 ) {
		return (uint8_t)(1 << -bits) - 1;
	} else if( bits > 0 ) {
		return (int8_t)0x80 >> (bits - 1);
	} else {
		return 0;
	}
}
