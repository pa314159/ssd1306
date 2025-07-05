#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-int.h"

static void ssd1306_draw_page_1(ssd1306_t device, 
	uint8_t page, int16_t offset, uint16_t width, const uint8_t* data, int8_t bits);
static void ssd1306_draw_page_2(ssd1306_t device,
	uint8_t page, int16_t offset, uint16_t width, const uint8_t* data, int8_t bits, uint8_t d_mask);

static inline bool adjust_source_bounds(ssd1306_bounds_t* target,
	const ssd1306_bitmap_t* bitmap,
	const ssd1306_bounds_t* source)
{
	target->head = POINT_ZERO;

	ssd1306_bounds_resize(target, bitmap->size);

	if( source != NULL && !ssd1306_bounds_intersect(target, source) ) {
		LOG_BOUNDS_D("non visible target", target);
		LOG_BOUNDS_D("            source", source);

		return false;
	}

	LOG_BOUNDS_T("target", target);

	return true;
}

static inline bool adjust_both_bounds(ssd1306_bounds_t* target, ssd1306_bounds_t* source)
{
	ssd1306_bounds_t s_temp = *source;

	ssd1306_bounds_move_to(&s_temp, target->head);

	if( ssd1306_bounds_intersect(target, &s_temp) ) {
		LOG_BOUNDS_T("target", target);
		LOG_BOUNDS_T("source", source);
	} else {
		LOG_BOUNDS_W("weird case, target", target);
		LOG_BOUNDS_W("            source", source);

		return false;
	}

	return true;
}

void ssd1306_draw2(ssd1306_t device, const ssd1306_bitmap_t* bitmap,
	const ssd1306_bounds_t* target,	const ssd1306_bounds_t* source)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(bitmap);

	ssd1306_bounds_t s_bounds;
	ssd1306_bounds_t d_bounds;

	if( !adjust_source_bounds(&s_bounds, bitmap, source) ) {
		return;
	}
	if( !ssd1306_adjust_target_bounds(&d_bounds, device, target) ) {
		return;
	}
	if( !adjust_both_bounds(&d_bounds, &s_bounds) ) {
		return;
	}

	if( !ssd1306_acquire(device) ) {
		LOG_W("Couldn't take mutex");

		return;
	}

	ssd1306_update_internal(device, &d_bounds);

	ssd1306_release(device);
}

void ssd1306_draw(ssd1306_t device, const ssd1306_bounds_t* target,
	const ssd1306_bitmap_t* bitmap)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(target);
	ABORT_IF_NULL(bitmap);

	ssd1306_bounds_t trimmed = *target;

	if( !ssd1306_trim(device, &trimmed, &bitmap->size) ) {
		return;
	}
	if( !ssd1306_acquire(device) ) {
		LOG_W("Couldn't take mutex");

		return;
	}

	ssd1306_draw_internal(device, target, &trimmed, bitmap);
	ssd1306_update_internal(device, &trimmed);

	ssd1306_release(device);
}

void ssd1306_draw_c(ssd1306_t device, const ssd1306_bitmap_t* bitmap)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(bitmap);

	if( !ssd1306_acquire(device) ) {
		LOG_W("Couldn't take mutex");

		return;
	}

	ssd1306_bounds_t bounds;
	ssd1306_center_bounds(device, &bounds, bitmap);

	ssd1306_draw_internal(device, &bounds, &bounds, bitmap);
	ssd1306_update_internal(device, &bounds);

	ssd1306_release(device);
}

void ssd1306_draw_internal(ssd1306_t device,
	const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed,
	const ssd1306_bitmap_t* bitmap)
{
	const uint8_t* image = bitmap->image;

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
			ssd1306_draw_page_1(device, page, trimmed->x0, trimmed_w, image, -t_bits);
		}

		page++;

		if( t_bits && (page < b_page) ) {
			ssd1306_draw_page_1(device, page, trimmed->x0, trimmed_w, image, 8-t_bits);
		}
		if( b_bits && (page == b_page) ) {
			ssd1306_draw_page_1(device, page, trimmed->x0, trimmed_w, image, b_bits);
		}
	}
}

void ssd1306_draw_page_1(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, const uint8_t* data, int8_t s_bits)
{
	LOG_T("page %d with s_bits = %+d", page, s_bits);

	ssd1306_draw_page_2(device, page, offset, width, data, s_bits, set_bits(s_bits));
}

void ssd1306_draw_page_2(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, const uint8_t* data, int8_t s_bits, uint8_t d_mask)
{
	uint8_t* buff = ssd1306_raster(device, page) + offset;

	if( d_mask == 0xff && s_bits == 0 ) {
		LOG_T("page %d from %p", page, data);

		memcpy(buff, data, width);
	} else {
		const uint8_t s_mask = ~d_mask;

		LOG_T("page %d with d_mask 0x%02x, s_bits = %+d, s_mask = 0x%02x, from %p", page, d_mask, s_bits, s_mask, data);

		ssd1306_dump(data, width, "Source buff");

		for( unsigned x = 0; x < width; x++ ) {
			buff[x] = (buff[x] & d_mask) | (shift_bits(data[x], s_bits) & s_mask);
		}

		ssd1306_dump(buff, width, "Raster buff");
	}
}
