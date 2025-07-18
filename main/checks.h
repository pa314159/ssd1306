#include <ssd1306.h>
#include <ssd1306-int.h>
#include <ssd1306-log.h>
#include <ssd1306-misc.h>

extern void verify_bounds(const char* file, int line, 
	int16_t x0, int16_t y0, int16_t x1, int16_t y1, 
	const ssd1306_bounds_t* source);

#define VERIFY_BOUNDS(x0, y0, x1, y1, source) \
	verify_bounds(__FILE__, __LINE__, (x0), (y0), (x1), (y1), (source))

#define VERIFY_NOT_NULL(source) \
	ABORT_IF_NULL(source)

extern void test_geom();
