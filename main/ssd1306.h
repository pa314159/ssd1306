#if !defined(__SSD1306_H)
#define __SSD1306_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct ssd1306_s* ssd1306_t;
typedef struct ssd1306_init_s* ssd1306_init_t;

typedef uint8_t ssd1306_glyph_t[8];

typedef struct ssd1306_init_s {
	unsigned width;
	unsigned height;

	bool flip;
	bool invert;

	const ssd1306_glyph_t* font;

	struct {
		enum {
			ssd1306_type_i2c, ssd1306_type_spi
		} type;

		int16_t rst;

		union {
			struct {
				int16_t sda;
				int16_t scl;

				uint16_t port;
			};
			struct {
				int16_t mosi;
				int16_t sclk;
				int16_t cs;
				int16_t dc;

				uint16_t host;
			};
		};

		uint16_t freq;
	} connection;
} ssd1306_init_s;

typedef struct {
	int x, y;
	unsigned w, h;
} ssd1306_bounds_t;

#undef __SSD1306_FREE

// initialisation
ssd1306_init_t ssd1306_create_init();
ssd1306_t      ssd1306_init(ssd1306_init_t init);
#if __SSD1306_FREE
void           ssd1306_free(ssd1306_t device);
#endif

// features
void ssd1306_on(ssd1306_t device, bool on);
void ssd1306_contrast(ssd1306_t device, uint8_t contrast);

void ssd1306_clear_b(ssd1306_t device, const ssd1306_bounds_t* bounds);
void ssd1306_bitmap_b(ssd1306_t device, const uint8_t* bitmap, const ssd1306_bounds_t* bounds);
void ssd1306_text_b(ssd1306_t device, const char* text, const ssd1306_bounds_t* bounds);

#define ssd1306_clear(device, _x, _y, _w, _h) \
	do { \
		const ssd1306_bounds_t b = { \
			x: _x, y: _y, w: _w, h: _h, \
		}; \
		ssd1306_clear_b(device, &b); \
	} while( 0 )
#define ssd1306_bitmap(device, bitmap, _x, _y, _w, _h) \
	do { \
		const ssd1306_bounds_t b = { \
			x: _x, y: _y, w: _w, h: _h, \
		}; \
		ssd1306_bitmap_b(device, bitmap, &b); \
	} while( 0 )
#define ssd1306_text(device, text, _x, _y, _w) \
	do { \
		const ssd1306_bounds_t b = { \
			x: _x, y: _y, w: _w, \
		}; \
		ssd1306_text_b(device, text, &b); \
	} while( 0 )

void ssd1306_auto_flush(ssd1306_t device, bool on);

#if defined(__cplusplus)
}
#endif

#endif
