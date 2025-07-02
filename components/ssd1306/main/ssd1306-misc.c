#include "ssd1306.h"
#include "ssd1306-misc.h"
#include "ssd1306-int.h"

#include <esp_random.h>

void ssd1306_fill_randomly(ssd1306_t device, const ssd1306_bounds_t* bounds)
{
	ABORT_IF_NULL(device);

	ssd1306_bounds_t target = device->bounds;

	if( !ssd1306_bounds_intersect(&target, bounds) ) {
		LOG_BOUNDS_D("non visible target", &target);

		return;
	}
	if( !ssd1306_acquire(device) ) {
		LOG_W("cannot acquire device");

		return;
	}

	ssd1306_bitmap_t* bitmap = ssd1306_create_bitmap((ssd1306_size_t){
		ssd1306_bounds_width(&target),
		ssd1306_bounds_height(&target)});

	esp_fill_random(bitmap->image, bitmap->w * bytes_cap(bitmap->h));

	ssd1306_draw_internal(device, &target, &target, bitmap);

	free(bitmap);

	ssd1306_update(device, &target);
	ssd1306_release(device);
}
