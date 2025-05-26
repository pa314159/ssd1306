#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-int.h"

ssd1306_bitmap_t* ssd1306_create_bitmap(uint16_t width, uint16_t height)
{
	const size_t length = sizeof(ssd1306_bitmap_t) + bytes_cap(height) * width;

	ssd1306_bitmap_t* bitmap = calloc(1, length);

	ABORT_IF(bitmap == NULL, "cannot allocate memory for bitmap of size %u", length);

	LOG_D("allocated bitmap of %ux%u, %u bytes at %p", width, height, length, bitmap);

	const ssd1306_size_t size = {
		width: width,
		height: height,
	};

	memcpy((void*)&bitmap->size, &size, sizeof(size));

	return bitmap;
}

void ssd1306_center_bounds(ssd1306_t device, ssd1306_bounds_t* bounds, const ssd1306_bitmap_t* bitmap)
{
	bounds->x0 = ((int)device->w - (int)bitmap->w) / 2;
	bounds->y0 = ((int)device->h - (int)bitmap->h) / 2;
	bounds->x1 = ((int)device->w + (int)bitmap->w) / 2;
	bounds->y1 = ((int)device->h + (int)bitmap->h) / 2;
}
