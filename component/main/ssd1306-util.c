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

	if( size ) {
		LOG_V(">> bounds [%+d%+d, %+d%+d], size %ux%u",
			bounds->x0, bounds->y0, bounds->x1, bounds->y1,
			size->w, size->h);

		bounds->x1 = bounds->x0 + minu(bounds_w, size->w);
		bounds->y1 = bounds->y0 + minu(bounds_h, size->h);
	} else {
		LOG_V(">> bounds [%+d%+d, %+d%+d]",
			bounds->x0, bounds->y0, bounds->x1, bounds->y1);
	}

	if( ssd1306_bounds_intersect(bounds, &device->bounds) ) {
		LOG_V("<< bounds [%+d%+d, %+d%+d], size %ux%u",
			bounds->x0, bounds->y0, bounds->x1, bounds->y1,
			ssd1306_bounds_width(bounds), ssd1306_bounds_height(bounds));

		return true;
	} else {
		LOG_V("<< bounds not visible [%+d%+d, %+d%+d]",
			bounds->x0, bounds->y0, bounds->x1, bounds->y1);

		return false;
	}
}
