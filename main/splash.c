#include "sdkconfig.h"
#include "ssd1306.h"
#include "ssd1306-priv.h"

#include <math.h>

#include <esp_random.h>
#include <esp_check.h>

#define TAG                 "SSD1306"
#define SCREEN_SPLASH_FPS   25
#define SCREEN_SPLASH_TICKS pdMS_TO_TICKS(1000/SCREEN_SPLASH_FPS)

const ssd1306_bitmap_t splash_bmp = {
	width: 120,
	height: 24,

	image: {
		// https://javl.github.io/image2cpp/
		0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x80, 0x80, 
		0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0x80, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0xf8, 0xfc, 0xfc, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0xbc, 0xbc, 
		0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 
		0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		
		0x00, 0x00, 0x40, 0xfc, 0xfe, 0xff, 0x07, 0x03, 0x03, 0x01, 0x01, 0x03, 0x07, 0xff, 0xff, 0xff,
		0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x3f, 0x7b, 0x71, 0xe3, 0xe7, 0xc3, 0x00, 0x00, 0x00,
		0x00, 0x00, 0xfc, 0xfe, 0x3f, 0x07, 0x03, 0x01, 0x01, 0x01, 0x01, 0x03, 0x07, 0x07, 0x02, 0x00,
		0x00, 0x00, 0xf0, 0xfc, 0xff, 0x77, 0x73, 0x73, 0x73, 0x73, 0x73, 0x77, 0x7f, 0xff, 0x7e, 0x78,
		0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
		0xff, 0x00, 0x00, 0x00, 0x00, 0xf8, 0xfc, 0xfe, 0x1f, 0x07, 0x07, 0x03, 0x03, 0x03, 0x03, 0x03,
		0x07, 0xfe, 0xfc, 0xf0, 0x00, 0x00, 0x00, 0x00,	0xff, 0xff, 0xff, 0x7f, 0x0f, 0x07, 0x03, 0x03,
		0x03, 0x1f, 0xff, 0xfc, 0xf0, 0x00, 0x00, 0x00, 

		0x00, 0x00, 0x00, 0x01, 0x07, 0x0f, 0x0e, 0x1e, 0x1c, 0x1c, 0x1c, 0x1c, 0x0e, 0x0f, 0x3f, 0x1f, 
		0x1f, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0e, 0x1e, 0x1c, 0x1c, 0x1c, 0x1f, 0x0f, 0x07, 0x00, 0x00, 
		0x00, 0x00, 0x03, 0x07, 0x0f, 0x1c, 0x1c, 0x18, 0x18, 0x38, 0x3c, 0x1c, 0x1e, 0x0e, 0x08, 0x00, 
		0x00, 0x00, 0x01, 0x07, 0x0f, 0x0e, 0x1c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1c, 0x0f, 0x07, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x1c, 0x3f, 0x3f, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x1f, 0x1f, 
		0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x0f, 0x1f, 0x1c, 0x1c, 0x18, 0x18, 0x18, 0x18, 0x1c, 
		0x1f, 0x0f, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x3f, 0x3f, 0x3f, 0x00, 0x00, 0x00
	},
};

const ssd1306_bitmap_t bitmap_16x24 = {
	width: 16,
	height: 24,

	image: {
		0x00, 0x00, 0x00, 0x06, 0x03, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x81, 0x02, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x18, 0x99, 0x00, 0x00, 0x00, 0xc0, 0x30, 0x0c, 0xf3, 0x01, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x60, 0xc0, 0x30, 0x0c, 0x03, 0x00, 0xc0, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00,
	}
};

static TickType_t splash_ticks;

static void bitmap_each_page_b(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bitmap_t* bitmap)
{
	const TickType_t waitTicks = pdMS_TO_TICKS(250);

	ssd1306_bounds_t trimmed = *bounds;

	if( !ssd1306_trim(device, &trimmed, &bitmap->size) ) {
		return;
	}

	const uint8_t* image = bitmap->image;

	LOG_D("Bitmap %p", image);

	if( bounds->x < 0 ) {
		image += -bounds->x;
	}

	int y_bits = 0;
	
	if( bounds->y < 0 ) {
		y_bits = bounds->y & 7;

		image += ((-bounds->y-1) / 8) * bitmap->width;

		LOG_D("Image shifted by %d for y_bits %d", image - bitmap->image, y_bits);
	}

	const int t_page = bounds->y < 0 ? -1 : trimmed.y >> 3;
	const int t_bits = bounds->y < 0 ? y_bits : trimmed.y & 7;

	LOG_D("t_page = %d, t_bits = %d", t_page, t_bits);

	const int b_page = (trimmed.y + (int)trimmed.height) >> 3;
	const int b_bits = (trimmed.y + (int)trimmed.height) & 7;

	LOG_D("b_page = %d, b_bits = %d", b_page, b_bits);

	for( int page = t_page; page < b_page; image += bitmap->width ) {
		if( page >= 0 ) {
			assert(ssd1306_acquire(device));
			ssd1306_fill_page_1(device, page, trimmed.x, trimmed.width, image, -t_bits);
			ssd1306_release(device);
			ssd1306_update(device);
			vTaskDelayUntil(&splash_ticks, waitTicks);
		}

		page++;

		if( t_bits && (page < b_page) ) {
			assert(ssd1306_acquire(device));
			ssd1306_fill_page_1(device, page, trimmed.x, trimmed.width, image, 8-t_bits);
			ssd1306_release(device);
			ssd1306_update(device);
			vTaskDelayUntil(&splash_ticks, waitTicks);
		}

		if( b_bits && (page == b_page) ) {
			assert(ssd1306_acquire(device));
			ssd1306_fill_page_2(device, page, trimmed.x, trimmed.width, image, (8-t_bits) & 7, set_bits(8-b_bits));
			ssd1306_release(device);
			ssd1306_update(device);
			vTaskDelayUntil(&splash_ticks, waitTicks);
		}
	}
}

static void bitmap_each_page(ssd1306_t device, const ssd1306_bitmap_t* bitmap, int x, int y, unsigned width, unsigned height)
{
	const ssd1306_bounds_t bounds = {
		x: x, y: y, width: width, height: height,
	};

	bitmap_each_page_b(device, &bounds, bitmap);
}

static void translate_bitmap(ssd1306_t device, const ssd1306_bitmap_t* bitmap)
{
	const TickType_t waitTicks = pdMS_TO_TICKS(250);

	unsigned posX = abs((int)device->width - (int)bitmap->width) / 2;
	unsigned posY = abs((int)device->height - (int)bitmap->height) / 2;

	ssd1306_bounds_t bounds = {
		x: posX,
		y: posY,
		width: bitmap->width,
		height: bitmap->height,
	};

	bitmap_each_page_b(device, &bounds, bitmap);
	vTaskDelayUntil(&splash_ticks, pdMS_TO_TICKS(2000));

	char text[16] = "";

	const int x0 = -(int)bitmap->width - 1;
	const int x1 = device->width + bitmap->width + 1;

	ssd1306_clear_b(device, &bounds);
	ssd1306_auto_update(device, false);
	for( bounds.x = x1, bounds.y = posY; bounds.x >= x0; bounds.x-- ) {
		snprintf(text, sizeof(text), "X: %+d", bounds.x);
		ssd1306_text(device, 0, device->height - 8, device->width, 8, text);

		ssd1306_bitmap_b(device, &bounds, bitmap);
		ssd1306_auto_update(device, true);
		vTaskDelayUntil(&splash_ticks, SCREEN_SPLASH_TICKS);
		ssd1306_auto_update(device, false);
		ssd1306_clear_b(device, &bounds);
		ssd1306_clear(device, 0, device->height - 8, device->width, 8);
	}
	ssd1306_auto_update(device, true);

	const int y0 = -(int)bitmap->height - 1;
	const int y1 = device->height + bitmap->height + 1;

	ssd1306_clear_b(device, &bounds);
	ssd1306_auto_update(device, false);
	for( bounds.x = posX, bounds.y = y0; bounds.y <= y1; bounds.y++ ) {
		snprintf(text, sizeof(text), "Y: %+d", bounds.y);

		if( bounds.y < (int)device->height / 2 ) {
			ssd1306_text(device, 0, device->height - 8, device->width, 8, text);
		}
		else {
			ssd1306_text(device, 0, 0, device->width, 8, text);
		}

		ssd1306_bitmap_b(device, &bounds, bitmap);
		ssd1306_auto_update(device, true);
		vTaskDelayUntil(&splash_ticks, waitTicks);
		ssd1306_auto_update(device, false);
		ssd1306_clear_b(device, &bounds);

		if( bounds.y < (int)device->height / 2 ) {
			ssd1306_clear(device, 0, device->height - 8, device->width, 8);
		}
		else {
			ssd1306_clear(device, 0, 0, device->width, 8);
		}
	}

	ssd1306_auto_update(device, true);
}

static void fill_with_random(ssd1306_t device)
{
	ssd1306_acquire(device);
	for( unsigned page = 0; page < device->pages; page++ ) {
		esp_fill_random(raster_page(device, page), device->width);
	}
	ssd1306_release(device);
	ssd1306_update(device);
}

static void fill_with_sequence(ssd1306_t device)
{
	uint8_t value = 0;

	ssd1306_acquire(device);
	for( unsigned page = 0; page < device->pages; page++ ) {
		for( unsigned col = 0; col < device->width; col++ ) {
			raster_page(device, page)[col] = value++;
		}
	}
	ssd1306_release(device);
	ssd1306_update(device);
}

static void fill_with_bitmap(ssd1306_t device, const ssd1306_bitmap_t* bitmap)
{
	uint8_t value = 0;

	ssd1306_acquire(device);
	for( unsigned page = 0; page < bitmap->height / 8; page++ ) {
		memcpy(raster_page(device, page), bitmap->image + page * bitmap->width, bitmap->width);
	}
	ssd1306_release(device);
	ssd1306_update(device);
}

static void fill_with_white(ssd1306_t device)
{
	uint8_t value = 0;

	ssd1306_acquire(device);
	for( unsigned page = 0; page < device->pages; page++ ) {
		memset(raster_page(device, page), 0xFF, device->width);
	}
	ssd1306_release(device);
	ssd1306_update(device);
}

static void expand_rectangle(ssd1306_t device)
{
	const unsigned dim = abs((int)device->width - (int)device->height) / 2 - 1;

	ssd1306_bounds_t bounds = {
		x: dim, y: dim, 
		width: device->width - 2*dim,
		height: device->height - 2*dim,
	};

	while( bounds.x >= 0 && bounds.y >= 0 ) {
		ssd1306_clear_b(device, &bounds);

		vTaskDelayUntil(&splash_ticks, SCREEN_SPLASH_TICKS);

		bounds.x--;
		bounds.y--;
		bounds.width += 2;
		bounds.height += 2;
	}
}

void ssd1306_show_splash(ssd1306_t device)
{
	splash_ticks = xTaskGetTickCount();

	// ssd1306_clear_b(device, NULL);

	// fill_with_random(device);
	// vTaskDelayUntil(&splash_ticks, pdMS_TO_TICKS(1000));
	// expand_rectangle(device);
	// vTaskDelayUntil(&splash_ticks, pdMS_TO_TICKS(1000));
	// fill_with_sequence(device);
	// vTaskDelayUntil(&splash_ticks, pdMS_TO_TICKS(1000));
	// expand_rectangle(device);

	ssd1306_clear_b(device, NULL);
	translate_bitmap(device, &bitmap_16x24);
	ssd1306_clear_b(device, NULL);
	translate_bitmap(device, &splash_bmp);
	vTaskDelayUntil(&splash_ticks, pdMS_TO_TICKS(1000));

	// LOG_D("try manually");
	// assert(ssd1306_acquire(device));
	// const int b0 = -7;
	// const int b1 = +1;
	// ssd1306_fill_page_2(device, 0, x, width, bitmap_16x24.image + 0*bitmap_16x24.width, set_bits(b1), b1);
	// ssd1306_fill_page_2(device, 0, x, width, bitmap_16x24.image + 1*bitmap_16x24.width, set_bits(b0), b0);
	// ssd1306_fill_page_2(device, 1, x, width, bitmap_16x24.image + 1*bitmap_16x24.width, set_bits(b1), b1);
	// ssd1306_fill_page_2(device, 1, x, width, bitmap_16x24.image + 2*bitmap_16x24.width, set_bits(b0), b0);
	// ssd1306_fill_page_2(device, 2, x, width, bitmap_16x24.image + 2*bitmap_16x24.width, set_bits(b1), b1);
	// ssd1306_release(device);
	// ssd1306_update(device);

	// bitmap_each_page(device, &bitmap_16x24, x, y - 7, width, 24); x += inc;

	// bitmap_each_page(device, &bitmap_16x24, x, y + 8, width, 24); x += inc;
	// bitmap_each_page(device, &bitmap_16x24, x, y - 8, width, 24); x += inc;
	// bitmap_each_page(device, &bitmap_16x24, x, y-3, width, 24); x += inc;
	// bitmap_each_page(device, &bitmap_16x24, x, y-5, width, 24); x += inc;
	// bitmap_each_page(device, &bitmap_16x24, x, y-7, width, 24); x += inc;
	// bitmap_each_page(device, &bitmap_16x24, x, y-9, width, 24); x += inc;

	// x = 0;
	// y = 40;

	// bitmap_each_page(device, &bitmap_16x24, x, y + 3, width, 24); x += inc;
	// bitmap_each_page(device, &bitmap_16x24, x, y + 5, width, 23); x += inc;
	// bitmap_each_page(device, &bitmap_16x24, x, y + 7, width, 22); x += inc;
	// bitmap_each_page(device, &bitmap_16x24, x, y + 9, width, 21); x += inc;

	// bitmap_each_page(device, &splash_bmp, 1, 35, 22, 24); x += inc;
	// bitmap_each_page(device, &splash_bmp, 26, 35, 22, 21); x += inc;
	// vTaskDelayUntil(&splash_ticks, pdMS_TO_TICKS(1000));

	// fill_with_white(device);
	// ssd1306_clear_b(device, NULL);
	// fill_with_sample(device);
	// unsigned width = bitmap_16x24.width;
	// unsigned height = bitmap_16x24.height;
	// unsigned x = width + 1;
	// bitmap_each_page(device, &bitmap_16x24, x, y, width, height-1); x += inc;
	// bitmap_each_page(device, &bitmap_16x24, x, y, width, height-3); x += inc;
	// bitmap_each_page(device, &bitmap_16x24, x, y, width, height-8); x += inc;
	// bitmap_each_page(device, &bitmap_16x24, x, y, width, height-10); x += inc;
	// bitmap_each_page(device, &bitmap_16x24[0], x, y + 3, width, height); x += inc;
	// bitmap_each_page(device, &bitmap_16x24[0], x, y + 3, width, height-1); x += inc;
	// bitmap_each_page(device, &bitmap_16x24[0], x, y + 3, width, height-2); x += inc;
	// bitmap_each_page(device, &bitmap_16x24[0], x, y + 3, width, height-3); x += inc;
	// bitmap_each_page(device, &bitmap_16x24[0], x, y + 1, width, height - 1); x += inc;
	// bitmap_each_page(device, &bitmap_16x24[0], x, y + 2, width, height - 3); x += inc;

	// ssd1306_clear_b(device, NULL);
	// translate_bitmap(device, &splash_bmp);
	// vTaskDelayUntil(&splash_ticks, pdMS_TO_TICKS(1000));

	// fill_with_sample(device);
	// vTaskDelayUntil(&splash_ticks, pdMS_TO_TICKS(1000));

	// fill_with_white(device);
	// unsigned width = _countof(bitmap_16x24[0]) + 1;
	// unsigned x = -width;
	// bitmap_each_page(device, x += width, y, 24);
	// bitmap_each_page(device, x += width, y, 23);
	// bitmap_each_page(device, x += width, y, 22);
	// bitmap_each_page(device, x += width, 1, 23);
	// bitmap_each_page(device, x += width, -3, 24);
	// bitmap_each_page(device, x += width, -3, 23);
	// bitmap_each_page(device, x += width, -11, 24);
	// bitmap_each_page(device, x += 8, -11, 23);
	// bitmap_each_page(device, 64, 1, 23);
	// bitmap_each_page(device, 1, 5, 23);
	// bitmap_each_page(device, -4,  24, 24);
	// vTaskDelayUntil(&splash_ticks, pdMS_TO_TICKS(2000));

	// fill_with_random(device);

	// ssd1306_bitmap(device, splash_bmp, 8, 1, 128, 23);

	// vTaskDelayUntil(&splash_ticks, pdMS_TO_TICKS(1000));

	// unsigned clear_min = (device->width < device->height ? device->width : device->height) / 2;

	// for( int x = clear_min, y = clear_min; x >= 0 && y >= 0; x--, y-- ) {
	// 	ssd1306_clear(device, x, y, device->width-2*x, device->height-2*y);

	// 	vTaskDelayUntil(&splash_ticks, SCREEN_SPLASH_TICKS);
	// }

	// const unsigned slice = device->width / 4;
	// uint8_t bitmap[2 * 4 * slice];

	// memset(bitmap + 0*slice, 0xFF, slice);
	// memset(bitmap + 1*slice, 0xCC, slice);
	// memset(bitmap + 2*slice, 0xAA, slice);
	// memset(bitmap + 3*slice, 0x55, slice);

	// memset(bitmap + 4*slice, 0xFF, slice);
	// memset(bitmap + 5*slice, 0xCC, slice);
	// memset(bitmap + 6*slice, 0xAA, slice);
	// memset(bitmap + 7*slice, 0x55, slice);

	// flying_bitmap(device, splash_bmp, 128, 24);

	// ssd1306_bounds_t bounds = {
	// 	x: 0, y: 0, width: 128, height: 20,
	// };

	// ssd1306_clear_b(device, &bounds);
	// vTaskDelayUntil(&splash_ticks, pdMS_TO_TICKS(2000));
	// ssd1306_bitmap_b(device, splash_bmp, &bounds);

	// LOG_I("sizeof splash %u", sizeof(splash_bmp));

	// ssd1306_acquire(device);
	// memcpy(raster_page(device, 5), splash_bmp, sizeof(splash_bmp) - 128);
	// ssd1306_release(device);
	// ssd1306_update(device);

	// bounds.y = 0;

	// // ssd1306_clear_b(device, &bounds);
	// vTaskDelayUntil(&splash_ticks, pdMS_TO_TICKS(2000));
	// ssd1306_bitmap_b(device, splash_bmp, &bounds);
}
