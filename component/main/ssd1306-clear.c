#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-int.h"

static void ssd1306_clear_page(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, uint8_t mask);

void ssd1306_clear(ssd1306_t device, const ssd1306_bounds_t* bounds)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(bounds);

	ssd1306_bounds_t trimmed = *bounds;
	
	if( !ssd1306_trim(device, &trimmed, NULL) ) {
		return;
	}
	if( !ssd1306_acquire(device) ) {
		LOG_W("Couldn't take mutex");

		return;
	}

	ssd1306_clear_internal(device, bounds, &trimmed);

	ssd1306_update(device, &trimmed);
	ssd1306_release(device);
}

void ssd1306_clear_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed)
{
	const uint16_t trimmed_w = ssd1306_bounds_width(trimmed);

	const uint16_t t_page = trimmed->y0 / 8;
	const uint16_t t_bits = trimmed->y0 % 8;

	const uint16_t b_page = trimmed->y1 / 8;
	const uint16_t b_bits = trimmed->y1 % 8;

	const uint8_t t_mask = set_bits(-t_bits);
	const uint8_t b_mask = set_bits(8 - b_bits);

	LOG_D("t_page = %u, t_bits = %u, t_mask = 0x%02x", t_page, t_bits, t_mask);
	LOG_D("b_page = %u, b_bits = %u, b_mask = 0x%02x", b_page, b_bits, b_mask);

	uint8_t page = t_page;

	if( t_bits ) {
		ssd1306_clear_page(device, page++, trimmed->x0, trimmed_w, t_mask);
	}

	while( page < b_page ) {
		ssd1306_clear_page(device, page++, trimmed->x0, trimmed_w, 0);
	}

	if( b_bits ) {
		ssd1306_clear_page(device, page++, trimmed->x0, trimmed_w, b_mask);
	}
}

void ssd1306_clear_page(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, uint8_t mask)
{
	uint8_t* buff = ssd1306_raster(device, page) + offset;

	if( mask == 0 ) {
		LOG_V("clear page %d", page);

		memset(buff, 0, width);
	} else {
		LOG_V("clear page %d with mask 0x%02x", page, mask);

		for( unsigned x = 0; x < width; x++ ) {
			buff[x] &= mask;
		}
	}
}
