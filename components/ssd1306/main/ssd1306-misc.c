#include "ssd1306.h"
#include "ssd1306-misc.h"
#include "ssd1306-int.h"

#include <esp_random.h>

void ssd1306_fill_randomly(ssd1306_t device, const ssd1306_bounds_t* bounds)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(bounds);

	ssd1306_bounds_t trimmed = *bounds;

	if( !ssd1306_trim(device, &trimmed, NULL) ) {
		return;
	}

	if( !ssd1306_acquire(device) ) {
		LOG_W("cannot acquire device");

		return;
	}

	ssd1306_bitmap_t* bitmap = ssd1306_create_bitmap((ssd1306_size_t){
		ssd1306_bounds_width(&trimmed),
		ssd1306_bounds_height(&trimmed)});

	esp_fill_random(bitmap->image, bitmap->w * bytes_cap(bitmap->h));

	ssd1306_draw_internal(device, &trimmed, &trimmed, bitmap);

	free(bitmap);

	ssd1306_update(device, &trimmed);
	ssd1306_release(device);
}
