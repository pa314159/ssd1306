#include "ssd1306.h"
#include "ssd1306-int.h"

#include "os.h"

// bool ssd1306_bounds_check(const ssd1306_bounds_t* bounds)
// {
// 	ABORT_IF_NULL(bounds);
// 	ABORT_IF(bounds->x0 > bounds->x1, "bounds corrupted, x0(%+d) > x1(%+d)", bounds->x0, bounds->x1);
// 	ABORT_IF(bounds->y0 > bounds->y1, "bounds corrupted, y0(%+d) > y1(%+d)", bounds->y0, bounds->y1);

// 	int val = 0;
	
// 	val += abs(bounds->x0);
// 	val += abs(bounds->y0);
// 	val += abs(bounds->x1);
// 	val += abs(bounds->x1);

// 	return val == 0;
// }

ssd1306_bounds_t* ssd1306_bounds_union(ssd1306_bounds_t* target, const ssd1306_bounds_t* source)
{
	ABORT_IF_NULL(target);

	if( source == NULL ) {
		return target;
	}

	if( source->x0 < target->x0 ) {
		target->x0 = source->x0;
	}
	if( source->y0 < target->y0 ) {
		target->y0 = source->y0;
	}
	if( source->x1 > target->x1 ) {
		target->x1 = source->x1;
	}
	if( source->y1 > target->y1 ) {
		target->y1 = source->y1;
	}

	return target;
}

ssd1306_bounds_t* ssd1306_bounds_intersect(ssd1306_bounds_t* target, const ssd1306_bounds_t* source)
{
	ABORT_IF_NULL(target);

	if( source == NULL ) {
		return target;
	}

	if( source->x0 > target->x0 ) {
		target->x0 = source->x0;
	}
	if( source->y0 > target->y0 ) {
		target->y0 = source->y0;
	}
	if( source->x1 < target->x1 ) {
		target->x1 = source->x1;
	}
	if( source->y1 < target->y1 ) {
		target->y1 = source->y1;
	}

	return target->x0 < target->x1 && target->y0 < target->y1 ? target : NULL;
}

uint16_t ssd1306_bounds_width(const ssd1306_bounds_t* bounds)
{
	ABORT_IF_NULL(bounds);
	ABORT_IF(bounds->x1 < bounds->x0, "invalid coordinates x0 = %d, x1 = %d", bounds->x0, bounds->x1);

	return bounds->x1 - bounds->x0;
}

uint16_t ssd1306_bounds_height(const ssd1306_bounds_t* bounds)
{
	ABORT_IF_NULL(bounds);
	ABORT_IF(bounds->y1 < bounds->y0, "invalid coordinates y0 = %d, y1 = %d", bounds->y0, bounds->y1);

	return bounds->y1 - bounds->y0;
}

ssd1306_bounds_t* ssd1306_bounds_resize(ssd1306_bounds_t* target, const ssd1306_size_t size)
{
	ABORT_IF_NULL(target);

	target->x1 = target->x0 + size.w;
	target->y1 = target->y0 + size.h;

	return target;
}

ssd1306_bounds_t* ssd1306_bounds_move_to(ssd1306_bounds_t* target, const ssd1306_point_t origin)
{
	ABORT_IF_NULL(target);

	const uint16_t w = ssd1306_bounds_width(target);
	const uint16_t h = ssd1306_bounds_height(target);

	target->x0 = origin.x;
	target->y0 = origin.y;
	target->x1 = origin.x + w;
	target->y1 = origin.y + h;

	return target;
}

ssd1306_bounds_t* ssd1306_bounds_move_by(ssd1306_bounds_t* target, const ssd1306_point_t offset)
{
	ABORT_IF_NULL(target);

	target->x0 += offset.x;
	target->y0 += offset.y;
	target->x1 += offset.x;
	target->y1 += offset.y;

	return target;
}

ssd1306_point_t ssd1306_bounds_center(const ssd1306_bounds_t* bounds)
{
	ABORT_IF_NULL(bounds);

	return (ssd1306_point_t){ (bounds->x0 + bounds->x1) / 2, (bounds->y0 + bounds->y1) / 2 };
}
