#include "ssd1306.h"
#include "ssd1306-int.h"

bool ssd1306_bounds_check(const ssd1306_bounds_t* bounds)
{
	ABORT_IF_NULL(bounds);
	ABORT_IF(bounds->x0 > bounds->x1, "bounds corrupted, x0(%+d) > x1(%+d)", bounds->x0, bounds->x1);
	ABORT_IF(bounds->y0 > bounds->y1, "bounds corrupted, y0(%+d) > y1(%+d)", bounds->y0, bounds->y1);

	int val = 0;
	
	val += abs(bounds->x0);
	val += abs(bounds->y0);
	val += abs(bounds->x1);
	val += abs(bounds->x1);

	return val == 0;
}

void ssd1306_bounds_union(ssd1306_bounds_t* target, const ssd1306_bounds_t* source)
{
	ABORT_IF_NULL(target);
	ABORT_IF_NULL(source);

	if( source->x0 < target->x0 ) {
		target->x0 = source->x0;
	}
	if( source->y0 < target->y0 ) {
		target->y0 = source->y0;
	}
	if( source->x1 > target->x1 ) {
		target->x1 = source->x1;
	}
	if( source->y1 < target->y1 ) {
		target->y1 = source->y1;
	}
}

bool ssd1306_bounds_intersect(ssd1306_bounds_t* target, const ssd1306_bounds_t* source)
{
	ABORT_IF_NULL(target);
	ABORT_IF_NULL(source);

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

	return target->x0 < target->x1 && target->y0 < target->y1;
}
