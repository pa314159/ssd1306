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
	const ssd1306_bounds_t;
	anim_state_t state;
	ssd1306_bitmap_t* bitmap;
	TickType_t ticks;
	int16_t offset;
} status_info_t;

typedef struct ssd1306_int_s {
	struct ssd1306_s;

	const ssd1306_connection_t connection;

	union {
		ssd1306_iic_t i2c;
		ssd1306_spi_t spi;
	};

	bool volatile active;
	int16_t defer_update;

#if CONFIG_SSD1306_OPTIMIZE
	QueueHandle_t queue;
	ssd1306_bounds_t* dirty_bounds;
#else
	TaskHandle_t task;
#endif
	SemaphoreHandle_t mutex;

	status_info_t statuses[2];

	uint8_t buff[];
} ssd1306_int_s;


extern const ssd1306_point_t POINT_ZERO;

void ssd1306_task(ssd1306_int_t dev);
void ssd1306_send_buff(ssd1306_int_t dev, uint8_t ctl, const uint8_t* buff, uint16_t size);
bool ssd1306_trim(ssd1306_t device, ssd1306_bounds_t* bounds, const ssd1306_size_t* size);

bool ssd1306_adjust_target_bounds(ssd1306_bounds_t* bounds, ssd1306_t device, const ssd1306_bounds_t* narrow);

void ssd1306_clear_internal(ssd1306_t device,
		const ssd1306_bounds_t* target);
void ssd1306_draw_internal(ssd1306_t device, 
		const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed,
		const ssd1306_bitmap_t* bitmap);
void ssd1306_grab_internal(ssd1306_t device, 
		const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed,
		ssd1306_bitmap_t* bitmap);

#if CONFIG_COMPILER_OPTIMIZATION_NONE
#define inline inline static
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

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

inline uint16_t ssd1306_status_index(ssd1306_int_t dev, ssd1306_status_t status)
{
	if( status <= ssd1306_status_1 ) {
		return status;
	}

	return dev->flip ? 1 - ((int)status - (int)ssd1306_status_ext) : (int)status - (int)ssd1306_status_ext;
}

#if CONFIG_COMPILER_OPTIMIZATION_NONE
#undef inline
#endif
