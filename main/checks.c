#include "checks.h"

#define VERIFY_EQ(expected, actual) \
	ABORT_IF((expected) != (actual), "expected %d, actual %d", (expected), (actual))

void verify_bounds(const char* file, int line, int16_t x0, int16_t y0, int16_t x1, int16_t y1, const ssd1306_bounds_t* source)
{
	LOG_I("test from %s:%d", file, line);

	LOG_BOUNDS_I("expecting ", (&(ssd1306_bounds_t){ x0: x0, y0: y0, x1: x1, y1: y1 }));

	ABORT_IF_NULL(source);

	LOG_BOUNDS_I("actual ", source);

	VERIFY_EQ(x0, source->x0);
	VERIFY_EQ(y0, source->y0);
	VERIFY_EQ(x1, source->x1);
	VERIFY_EQ(y1, source->y1);
}
