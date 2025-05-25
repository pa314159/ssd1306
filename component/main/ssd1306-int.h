#pragma once

#include "os.h"

#define SSD1306_TEXT_HEIGHT 8
#define SSD1306_PAGE_HEIGHT 8

typedef enum {
	anim_init,
	anim_wait,
	anim_move,
} anim_state_t;

typedef struct PACKED status_info_t {
	anim_state_t state;
	ssd1306_bitmap_t* bitmap;
	TickType_t ticks;
	uint8_t page;
	int16_t offset;
} status_info_t;

typedef struct ssd1306_int_s {
	struct ssd1306_s;

	const ssd1306_connection_t connection;

	union {
		ssd1306_i2c_t i2c;
		ssd1306_spi_t spi;
	};

	bool volatile active;
	unsigned no_update;

#if CONFIG_SSD1306_OPTIMIZE
	QueueHandle_t queue;
	ssd1306_bounds_t dirty_bounds;
#else
	TaskHandle_t task;
#endif
	SemaphoreHandle_t mutex;

	status_info_t statuses[2];

	uint8_t buff[];
} ssd1306_int_s;

void ssd1306_task(ssd1306_int_t dev);
void ssd1306_send_buff(ssd1306_int_t dev, uint8_t ctl, const uint8_t* buff, uint16_t size);

inline uint16_t minu(uint16_t a, uint16_t b)
{
	return a < b ? a : b;
}
inline int16_t mini(int16_t a, int16_t b)
{
	return a < b ? a : b;
}

inline uint8_t shift_bits(uint8_t value, int8_t bits)
{
	return (bits < 0) ? (value << -bits) : (bits > 0) ? (value >> bits) : value;
}

inline uint8_t set_bits(int8_t bits)
{
	if( bits < 0 ) {
		return (uint8_t)(1 << -bits) - 1;
	} else if( bits > 0 ) {
		return (int8_t)0x80 >> (bits - 1);
	} else {
		return 0;
	}
}

inline unsigned bytes_cap(uint8_t bits)
{
	return bits / 8 + (bits % 8 ? 1 : 0);
}
