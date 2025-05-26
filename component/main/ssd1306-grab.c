#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-int.h"

static void ssd1306_grab_page_1(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, uint8_t* data, int8_t bits);
static void ssd1306_grab_page_2(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, uint8_t* data, int8_t bits, uint8_t d_mask);

void ssd1306_grab_b(ssd1306_t device, const ssd1306_bounds_t* bounds, ssd1306_bitmap_t* bitmap)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(bounds);
	ABORT_IF_NULL(bitmap);

	ssd1306_bounds_t trimmed = *bounds;

	if( !ssd1306_trim(device, &trimmed, &bitmap->size) ) {
		return;
	}

	ssd1306_grab_internal(device, bounds, &trimmed, bitmap);
}

void ssd1306_grab_c(ssd1306_t device, ssd1306_bitmap_t* bitmap)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(bitmap);

	ssd1306_bounds_t bounds;
	ssd1306_center_bounds(device, &bounds, bitmap);
	ssd1306_grab_internal(device, &bounds, &bounds, bitmap);
}

void ssd1306_grab_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed, ssd1306_bitmap_t* bitmap)
{
	uint8_t* image = bitmap->image;

	if( bounds->x0 < 0 ) {
		image += -bounds->x0;
	}

	int16_t y_bits = 0;

	if( bounds->y0 < 0 ) {
		y_bits = bounds->y0 & 7;

		image += ((-bounds->y0-1) / 8) * bitmap->w;

		LOG_D("Image shifted by %d for y_bits %d", image - bitmap->image, y_bits);
	}

	const uint16_t trimmed_w = ssd1306_bounds_width(trimmed);

	const int16_t t_page = bounds->y0 < 0 ? -1 : trimmed->y0 >> 3;
	const int16_t t_bits = bounds->y0 < 0 ? y_bits : trimmed->y0 & 7;

	const int16_t b_page = trimmed->y1 >> 3;
	const int16_t b_bits = trimmed->y1 & 7;

	LOG_D("t_page = %d, t_bits = %d", t_page, t_bits);
	LOG_D("b_page = %d, b_bits = %d", b_page, b_bits);

	for( int16_t page = t_page; page < b_page; image += bitmap->w ) {
		if( page >= 0 ) {
			ssd1306_grab_page_1(device, page, trimmed->x0, trimmed_w, image, t_bits);
		}

		page++;

		if( t_bits && (page < b_page) ) {
			ssd1306_grab_page_1(device, page, trimmed->x0, trimmed_w, image, t_bits - 8);
		}
		if( b_bits && (page == b_page) ) {
			ssd1306_grab_page_2(device, page, trimmed->x0, trimmed_w, image, t_bits - 8, set_bits(b_bits - 8));
		}
	}
}

void ssd1306_grab_page_1(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, uint8_t* data, int8_t s_bits)
{
	ssd1306_grab_page_2(device, page, offset, width, data, s_bits, set_bits(s_bits));
}

void ssd1306_grab_page_2(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, uint8_t* data, int8_t s_bits, uint8_t d_mask)
{
	const uint8_t* buff = ssd1306_raster(device, page) + offset;

	if( d_mask == 0xff && s_bits == 0 ) {
		LOG_V("get page %d from %p", page, data);

		memcpy(data, buff, width);
	} else {
		const uint8_t s_mask = ~d_mask;

		LOG_V("get page %d with d_mask 0x%02x, s_bits = %+d, s_mask = 0x%02x, from %p", page, d_mask, s_bits, s_mask, data);

		ssd1306_dump(buff, width, "Raster buff");

		for( unsigned x = 0; x < width; x++ ) {
			data[x] = (data[x] & d_mask) | (shift_bits(buff[x], s_bits) & s_mask);
		}

		ssd1306_dump(data, width, "Target buff");
	}
}
