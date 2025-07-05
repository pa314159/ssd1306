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

#define _Nullable

typedef struct PACKED ssd1306_point_t {
	int16_t x, y;
} ssd1306_point_t;

typedef struct PACKED ssd1306_size_t {
	uint16_t w;
	uint16_t h;
} ssd1306_size_t;

typedef struct PACKED ssd1306_bounds_t {
	union {
		struct {
			ssd1306_point_t head;
			ssd1306_point_t tail;
		};
		struct {
			int16_t x0, y0, x1, y1;
		};
	};
} ssd1306_bounds_t;

typedef struct PACKED ssd1306_bitmap_t {
	const union {
		struct ssd1306_size_t;
		ssd1306_size_t size;
	};
	uint8_t image[];
} ssd1306_bitmap_t;

typedef struct PACKED ssd1306_glyph_t {
	const uint8_t w;
	const uint8_t h;
	const uint8_t image[8];
} ssd1306_glyph_t;

typedef enum {
	ssd1306_interface_any,
	ssd1306_interface_iic,
	ssd1306_interface_spi,
} ssd1306_interface_t;

typedef struct PACKED ssd1306_connection_t {
	const ssd1306_interface_t type;

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

typedef enum {
	 ssd1406_128x32,
	 ssd1406_128x64,
} ssd1306_panel_t;

typedef enum {
	ssd1306_status_0,
	ssd1306_status_1,
	ssd1306_status_ext, // the external status line - the line that's Close To The Edge, YES ;)
	ssd1306_status_int, // the internal status line
} ssd1306_status_t;

typedef struct PACKED ssd1306_init_s {
	struct PACKED {
		bool free; // structure to be freed by ssd1306_init

		ssd1306_panel_t panel:1;

		bool flip;
		bool invert;
	};

	uint8_t contrast;

	const ssd1306_glyph_t* font;

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

	union {
		struct ssd1306_size_t;
		ssd1306_size_t size;
	};

	uint8_t pages;

	const ssd1306_glyph_t* font;
} ssd1306_s;
typedef const ssd1306_s* ssd1306_t;

#undef __SSD1306_FREE

extern const ssd1306_bitmap_t* splash_bmp;

// initialisation
ssd1306_init_t ssd1306_create_init(ssd1306_interface_t type); // returned pointer can be freed after initialisation
ssd1306_t      ssd1306_init(ssd1306_init_t _Nullable init); // pass NULL to use the default configuration

#if __SSD1306_FREE
void           ssd1306_free(ssd1306_t device);
#endif

// operations
void ssd1306_on(ssd1306_t device, bool on);
void ssd1306_invert(ssd1306_t device, bool on);
void ssd1306_contrast(ssd1306_t device, uint8_t contrast);

// lower level
/**
 * @brief Turns the auto-update on or off.
 *
 * @param device Device handle of the SSD1306 display
 */
void ssd1306_auto_update(ssd1306_t device, bool on);

/**
 * @brief Enforce an update of the device even though the auto update is off.
 *
 * @param device Device handle of the SSD1306 display
 */
void ssd1306_update(ssd1306_t device);

/**
 * @brief Acquire exclusive access to the device.
 *
 * @param device Device handle of the SSD1306 display
 */
bool ssd1306_acquire(ssd1306_t device);

/**
 * @brief Release the exclusive access to the device.
 *
 * @param device Device handle of the SSD1306 display
 */
void ssd1306_release(ssd1306_t device);

// lowest level
uint8_t* ssd1306_raster(ssd1306_t device, uint8_t page);
ssd1306_bitmap_t* ssd1306_create_bitmap(const ssd1306_size_t size); // returned pointer must be freed after use
ssd1306_bitmap_t* ssd1306_text_bitmap(ssd1306_t device, const char* format, ...);
uint16_t ssd1306_text_width(ssd1306_t device, const char* text);

// geometry
void ssd1306_bounds_union(ssd1306_bounds_t* target, const ssd1306_bounds_t* _Nullable source);
bool ssd1306_bounds_intersect(ssd1306_bounds_t* target, const ssd1306_bounds_t* _Nullable source);
void ssd1306_bounds_resize(ssd1306_bounds_t* target, const ssd1306_size_t size);
void ssd1306_bounds_move_to(ssd1306_bounds_t* target, const ssd1306_point_t origin);
void ssd1306_bounds_move_by(ssd1306_bounds_t* target, const ssd1306_point_t offset);
uint16_t ssd1306_bounds_width(const ssd1306_bounds_t* bounds);
uint16_t ssd1306_bounds_height(const ssd1306_bounds_t* bounds);
ssd1306_point_t ssd1306_bounds_center(const ssd1306_bounds_t* bounds);

// features

/**
 * @brief Clear the display within the provided bounds.
 *
 * @param device Device handle of the SSD1306 display
 * @param bounds The bounds of the rectangle to be cleared
 */
void ssd1306_clear(ssd1306_t device,
		const ssd1306_bounds_t* _Nullable bounds);

void ssd1306_draw(ssd1306_t device,
		const ssd1306_bounds_t* _Nullable target,
		const ssd1306_bitmap_t* bitmap);
/**
 * @brief Draw a bitmap.
 *
 * @param device Device handle of the SSD1306 display
 * @param bitmap The bitmap to be drawn
 * @param target The bounds of the target rectangle to be drawn
 * @param source An optional rectangle of a cut of the image
 */
void ssd1306_draw2(ssd1306_t device,
		const ssd1306_bitmap_t* bitmap,
		const ssd1306_bounds_t* _Nullable target,
		const ssd1306_bounds_t* _Nullable source);

/**
 * @brief Draw a bitmap at center
 *
 * @param device Device handle of the SSD1306 display
 * @param bitmap The bitmap to be drawn
*/
void ssd1306_draw_c(ssd1306_t device,
		const ssd1306_bitmap_t* bitmap);

/**
 * @brief Grab the display content into a bitmap.
 *
 * @param device Device handle of the SSD1306 display
 * @param source The bounds of the source rectangle to be grabbed
 * @param bitmap The bitmap that will hold the content
 */
void ssd1306_grab(ssd1306_t device, const ssd1306_bounds_t* source,
		ssd1306_bitmap_t* bitmap);

/**
 * @brief Grab into bitmap from center.
 *
 * @param device Device handle of the SSD1306 display
 * @param bitmap The bitmap that will hold the content
 */
void ssd1306_grab_c(ssd1306_t device, ssd1306_bitmap_t* bitmap);

void ssd1306_text(ssd1306_t device, const ssd1306_bounds_t* target, const char* format, ...);

void ssd1306_status(ssd1306_t device, ssd1306_status_t status,
		const char* format, ...);
const ssd1306_bounds_t* ssd1306_status_bounds(ssd1306_t device, ssd1306_status_t status,
		ssd1306_bounds_t* _Nullable target);

void ssd1306_center_bounds(ssd1306_t device, ssd1306_bounds_t* target, const ssd1306_bitmap_t* bitmap);

#if defined(__cplusplus)
}
#endif

#endif
