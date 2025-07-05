#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-int.h"

uint8_t* ssd1306_raster(ssd1306_t device, uint8_t page)
{
	ABORT_IF_NULL(device);

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	return dev->buff + page * device->w;
}

bool ssd1306_trim(ssd1306_t device, ssd1306_bounds_t* bounds, const ssd1306_size_t* size)
{
	uint16_t bounds_w = ssd1306_bounds_width(bounds);
	uint16_t bounds_h = ssd1306_bounds_height(bounds);

	LOG_BOUNDS_T("trimming bounds", bounds);

	if( size ) {
		LOG_T("        to size %ux%u", size->w, size->h);

		bounds->x1 = bounds->x0 + minu(bounds_w, size->w);
		bounds->y1 = bounds->y0 + minu(bounds_h, size->h);

		LOG_BOUNDS_T("truncated bounds", bounds);
	}

	if( ssd1306_bounds_intersect(bounds, &device->bounds) ) {
		LOG_BOUNDS_T("trimmed bounds", bounds);

		return true;
	} else {
		LOG_BOUNDS_T("bounds not visible", bounds);

		return false;
	}
}

bool ssd1306_adjust_target_bounds(ssd1306_bounds_t* target, ssd1306_t device, const ssd1306_bounds_t* source)
{
	*target = device->bounds;

	if( source != NULL && !ssd1306_bounds_intersect(target, source) ) {
		LOG_BOUNDS_T("non visible target", target);
		LOG_BOUNDS_T("            source", source);

		return false;
	}

	LOG_BOUNDS_T("target", target);

	return true;
}
