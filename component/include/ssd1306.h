#if !defined(__SSD1306_H)
#define __SSD1306_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#if !defined(PACKED)
#define PACKED __attribute__((packed))
#endif

typedef struct PACKED ssd1306_point_t {
	int16_t x, y;
} ssd1306_point_t;

typedef struct PACKED ssd1306_size_t {
	union {
		uint16_t w, width;
	 };
	union {
		uint16_t h, height;
	};
} ssd1306_size_t;

typedef struct PACKED ssd1306_bounds_t {
	union {
		struct ssd1306_point_t;
		ssd1306_point_t origin;
	};
	union {
		struct ssd1306_size_t;
		ssd1306_size_t size;
	};
} ssd1306_bounds_t;

typedef struct PACKED ssd1306_bitmap_t {
	union {
		struct ssd1306_size_t;
		ssd1306_size_t size;
	};
	uint8_t image[];
} ssd1306_bitmap_t;

typedef struct PACKED ssd1306_glyph_t {
	union {
		uint8_t w, width;
	 };
	union {
		uint8_t h, height;
	};
	uint8_t image[8];
} ssd1306_glyph_t;

typedef enum {
		ssd1306_type_i2c, ssd1306_type_spi
} ssd1306_connection_type_t;

typedef struct PACKED ssd1306_connection_t {
	ssd1306_connection_type_t type;

	int16_t rst;

	union {
		struct PACKED {
			int16_t sda;
			int16_t scl;

			uint16_t port;
			uint8_t address;
		};
		struct PACKED {
			int16_t mosi;
			int16_t sclk;
			int16_t cs;
			int16_t dc;

			uint16_t host;
		};
	};

	uint16_t freq;
} ssd1306_connection_t;

typedef struct PACKED ssd1306_init_s {
	uint8_t id;

	struct PACKED {
		bool flip;
		bool invert;
		bool free; // structure to be freed by ssd1306_init
	};

	uint8_t contrast;

	const union {
		struct ssd1306_size_t;
		struct ssd1306_size_t size;
	};

	const ssd1306_glyph_t* const font;

	ssd1306_connection_t connection;
} ssd1306_init_s;
typedef ssd1306_init_s* ssd1306_init_t;

typedef struct PACKED ssd1306_s {
	uint8_t id;

	struct PACKED {
		bool flip;
	};

	union {
		struct ssd1306_bounds_t;
		ssd1306_bounds_t bounds;
	};

	uint8_t pages;

	const ssd1306_glyph_t* font;
} ssd1306_s;
typedef const ssd1306_s* ssd1306_t;

#undef __SSD1306_FREE

extern const ssd1306_bitmap_t* splash_bmp;

// initialisation
ssd1306_init_t ssd1306_create_init(); // returned pointer can be freed after initialisation
ssd1306_t      ssd1306_init(ssd1306_init_t init); // pass NULL to use the default configuration

#if __SSD1306_FREE
void           ssd1306_free(ssd1306_t device);
#endif

// operations
void ssd1306_on(ssd1306_t device, bool on);
void ssd1306_invert(ssd1306_t device, bool on);
void ssd1306_contrast(ssd1306_t device, uint8_t contrast);

// lower level
void ssd1306_auto_update(ssd1306_t device, bool on);

bool ssd1306_acquire(ssd1306_t device);
void ssd1306_release(ssd1306_t device);
void ssd1306_update(ssd1306_t device, const ssd1306_bounds_t* bounds);

// lowest level
uint8_t* ssd1306_raster(ssd1306_t device, uint8_t page);
ssd1306_bitmap_t* ssd1306_create_bitmap(uint16_t width, uint16_t height); // returned pointer must be freed after use
ssd1306_bitmap_t* ssd1306_text_bitmap(ssd1306_t device, const char* format, ...);
uint16_t ssd1306_text_width(ssd1306_t device, const char* text);

// features
void ssd1306_clear_b(ssd1306_t device, const ssd1306_bounds_t* bounds);
void ssd1306_draw_b(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bitmap_t* bitmap);
void ssd1306_grab_b(ssd1306_t device, const ssd1306_bounds_t* bounds, ssd1306_bitmap_t* bitmap);
void ssd1306_text_b(ssd1306_t device, const ssd1306_bounds_t* bounds, const char* format, ...);

typedef enum {
	ssd1306_status_0,
	ssd1306_status_1,
	ssd1306_status_ext, // the external status line - the line that's Close To The Edge, YES ;)
	ssd1306_status_int, // the internal status line
} ssd1306_status_t;

void ssd1306_status(ssd1306_t device, ssd1306_status_t status, const char* format, ...);
ssd1306_status_t ssd1306_status_bounds(ssd1306_t device, ssd1306_status_t status, ssd1306_bounds_t* bounds);

// others

#define ssd1306_clear(device, _x, _y, _w, _h) \
	do { \
		const ssd1306_bounds_t b = { \
			x: _x, y: _y, width: _w, height: _h, \
		}; \
		ssd1306_clear_b(device, &b); \
	} while( 0 )
	#define ssd1306_draw(device, _x, _y, _w, _h, bitmap) \
	do { \
		const ssd1306_bounds_t b = { \
			x: _x, y: _y, width: _w, height: _h, \
		}; \
		ssd1306_draw_b(device, &b, bitmap); \
	} while( 0 )
#define ssd1306_grab(device, _x, _y, _w, _h, bitmap) \
	do { \
		const ssd1306_bounds_t b = { \
			x: _x, y: _y, width: _w, height: _h, \
		}; \
		ssd1306_grab_b(device, &b, bitmap); \
	} while( 0 )
#define ssd1306_text(device, _x, _y, _w, _h, text) \
	do { \
		const ssd1306_bounds_t b = { \
			x: _x, y: _y, width: _w, height: _h, \
		}; \
		ssd1306_text_b(device, &b, text); \
	} while( 0 )

#if defined(__cplusplus)
}
#endif

#endif
