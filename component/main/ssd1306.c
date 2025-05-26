#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-defs.h"
#include "ssd1306-int.h"

#include <assert.h>
#include <ctype.h>

static void ssd1306_init_private(ssd1306_int_t dev, const ssd1306_init_t init, uint8_t pages);
static void ssd1306_init_status(ssd1306_int_t dev, ssd1306_status_t status);
static void ssd1306_init_screen(ssd1306_int_t dev, const ssd1306_init_t ini);
static void ssd1306_clear_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed);
static void ssd1306_mask_page(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, uint8_t mask);
static void ssd1306_draw_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed, const ssd1306_bitmap_t* bitmap);
static void ssd1306_set_page_1(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, const uint8_t* data, int8_t bits);
static void ssd1306_set_page_2(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, const uint8_t* data, int8_t bits, uint8_t d_mask);
static void ssd1306_grab_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed, ssd1306_bitmap_t* bitmap);
static void ssd1306_get_page_1(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, uint8_t* data, int8_t bits);
static void ssd1306_get_page_2(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, uint8_t* data, int8_t bits, uint8_t d_mask);
static bool ssd1306_trim(ssd1306_t device, ssd1306_bounds_t* bounds, const ssd1306_size_t* size);

static const char TEXT_SEPA[] = " \x4 ";
static const unsigned TEXT_SEPA_Z = 3;

static char* ssd1306_text_formatv(uint16_t* length, const char* format, va_list args);
static ssd1306_bitmap_t* ssd1306_text_bitmapv(ssd1306_t device, const char* format, va_list args);

static void ssd1306_text_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const char* format, va_list args);

ssd1306_init_t ssd1306_create_init(ssd1306_interface_t type)
{
	if( type == ssd1306_interface_any ) {
		type = CONFIG_SSD1306_INTERFACE;	
	}

	switch( type ) {
		case ssd1306_interface_iic: {
			return ssd1306_iic_create_init();
		}
		
		case ssd1306_interface_spi: {
			return ssd1306_spi_create_init();
		}

		default:
			ABORT_IF(false, "unreachable code");
			return NULL;
	}
}

ssd1306_t ssd1306_init(ssd1306_init_t init)
{
	ssd1306_log_set_level(CONFIG_SSD1306_LOGGING_LEVEL);

	LOG_E("LOG_E is active");
	LOG_W("LOG_W is active");
	LOG_I("LOG_I is active");
	LOG_D("LOG_D is active");
	LOG_V("LOG_V is active");

	if( init == NULL ) {
		init = ssd1306_create_init(ssd1306_interface_any);
	}

	// allocate additional bytes for internal buffer and raster
	const uint8_t pages = bytes_cap(init->h);
	const size_t total = sizeof(ssd1306_int_s) + pages*init->w;

	ssd1306_int_t dev = calloc(1, total);

	ABORT_IF(dev == NULL, "cannot allocate memory for ssd1306_t");

	ssd1306_init_private(dev, init, pages);
	ssd1306_init_status(dev, ssd1306_status_0);
	ssd1306_init_status(dev, ssd1306_status_1);

	LOG_I("Allocated %u bytes at %p", total, dev);

	ssd1306_dump(dev, sizeof(ssd1306_int_s), "Initialisation structure");

	LOG_I("Initialising device");
	LOG_I("    Size: %ux%u (%u pages)", dev->w, dev->h, dev->pages);
	LOG_I("    Bounds: [%+d%+d, %+d%+d]", dev->x0, dev->y0, dev->x1, dev->y1);
	LOG_I("    Flip: %s", dev->flip ? "yes" : "no");
	LOG_I("    Contrast: %d", init->contrast);
	LOG_I("    Invert: %d", init->invert);

	switch( init->connection.type ) {
		case ssd1306_interface_iic:
			LOG_I("    Type: IIC");
			dev->i2c = ssd1306_iic_init(init);
		break;
		case ssd1306_interface_spi:
			LOG_I("    Type: SPI");
			dev->spi = ssd1306_spi_init(init);
		break;

		default:
			ABORT_IF(false, "unreachable code");
	}

	LOG_I("Initialising screen");
	ssd1306_init_screen(dev, init);

	LOG_I("Initialising locks");

	dev->mutex = xSemaphoreCreateRecursiveMutex();

	configASSERT(dev->mutex);

	static unsigned tasks = 0;

	char task_name[] = "ssd1306-\0";

	task_name[strlen(task_name)] = 'A' + tasks++;

	LOG_I("Creating task %s", task_name);

#if CONFIG_SSD1306_OPTIMIZE
	dev->queue = xQueueCreate(1, sizeof(ssd1306_bounds_t));

	configASSERT(dev->queue);

	xTaskCreate((TaskFunction_t)ssd1306_task, task_name, 1024 * CONFIG_SSD1306_STACK_SIZE, dev, CONFIG_SSD1306_PRIORITY, NULL);
#else
	TaskHandle_t task;
	xTaskCreate((TaskFunction_t)ssd1306_task, task_name, 1024 * CONFIG_SSD1306_STACK_SIZE, dev, CONFIG_SSD1306_PRIORITY, &task);

	dev->task = task;
#endif

	while( !dev->active ) {
		vTaskDelay(SSD1306_SEM_TICKS);
	}

	if( init->free ) {
		free(init);
	}

#if CONFIG_SSD1306_OPTIMIZE || !CONFIG_SSD1306_SPLASH
	ssd1306_update((ssd1306_t)dev, NULL);
#endif

#if CONFIG_SSD1306_SPLASH > 0
	ssd1306_draw((ssd1306_t)dev, (dev->w - splash_bmp->w) / 2, (dev->h - splash_bmp->h) / 2, splash_bmp->w, splash_bmp->h, splash_bmp);
	vTaskDelay(pdMS_TO_TICKS(CONFIG_SSD1306_SPLASH));
#endif

	return (ssd1306_t)dev;
}

#if __SSD1306_FREE
void ssd1306_free(ssd1306_t device)
{
	if( device == NULL ) {
		return;
	}

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	switch( dev->connection.type ) {
		case ssd1306_interface_i2c:
			ssd1306_iic_free(dev->i2c);
		break;
		case ssd1306_interface_spi:
			ssd1306_spi_free(dev->spi);
		break;
	}

	free(dev->data)
	free(dev);
}
#endif

bool ssd1306_acquire(ssd1306_t device)
{
	ABORT_IF_NULL(device);

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	return xSemaphoreTakeRecursive(dev->mutex, SSD1306_SEM_TICKS);
}

void ssd1306_release(ssd1306_t device)
{
	ABORT_IF_NULL(device);

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	xSemaphoreGiveRecursive(dev->mutex);
}

void ssd1306_update(ssd1306_t device, const ssd1306_bounds_t* bounds)
{
	ABORT_IF_NULL(device);

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	ABORT_IF(dev->defer_update < 0, "unbalanced auto update value (%+d)", dev->defer_update);
#if CONFIG_SSD1306_OPTIMIZE
	if( dev->defer_update ) {
		ABORT_IF(bounds == NULL, "defer_update is %u, bounds cannot be NULL");

		if( dev->dirty_bounds ) {
			ssd1306_bounds_union(dev->dirty_bounds, bounds);
		} else {
			dev->dirty_bounds = malloc(sizeof(ssd1306_bounds_t));

			*dev->dirty_bounds = *bounds;
		}

		LOG_D("dirty bounds [%+d%+d, %+d%+d]", 
			dev->dirty_bounds->x0, dev->dirty_bounds->y0,
			dev->dirty_bounds->x1, dev->dirty_bounds->y1);
	} else {
#else
	if( dev->defer_update == 0 ) {
#endif
#if CONFIG_SSD1306_OPTIMIZE
		if( bounds ) {
			LOG_D("enqueue bounds [%+d%+d, %+d%+d]",
				bounds->x0, bounds->y0, bounds->x1, bounds->y1);

			xQueueSend(dev->queue, bounds, portMAX_DELAY);
		} else if( dev->dirty_bounds ) {
			LOG_D("enqueue dirty bounds [%+d%+d, %+d%+d]",
				dev->dirty_bounds->x0, dev->dirty_bounds->y0,
				dev->dirty_bounds->x1, dev->dirty_bounds->y1);

			xQueueSend(dev->queue, dev->dirty_bounds, portMAX_DELAY);

			free(dev->dirty_bounds);

			dev->dirty_bounds = NULL;
		} else {
			LOG_D("enqueue device bounds [%+d%+d, %+d%+d]",
				dev->bounds.x0, dev->bounds.y0,
				dev->bounds.x1, dev->bounds.y1);

			xQueueSend(dev->queue, &dev->bounds, portMAX_DELAY);
		}
#else
		xTaskNotifyGive(dev->task);
#endif
	}
}

void ssd1306_auto_update(ssd1306_t device, bool on)
{
	ABORT_IF_NULL(device);

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	dev->defer_update += on ? -1 : +1;

	if( dev->defer_update == 0 ) {
		ssd1306_update(device, NULL);
	}
}

uint8_t* ssd1306_raster(ssd1306_t device, uint8_t page)
{
	ABORT_IF_NULL(device);

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	return dev->buff + page * device->w;
}

void ssd1306_on(ssd1306_t device, bool on)
{
	ABORT_IF_NULL(device);

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	const uint8_t data = OLED_CMD1(DISPLAY_OFF, on);

	if( ssd1306_acquire(device) ) {
		ssd1306_send_buff(dev, OLED_CTL_COMMAND, &data, 1);

		ssd1306_release(device);
	} else {
		LOG_W("Couldn't take mutex");
	}
}

void ssd1306_invert(ssd1306_t device, bool on)
{
	ABORT_IF_NULL(device);

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	const uint8_t data = OLED_CMD1(DISPLAY_NORMAL, on);

	if( ssd1306_acquire(device) ) {
		ssd1306_send_buff(dev, OLED_CTL_COMMAND, &data, 1);

		ssd1306_release(device);
	} else {
		LOG_W("Couldn't take mutex");
	}
}

void ssd1306_contrast(ssd1306_t device, uint8_t contrast)
{
	ABORT_IF_NULL(device);

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	const uint8_t data[] = { OLED_CMD2(SET_CONTRAST, contrast) };

	if( ssd1306_acquire(device) ) {
		ssd1306_send_buff(dev, OLED_CTL_COMMAND, data, _countof(data));

		ssd1306_release(device);
	} else {
		LOG_W("Couldn't take mutex");
	}
}

void ssd1306_clear_b(ssd1306_t device, const ssd1306_bounds_t* bounds)
{
	ABORT_IF_NULL(device);

	if( bounds == NULL ) {
		ssd1306_clear_b(device, &device->bounds);

		return;
	}

	ssd1306_bounds_t trimmed = *bounds;
	
	if( !ssd1306_trim(device, &trimmed, NULL) ) {
		return;
	}
	if( !ssd1306_acquire(device) ) {
		LOG_W("Couldn't take mutex");

		return;
	}

	ssd1306_clear_internal(device, bounds, &trimmed);

	ssd1306_update(device, &trimmed);
	ssd1306_release(device);
}

void ssd1306_clear_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed)
{
	const uint16_t trimmed_w = ssd1306_bounds_width(trimmed);

	const uint8_t t_page = trimmed->y0 / 8;
	const uint8_t t_bits = trimmed->y0 % 8;

	const uint8_t b_page = trimmed->y1 / 8;
	const uint8_t b_bits = trimmed->y1 % 8;

	const uint8_t t_mask = set_bits(-t_bits);
	const uint8_t b_mask = set_bits(8 - b_bits);

	LOG_D("t_page = %u, t_bits = %u, t_mask = 0x%02x", t_page, t_bits, t_mask);
	LOG_D("b_page = %u, b_bits = %u, b_mask = 0x%02x", b_page, b_bits, b_mask);

	uint8_t page = t_page;

	if( t_bits ) {
		ssd1306_mask_page(device, page++, trimmed->x0, trimmed_w, t_mask);
	}

	while( page < b_page ) {
		ssd1306_mask_page(device, page++, trimmed->x0, trimmed_w, 0);
	}

	if( b_bits ) {
		ssd1306_mask_page(device, page++, trimmed->x0, trimmed_w, b_mask);
	}
}

void ssd1306_mask_page(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, uint8_t mask)
{
	uint8_t* buff = ssd1306_raster(device, page) + offset;

	if( mask == 0 ) {
		LOG_V("clear page %d", page);

		memset(buff, 0, width);
	} else {
		LOG_V("clear page %d with mask 0x%02x", page, mask);

		for( unsigned x = 0; x < width; x++ ) {
			buff[x] &= mask;
		}
	}
}

void ssd1306_draw_b(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bitmap_t* bitmap)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(bitmap);

	if( bounds == NULL ) {
		ssd1306_draw(device, 
			((int)device->w - (int)bitmap->w) / 2,
			((int)device->h - (int)bitmap->h) / 2,
			bitmap->w, bitmap->h, bitmap);

		return;
	}

	ssd1306_bounds_t trimmed = *bounds;

	if( !ssd1306_trim(device, &trimmed, &bitmap->size) ) {
		return;
	}
	if( !ssd1306_acquire(device) ) {
		LOG_W("Couldn't take mutex");

		return;
	}

	ssd1306_draw_internal(device, bounds, &trimmed, bitmap);

	ssd1306_update(device, &trimmed);
	ssd1306_release(device);
}

void ssd1306_draw_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed, const ssd1306_bitmap_t* bitmap)
{
	const uint8_t* image = bitmap->image;

	if( bounds->x0 < 0 ) {
		image += -bounds->x0;
	}

	int16_t y_bits = 0;

	if( bounds->y0 < 0 ) {
		y_bits = bounds->y0 & 7;

		image += ((-bounds->y0-1) / 8) * bitmap->w;

		LOG_D("Image shifted by %d for y_bits %d", image - bitmap->image, y_bits);
	}

	const uint16_t trimmed_w = ssd1306_bounds_width(trimmed);

	const int16_t t_page = bounds->y0 < 0 ? -1 : trimmed->y0 >> 3;
	const int16_t t_bits = bounds->y0 < 0 ? y_bits : trimmed->y0 & 7;

	const int16_t b_page = trimmed->y1 >> 3;
	const int16_t b_bits = trimmed->y1 & 7;

	LOG_D("t_page = %d, t_bits = %d", t_page, t_bits);
	LOG_D("b_page = %d, b_bits = %d", b_page, b_bits);

	for( int16_t page = t_page; page < b_page; image += bitmap->w ) {
		if( page >= 0 ) {
			ssd1306_set_page_1(device, page, trimmed->x0, trimmed_w, image, -t_bits);
		}

		page++;

		if( t_bits && (page < b_page) ) {
			ssd1306_set_page_1(device, page, trimmed->x0, trimmed_w, image, 8-t_bits);
		}
		if( b_bits && (page == b_page) ) {
			ssd1306_set_page_2(device, page, trimmed->x0, trimmed_w, image, 8-t_bits, set_bits(8-b_bits));
		}
	}
}

void ssd1306_set_page_1(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, const uint8_t* data, int8_t s_bits)
{
	ssd1306_set_page_2(device, page, offset, width, data, s_bits, set_bits(s_bits));
}

void ssd1306_set_page_2(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, const uint8_t* data, int8_t s_bits, uint8_t d_mask)
{
	uint8_t* buff = ssd1306_raster(device, page) + offset;

	if( d_mask == 0xff && s_bits == 0 ) {
		LOG_V("set page %d from %p", page, data);

		memcpy(buff, data, width);
	} else {
		const uint8_t s_mask = ~d_mask;

		LOG_V("set page %d with d_mask 0x%02x, s_bits = %+d, s_mask = 0x%02x, from %p", page, d_mask, s_bits, s_mask, data);

		ssd1306_dump(data, width, "Source buff");

		for( unsigned x = 0; x < width; x++ ) {
			buff[x] = (buff[x] & d_mask) | (shift_bits(data[x], s_bits) & s_mask);
		}

		ssd1306_dump(buff, width, "Raster buff");
	}
}

void ssd1306_grab_b(ssd1306_t device, const ssd1306_bounds_t* bounds, ssd1306_bitmap_t* bitmap)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(bitmap);

	if( bounds == NULL ) {
		ssd1306_grab(device, 
			((int)device->w - (int)bitmap->w) / 2,
			((int)device->h - (int)bitmap->h) / 2,
			bitmap->w, bitmap->h, bitmap);

		return;
	}

	ssd1306_bounds_t trimmed = *bounds;

	if( !ssd1306_trim(device, &trimmed, &bitmap->size) ) {
		return;
	}
	if( !ssd1306_acquire(device) ) {
		LOG_W("Couldn't take mutex");

		return;
	}

	ssd1306_grab_internal(device, bounds, &trimmed, bitmap);

	ssd1306_update(device, &trimmed);
	ssd1306_release(device);
}

void ssd1306_grab_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed, ssd1306_bitmap_t* bitmap)
{
	uint8_t* image = bitmap->image;

	if( bounds->x0 < 0 ) {
		image += -bounds->x0;
	}

	int16_t y_bits = 0;

	if( bounds->y0 < 0 ) {
		y_bits = bounds->y0 & 7;

		image += ((-bounds->y0-1) / 8) * bitmap->w;

		LOG_D("Image shifted by %d for y_bits %d", image - bitmap->image, y_bits);
	}

	const uint16_t trimmed_w = ssd1306_bounds_width(trimmed);

	const int16_t t_page = bounds->y0 < 0 ? -1 : trimmed->y0 >> 3;
	const int16_t t_bits = bounds->y0 < 0 ? y_bits : trimmed->y0 & 7;

	const int16_t b_page = trimmed->y1 >> 3;
	const int16_t b_bits = trimmed->y1 & 7;

	LOG_D("t_page = %d, t_bits = %d", t_page, t_bits);
	LOG_D("b_page = %d, b_bits = %d", b_page, b_bits);

	for( int16_t page = t_page; page < b_page; image += bitmap->w ) {
		if( page >= 0 ) {
			ssd1306_get_page_1(device, page, trimmed->x0, trimmed_w, image, t_bits);
		}

		page++;

		if( t_bits && (page < b_page) ) {
			ssd1306_get_page_1(device, page, trimmed->x0, trimmed_w, image, t_bits - 8);
		}
		if( b_bits && (page == b_page) ) {
			ssd1306_get_page_2(device, page, trimmed->x0, trimmed_w, image, t_bits - 8, set_bits(b_bits - 8));
		}
	}
}

void ssd1306_get_page_1(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, uint8_t* data, int8_t s_bits)
{
	ssd1306_get_page_2(device, page, offset, width, data, s_bits, set_bits(s_bits));
}

void ssd1306_get_page_2(ssd1306_t device, uint8_t page, int16_t offset, uint16_t width, uint8_t* data, int8_t s_bits, uint8_t d_mask)
{
	const uint8_t* buff = ssd1306_raster(device, page) + offset;

	if( d_mask == 0xff && s_bits == 0 ) {
		LOG_V("get page %d from %p", page, data);

		memcpy(data, buff, width);
	} else {
		const uint8_t s_mask = ~d_mask;

		LOG_V("get page %d with d_mask 0x%02x, s_bits = %+d, s_mask = 0x%02x, from %p", page, d_mask, s_bits, s_mask, data);

		ssd1306_dump(buff, width, "Raster buff");

		for( unsigned x = 0; x < width; x++ ) {
			data[x] = (data[x] & d_mask) | (shift_bits(buff[x], s_bits) & s_mask);
		}

		ssd1306_dump(data, width, "Target buff");
	}
}

ssd1306_bitmap_t* ssd1306_text_bitmap(ssd1306_t device, const char* format, ...)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(format);

	ssd1306_bitmap_t* bitmap;
	va_list args;

	va_start(args, format);
	bitmap = ssd1306_text_bitmapv(device, format, args);
	va_end(args);

	return bitmap;
}

ssd1306_bitmap_t* ssd1306_text_bitmapv(ssd1306_t device, const char* format, va_list args)
{
	uint16_t length;
	char* text = ssd1306_text_formatv(&length, format, args);

	uint16_t width = ssd1306_text_width(device, text);

	if( width > device->w ) {
		length += TEXT_SEPA_Z;

		strcat(text, TEXT_SEPA);

		width = ssd1306_text_width(device, text);
	}

	ssd1306_bitmap_t* bitmap = ssd1306_create_bitmap(width, SSD1306_TEXT_HEIGHT);

	bool invert = false;

	for( unsigned index = 0, offset = 0; index < length && offset < width; index++ ) {
		if( text[index] == CONFIG_SSD1306_TEXT_INVERT ) {
			invert = !invert;

			continue;
		}

		const unsigned cpos = text[index];
		const ssd1306_glyph_t* glyph = device->font + cpos;

		if( invert ) {
			for( int k = 0; k < glyph->w; k++ ) {
				bitmap->image[offset + k] = ~glyph->image[k];
			}
		} else {
			memcpy(bitmap->image + offset, glyph->image, glyph->w);
		}

		offset += glyph->w;
	}

	free(text);

	return bitmap;
}

void ssd1306_text_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const char* format, va_list args)
{
	ssd1306_bitmap_t* bitmap = ssd1306_text_bitmapv(device, format, args);
	ssd1306_bounds_t trimmed = *bounds;

	if( !ssd1306_trim(device, &trimmed, &bitmap->size) ) {
		LOG_W("not visible");

		free(bitmap);

		return;
	}
	if( !ssd1306_acquire(device) ) {
		free(bitmap);

		LOG_W("couldn't take mutex");

		return;
	}

	ssd1306_draw_internal(device, bounds, &trimmed, bitmap);

	free(bitmap);

	ssd1306_update(device, &trimmed);
	ssd1306_release(device);
}

uint16_t ssd1306_text_width(ssd1306_t device, const char* text)
{
	ABORT_IF_NULL(device);
	ABORT_IF_NULL(text);

	uint16_t width = 0;

	for( ; *text; text++ ) {
		if( *text == CONFIG_SSD1306_TEXT_INVERT ) {
			continue;
		}

		width += device->font[(uint8_t)(*text)].width;
	}

	return width;
}

char* ssd1306_text_formatv(uint16_t* length, const char* format, va_list args)
{
	uint16_t needed = vsnprintf(NULL, 0, format, args) + 1;
	char* text = malloc(needed + TEXT_SEPA_Z);

	ABORT_IF(text == NULL, "cannot allocate memory for text of size %u", needed);

	vsnprintf(text, needed, format, args);

	LOG_D("text formatted as \"%s\"", text);

	*length = needed;

	return text;
}

void ssd1306_text_b(ssd1306_t device, const ssd1306_bounds_t* bounds, const char* format, ...)
{
	va_list args;

	va_start(args, format);
	ssd1306_text_internal(device, bounds, format, args);
	va_end(args);
}

const ssd1306_bounds_t* ssd1306_status_bounds(ssd1306_t device, ssd1306_status_t status)
{
	ssd1306_int_t dev = (ssd1306_int_t)device;
	uint16_t index = ssd1306_status_index(dev, status);

	return (ssd1306_bounds_t*)&dev->statuses[index];
}

void ssd1306_status(ssd1306_t device, ssd1306_status_t status, const char* format, ...)
{
	if( !ssd1306_acquire(device) ) {
		LOG_W("couldn't take mutex");

		return;
	}

	ssd1306_int_t dev = (ssd1306_int_t)device;
	const uint16_t index = ssd1306_status_index(dev, status);
	status_info_t* si = &dev->statuses[status];

	if( si->bitmap ) {
		free(si->bitmap);

		si->bitmap = NULL;
	}

	const ssd1306_bounds_t* bounds = (ssd1306_bounds_t*)&dev->statuses[index];

	ssd1306_clear_internal(device, bounds, bounds);

	if( format ) {
		ssd1306_bitmap_t* bitmap;
		va_list args;

		va_start(args, format);
		bitmap = ssd1306_text_bitmapv(device, format, args);
		va_end(args);

		ssd1306_bounds_t trimmed = *bounds;

		ssd1306_trim(device, &trimmed, &bitmap->size);
		ssd1306_draw_internal(device, bounds, &trimmed, bitmap);

		if( bitmap->w > device->w ) {
			si->bitmap = bitmap;
			si->state = anim_init;

			LOG_D("text will scroll in background");
		} else {
			free(bitmap);
		}
	}

	ssd1306_update(device, bounds);
	ssd1306_release(device);
}

void ssd1306_init_private(ssd1306_int_t dev, const ssd1306_init_t ini, uint8_t pages)
{
	dev->flip = ini->flip;
	dev->x1 = ini->w;
	dev->y1 = ini->h;
	dev->size = ini->size;
	dev->pages = pages;
	dev->font = ini->font;
	
	memcpy((void*)&dev->connection, &ini->connection, sizeof(dev->connection));
}

void ssd1306_init_status(ssd1306_int_t dev, ssd1306_status_t status)
{
	uint16_t index = ssd1306_status_index(dev, status);
	ssd1306_bounds_t* bounds = (ssd1306_bounds_t*)&dev->statuses[index];

	bounds->x0 = 0;
	bounds->y0 = dev->flip ? dev->h - SSD1306_TEXT_HEIGHT * (2-index) : SSD1306_TEXT_HEIGHT * index;
	bounds->x1 = dev->w;
	bounds->y1 = bounds->y0 + SSD1306_TEXT_HEIGHT;
}

void ssd1306_init_screen(ssd1306_int_t dev, const ssd1306_init_t ini)
{
	const uint8_t data[] = {
// 		OLED_CMD_DISPLAY_OFF,
// 		OLED_CMD_SET_DISPLAY_CLK_DIV, 0x80,
// 		OLED_CMD_SET_MUX_RATIO, dev->h-1,
// 		OLED_CMD_SET_DISPLAY_OFFSET, 0x00,
// 		OLED_CMD_SET_DISPLAY_START_LINE | 0x00,
// 		OLED_CMD_SET_CHARGE_PUMP, 0x14,

// #if !CONFIG_SSD1306_OPTIMIZE
// 		OLED_CMD_SET_MEMORY_ADDR_MODE, OLED_CMD_SET_HORI_ADDR_MODE,
// 		OLED_CMD_SET_COLUMN_RANGE, 0, dev->w-1,
// 		OLED_CMD_SET_PAGE_RANGE, 0, dev->pages-1,
// #endif

// 		OLED_CMD_SET_SEGMENT_REMAP | (dev->flip ? 0x00 : 0x01),
// 		OLED_CMD_SET_COM_SCAN_MODE | (dev->flip ? 0x00 : 0x08),

// 		OLED_CMD_SET_COM_PIN_MAP, dev->h == 64 ? 0x12:  0x02,
// 		OLED_CMD_SET_CONTRAST, 0x7F,
// 		OLED_CMD_SET_PRECHARGE, 0xf1,
// 		OLED_CMD_SET_VCOMH_DESELCT, 0x40,

// 		OLED_CMD_DEACTIVE_SCROLL,
// 		OLED_CMD_DISPLAY_NORMAL | (dev->invert ? 0x01 : 0x00),
// 		OLED_CMD_DISPLAY_RAM,
// 		OLED_CMD_DISPLAY_ON,

		OLED_CMD0(DISPLAY_OFF),

		OLED_CMD2(SET_MUX_RATIO, dev->h-1),
		OLED_CMD1(SET_SEGMENT_REMAP, dev->flip ? 0x00 : 0x01),
		OLED_CMD1(SET_COM_SCAN_MODE, dev->flip ? 0x00 : 0x08),
		OLED_CMD2(SET_DISPLAY_CLK_DIV, 0x80),

		OLED_CMD2(SET_COM_PIN_MAP, dev->h == 64 ? 0x12:  0x02),
		OLED_CMD2(SET_VCOMH_DESELCT, 0x40),
		OLED_CMD2(SET_CHARGE_PUMP, 0x14),
		OLED_CMD2(SET_PRECHARGE, 0xf1),

		OLED_CMD0(DISPLAY_RAM),
		OLED_CMD2(SET_DISPLAY_OFFSET, 0x00),
		OLED_CMD1(SET_DISPLAY_START_LINE, 0x00),

		OLED_CMD2(SET_MEMORY_ADDR_MODE, OLED_HORI_ADDR_MODE),

#if !CONFIG_SSD1306_OPTIMIZE
		OLED_CMD3(SET_COLUMN_RANGE, 0, dev->w-1),
		OLED_CMD3(SET_PAGE_RANGE, 0, dev->pages-1),
#endif

		OLED_CMD0(DEACTIVE_SCROLL),
		OLED_CMD2(SET_CONTRAST, ini->contrast),
		OLED_CMD1(DISPLAY_NORMAL, ini->invert ? 0x01 : 0x00),

		OLED_CMD0(DISPLAY_ON),

	};

	ssd1306_send_buff(dev, OLED_CTL_COMMAND, data, _countof(data));
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
		LOG_V("<< bounds [%+d%+d, %+d%+d]",
			bounds->x0, bounds->y0, bounds->x1, bounds->y1);

		return true;
	} else {
		LOG_V("<< bounds not visible [%+d%+d, %+d%+d]",
			bounds->x0, bounds->y0, bounds->x1, bounds->y1);

		return false;
	}
}

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
