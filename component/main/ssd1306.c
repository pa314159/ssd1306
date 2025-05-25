#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-defs.h"
#include "ssd1306-int.h"

#include <assert.h>
#include <ctype.h>

static void ssd1306_init_private(ssd1306_int_t dev, const ssd1306_init_t init, uint8_t pages);
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

static char* ssd1306_text_formatv(uint16_t* length, const char* format, va_list args);
static ssd1306_bitmap_t* ssd1306_text_bitmapv(ssd1306_t device, const char* format, va_list args);

static void ssd1306_text_internal(ssd1306_t device, const ssd1306_bounds_t* bounds, const char* format, va_list args);

ssd1306_t ssd1306_init(ssd1306_init_t init)
{
	ssd1306_log_set_level(CONFIG_SSD1306_LOGGING_LEVEL);

	LOG_E("LOG_ERROR");
	LOG_W("LOG_WARN");
	LOG_I("LOG_INFO");
	LOG_D("LOG_DEBUG");
	LOG_V("LOG_VERBOSE");

	if( init == NULL ) {
		init = ssd1306_create_init();
	}

	// allocate additional bytes for internal buffer and raster
	const uint8_t pages = bytes_cap(init->height);
	const size_t total = sizeof(ssd1306_int_s) + pages*init->width;

	ssd1306_int_t dev = calloc(1, total);

	ABORT_IF(dev == NULL, "cannot allocate memory for ssd1306_t");

	ssd1306_init_private(dev, init, pages);

	LOG_I("Allocated %u bytes at %p", total, dev);

	ssd1306_dump(dev, sizeof(ssd1306_int_s), "Initialisation structure");

	LOG_I("Initialising device");
	LOG_I("    Size: %ux%u (%u pages)", dev->width, dev->height, dev->pages);
	LOG_I("    Flip: %s", dev->flip ? "yes" : "no");
	LOG_I("    Contrast: %d", init->contrast);
	LOG_I("    Invert: %d", init->invert);

	switch( init->connection.type ) {
		case ssd1306_type_i2c:
			LOG_I("    Type: I2C");
			dev->i2c = ssd1306_i2c_init(init);
		break;
		case ssd1306_type_spi:
			LOG_I("    Type: SPI");
			dev->spi = ssd1306_spi_init(init);
		break;
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

#if CONFIG_SSD1306_SPLASH > 0
	assert(dev->width >= splash_bmp->width);
	assert(dev->height >= splash_bmp->height);

	const ssd1306_bounds_t splash_bnd = {
		x: (dev->width - splash_bmp->width) / 2,
		y: (dev->height - splash_bmp->height) / 2,
		width: splash_bmp->width,
		height: splash_bmp->height,
	};

	ssd1306_draw_b(&dev->parent, &splash_bnd, splash_bmp);
	vTaskDelay(pdMS_TO_TICKS(CONFIG_SSD1306_SPLASH));
#else
	ssd1306_update(&dev->parent, NULL);
#endif

	if( init->free ) {
		free(init);
	}

	return &dev->parent;
}

#if __SSD1306_FREE
void ssd1306_free(ssd1306_t device)
{
	if( device == NULL ) {
		return;
	}

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	switch( dev->connection.type ) {
		case ssd1306_type_i2c:
			ssd1306_i2c_free(dev->i2c);
		break;
		case ssd1306_type_spi:
			ssd1306_spi_free(dev->spi);
		break;
	}

	free(dev->data)
	free(dev);
}
#endif

bool ssd1306_acquire(ssd1306_t device)
{
	ssd1306_int_t const dev = (ssd1306_int_t)device;

	return xSemaphoreTakeRecursive(dev->mutex, SSD1306_SEM_TICKS);
}

void ssd1306_release(ssd1306_t device)
{
	ssd1306_int_t const dev = (ssd1306_int_t)device;

	xSemaphoreGiveRecursive(dev->mutex);
}

void ssd1306_update(ssd1306_t device, const ssd1306_bounds_t* bounds)
{
	ssd1306_int_t const dev = (ssd1306_int_t)device;

#if CONFIG_SSD1306_OPTIMIZE
	if( dev->no_update ) {
		if( bounds ) {
			ssd1306_extend_bounds(&dev->dirty_bounds, bounds);
		}
	} else {
#else
	if( dev->no_update == 0 ) {
#endif
#if CONFIG_SSD1306_OPTIMIZE
		if( bounds ) {
			xQueueSend(dev->queue, bounds, portMAX_DELAY);
		} else {
			const ssd1306_bounds_t m = { size: dev->size, };

			xQueueSend(dev->queue, &m, portMAX_DELAY);
		}
#else
		xTaskNotifyGive(dev->task);
#endif
	}
}

void ssd1306_auto_update(ssd1306_t device, bool on)
{
	ssd1306_int_t const dev = (ssd1306_int_t)device;

	dev->no_update += on ? -1 : +1;

#if CONFIG_SSD1306_OPTIMIZE
	if( !on && dev->no_update == 1 ) {
		memset(&dev->dirty_bounds, 0, sizeof(dev->dirty_bounds));
	}
	ssd1306_update(device, &dev->dirty_bounds);
#else
	ssd1306_update(device, NULL);
#endif
}

uint8_t* ssd1306_raster(ssd1306_t device, uint8_t page)
{
	ssd1306_int_t const dev = (ssd1306_int_t)device;

	return dev->buff + page * device->width;
}

void ssd1306_on(ssd1306_t device, bool on) {
	ssd1306_int_t const dev = (ssd1306_int_t)device;

	const uint8_t data = OLED_CMD1(DISPLAY_OFF, on);

	if( ssd1306_acquire(device) ) {
		ssd1306_send_buff(dev, OLED_CTL_COMMAND, &data, 1);

		ssd1306_release(device);
	} else {
		LOG_W("Couldn't take mutex");
	}
}

void ssd1306_invert(ssd1306_t device, bool on) {
	ssd1306_int_t const dev = (ssd1306_int_t)device;

	const uint8_t data = OLED_CMD1(DISPLAY_NORMAL, on);

	if( ssd1306_acquire(device) ) {
		ssd1306_send_buff(dev, OLED_CTL_COMMAND, &data, 1);

		ssd1306_release(device);
	} else {
		LOG_W("Couldn't take mutex");
	}
}

void ssd1306_contrast(ssd1306_t device, uint8_t contrast) {
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
	ESP_RETURN_VOID_ON_FALSE(device, LOG_DOMAIN, "device is NULL");

	if( bounds == NULL ) {
		ssd1306_auto_update(device, false);
		ssd1306_clear_b(device, &device->bounds);
		ssd1306_status(device, ssd1306_status_0, NULL);
		ssd1306_status(device, ssd1306_status_1, NULL);
		ssd1306_auto_update(device, true);

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
	const unsigned t_page = trimmed->y / 8;
	const unsigned t_bits = trimmed->y % 8;

	const unsigned b_page = (trimmed->y + trimmed->height) / 8;
	const unsigned b_bits = (trimmed->y + trimmed->height) % 8;

	const uint8_t t_mask = set_bits(-t_bits);
	const uint8_t b_mask = set_bits(8 - b_bits);

	LOG_D("t_page = %u, t_bits = %u, t_mask = 0x%02x", t_page, t_bits, t_mask);
	LOG_D("b_page = %u, b_bits = %u, b_mask = 0x%02x", b_page, b_bits, b_mask);

	uint8_t page = t_page;

	if( t_bits ) {
		ssd1306_mask_page(device, page++, trimmed->x, trimmed->width, t_mask);
	}

	while( page < b_page ) {
		ssd1306_mask_page(device, page++, trimmed->x, trimmed->width, 0);
	}

	if( b_bits ) {
		ssd1306_mask_page(device, page++, trimmed->x, trimmed->width, b_mask);
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
	ESP_RETURN_VOID_ON_FALSE(device, LOG_DOMAIN, "device is NULL");
	ESP_RETURN_VOID_ON_FALSE(bitmap, LOG_DOMAIN, "bitmap is NULL");

	if( bounds == NULL ) {
		ssd1306_draw_b(device, &device->bounds, bitmap);

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

	if( bounds->x < 0 ) {
		image += -bounds->x;
	}

	int y_bits = 0;

	if( bounds->y < 0 ) {
		y_bits = bounds->y & 7;

		image += ((-bounds->y-1) / 8) * bitmap->width;

		LOG_D("Image shifted by %d for y_bits %d", image - bitmap->image, y_bits);
	}

	const int t_page = bounds->y < 0 ? -1 : trimmed->y >> 3;
	const int t_bits = bounds->y < 0 ? y_bits : trimmed->y & 7;

	LOG_D("t_page = %d, t_bits = %d", t_page, t_bits);

	const int b_page = (trimmed->y + trimmed->height) >> 3;
	const int b_bits = (trimmed->y + trimmed->height) & 7;

	LOG_D("b_page = %d, b_bits = %d", b_page, b_bits);

	for( int page = t_page; page < b_page; image += bitmap->width ) {
		if( page >= 0 ) {
			ssd1306_set_page_1(device, page, trimmed->x, trimmed->width, image, -t_bits);
		}

		page++;

		if( t_bits && (page < b_page) ) {
			ssd1306_set_page_1(device, page, trimmed->x, trimmed->width, image, 8-t_bits);
		}
		if( b_bits && (page == b_page) ) {
			ssd1306_set_page_2(device, page, trimmed->x, trimmed->width, image, 8-t_bits, set_bits(8-b_bits));
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
	ESP_RETURN_VOID_ON_FALSE(device, LOG_DOMAIN, "device is NULL");
	ESP_RETURN_VOID_ON_FALSE(bitmap, LOG_DOMAIN, "bitmap is NULL");

	if( bounds == NULL ) {
		ssd1306_grab_b(device, &device->bounds, bitmap);

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

	if( bounds->x < 0 ) {
		image += -bounds->x;
	}

	int y_bits = 0;

	if( bounds->y < 0 ) {
		y_bits = bounds->y & 7;

		image += ((-bounds->y-1) / 8) * bitmap->width;

		LOG_D("Image shifted by %d for y_bits %d", image - bitmap->image, y_bits);
	}

	const int t_page = bounds->y < 0 ? -1 : trimmed->y >> 3;
	const int t_bits = bounds->y < 0 ? y_bits : trimmed->y & 7;

	LOG_D("t_page = %d, t_bits = %d", t_page, t_bits);

	const int b_page = (trimmed->y + trimmed->height) >> 3;
	const int b_bits = (trimmed->y + trimmed->height) & 7;

	LOG_D("b_page = %d, b_bits = %d", b_page, b_bits);

	for( int page = t_page; page < b_page; image += bitmap->width ) {
		if( page >= 0 ) {
			ssd1306_get_page_1(device, page, trimmed->x, trimmed->width, image, t_bits);
		}

		page++;

		if( t_bits && (page < b_page) ) {
			ssd1306_get_page_1(device, page, trimmed->x, trimmed->width, image, t_bits - 8);
		}
		if( b_bits && (page == b_page) ) {
			ssd1306_get_page_2(device, page, trimmed->x, trimmed->width, image, t_bits - 8, set_bits(b_bits - 8));
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

	if( width > device->width ) {
		length += 3;
		text = realloc(text, length);

		ABORT_IF(text == NULL, "cannot reallocate memory for text of size %u", length);

		strcat(text, " \x4 ");

		width = ssd1306_text_width(device, text);
	}

	ssd1306_bitmap_t* const bitmap = ssd1306_create_bitmap(width, SSD1306_TEXT_HEIGHT);

	bool invert = false;

	for( unsigned index = 0, offset = 0; index < length && offset < width; index++ ) {
		if( text[index] == CONFIG_SSD1306_TEXT_INVERT ) {
			invert = !invert;

			continue;
		}

		const unsigned cpos = text[index];
		const ssd1306_glyph_t* glyph = device->font + cpos;

		if( invert ) {
			for( int k = 0; k < glyph->width; k++ ) {
				bitmap->image[offset + k] = ~glyph->image[k];
			}
		} else {
			memcpy(bitmap->image + offset, glyph->image, glyph->width);
		}

		offset += glyph->width;
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
	uint16_t width = 0;

	for( ; *text; text++ ) {
		if( *text == CONFIG_SSD1306_TEXT_INVERT ) {
			continue;
		}

		width += device->font[(uint8_t)(*text)].width;
	}

	LOG_D("text width in pixels of %$s is %u", width);

	return width;
}

char* ssd1306_text_formatv(uint16_t* length, const char* format, va_list args)
{
	uint16_t needed = vsnprintf(NULL, 0, format, args) + 1;
	char* text = malloc(needed);

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

void ssd1306_status(ssd1306_t device, ssd1306_status_t status, const char* format, ...)
{
	if( !ssd1306_acquire(device) ) {
		LOG_W("couldn't take mutex");

		return;
	}

	ssd1306_bounds_t bounds;

	status = ssd1306_status_bounds(device, status, &bounds);

	LOG_V("status %d, bounds %ux%u%+d%+d", status, bounds.w, bounds.h, bounds.x, bounds.y);

	ssd1306_int_t dev = (ssd1306_int_t)device;
	status_info_t* anim = &dev->statuses[status];

	if( anim->bitmap ) {
		free(anim->bitmap);

		anim->bitmap = NULL;
	}

	ssd1306_clear_internal(device, &bounds, &bounds);

	if( format ) {
		ssd1306_bitmap_t* bitmap;
		va_list args;

		va_start(args, format);
		bitmap = ssd1306_text_bitmapv(device, format, args);
		va_end(args);

		ssd1306_bounds_t trimmed = bounds;

		ssd1306_trim(device, &trimmed, &bitmap->size);
		ssd1306_draw_internal(device, &bounds, &trimmed, bitmap);
		ssd1306_update(device, &bounds);

		if( bitmap->width > device->width ) {
			anim->bitmap = bitmap;
			anim->page = bounds.y / 8;
			anim->state = anim_init;

			LOG_D("text will scroll in background");
		} else {
			free(bitmap);
		}
	}

	ssd1306_release(device);
}

ssd1306_status_t ssd1306_status_bounds(ssd1306_t device, ssd1306_status_t status, ssd1306_bounds_t* bounds)
{
	if( status > ssd1306_status_1 ) {
		status = device->flip ? 1 - ((int)status - (int)ssd1306_status_ext) : (int)status - (int)ssd1306_status_ext;
	}

	bounds->x = 0;
	bounds->y = device->flip ? device->height - SSD1306_TEXT_HEIGHT * (2-status) : SSD1306_TEXT_HEIGHT * status;
	bounds->width = device->width;
	bounds->height = SSD1306_TEXT_HEIGHT;

	return status;
}

void ssd1306_init_private(ssd1306_int_t dev, const ssd1306_init_t ini, uint8_t pages)
{
	struct ssd1306_s* device = (struct ssd1306_s*)&dev->parent;

	device->id = ini->id;
	device->flip = ini->flip;
	device->size = ini->size;
	device->pages = pages;
	device->font = ini->font;
	
	memcpy((void*)&dev->connection, &ini->connection, sizeof(dev->connection));
}

void ssd1306_init_screen(ssd1306_int_t dev, const ssd1306_init_t ini)
{
	const uint8_t data[] = {
// 		OLED_CMD_DISPLAY_OFF,
// 		OLED_CMD_SET_DISPLAY_CLK_DIV, 0x80,
// 		OLED_CMD_SET_MUX_RATIO, dev->height-1,
// 		OLED_CMD_SET_DISPLAY_OFFSET, 0x00,
// 		OLED_CMD_SET_DISPLAY_START_LINE | 0x00,
// 		OLED_CMD_SET_CHARGE_PUMP, 0x14,

// #if !CONFIG_SSD1306_OPTIMIZE
// 		OLED_CMD_SET_MEMORY_ADDR_MODE, OLED_CMD_SET_HORI_ADDR_MODE,
// 		OLED_CMD_SET_COLUMN_RANGE, 0, dev->width-1,
// 		OLED_CMD_SET_PAGE_RANGE, 0, dev->pages-1,
// #endif

// 		OLED_CMD_SET_SEGMENT_REMAP | (dev->flip ? 0x00 : 0x01),
// 		OLED_CMD_SET_COM_SCAN_MODE | (dev->flip ? 0x00 : 0x08),

// 		OLED_CMD_SET_COM_PIN_MAP, dev->height == 64 ? 0x12:  0x02,
// 		OLED_CMD_SET_CONTRAST, 0x7F,
// 		OLED_CMD_SET_PRECHARGE, 0xf1,
// 		OLED_CMD_SET_VCOMH_DESELCT, 0x40,

// 		OLED_CMD_DEACTIVE_SCROLL,
// 		OLED_CMD_DISPLAY_NORMAL | (dev->invert ? 0x01 : 0x00),
// 		OLED_CMD_DISPLAY_RAM,
// 		OLED_CMD_DISPLAY_ON,

		OLED_CMD0(DISPLAY_OFF),

		OLED_CMD2(SET_MUX_RATIO, dev->height-1),
		OLED_CMD1(SET_SEGMENT_REMAP, dev->flip ? 0x00 : 0x01),
		OLED_CMD1(SET_COM_SCAN_MODE, dev->flip ? 0x00 : 0x08),
		OLED_CMD2(SET_DISPLAY_CLK_DIV, 0x80),

		OLED_CMD2(SET_COM_PIN_MAP, dev->height == 64 ? 0x12:  0x02),
		OLED_CMD2(SET_VCOMH_DESELCT, 0x40),
		OLED_CMD2(SET_CHARGE_PUMP, 0x14),
		OLED_CMD2(SET_PRECHARGE, 0xf1),

		OLED_CMD0(DISPLAY_RAM),
		OLED_CMD2(SET_DISPLAY_OFFSET, 0x00),
		OLED_CMD1(SET_DISPLAY_START_LINE, 0x00),

		OLED_CMD2(SET_MEMORY_ADDR_MODE, OLED_HORI_ADDR_MODE),

#if !CONFIG_SSD1306_OPTIMIZE
		OLED_CMD3(SET_COLUMN_RANGE, 0, dev->width-1),
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
	if( size ) {
		LOG_V("bounds %ux%u%+d%+d, size %ux%u", bounds->width, bounds->height, bounds->x, bounds->y, size->width, size->height);

		bounds->width = minu(bounds->width, size->width);
		bounds->height = minu(bounds->height, size->height);
	} else {
		LOG_V("bounds %ux%u%+d%+d", bounds->width, bounds->height, bounds->x, bounds->y);
	}

	if( bounds->x < 0 ) {
		unsigned h_shift = -bounds->x;

		if( h_shift >= bounds->width ) {
			LOG_V("bounds %ux%u%+d%+d not visible", bounds->width, bounds->height, bounds->x, bounds->y);

			return false;
		}

		bounds->x = 0;
		bounds->width -= h_shift;

		LOG_V("bounds horizontally shifted by %d and shrunk to %u", h_shift, bounds->width);
	} else if( bounds->x >= device->width ) {
		LOG_V("bounds %ux%u%+d%+d not visible", bounds->width, bounds->height, bounds->x, bounds->y);

		return NULL;
	} else if( bounds->x + bounds->width > device->width ) {
		bounds->width = device->width - bounds->x;

		LOG_V("bounds horizontally shrunk to %u", bounds->width);
	}

	if( bounds->y < 0 ) {
		unsigned v_shift = -bounds->y;

		if( v_shift >= bounds->height ) {
			LOG_V("bounds %ux%u%+d%+d not visible", bounds->width, bounds->height, bounds->x, bounds->y);

			return false;
		}

		bounds->y = 0;
		bounds->height -= v_shift;

		LOG_V("bounds vertically shifted by %d and shrunk to %u", v_shift, bounds->height);
	} else  if( bounds->y >= device->height ) {
		LOG_V("bounds %ux%u%+d%+d not visible", bounds->width, bounds->height, bounds->x, bounds->y);

		return false;
	} else if( bounds->y + bounds->height > device->height ) {
		bounds->height = device->height - bounds->y;

		LOG_V("bounds vertically shrunk to %u", bounds->height);
	}

	LOG_D("trimmed %ux%u%+d%+d", bounds->width, bounds->height, bounds->x, bounds->y);

	return true;
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
