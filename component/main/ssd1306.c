#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-defs.h"
#include "ssd1306-priv.h"

#include <assert.h>
#include <ctype.h>

static void ssd1306_init_screen(ssd1306_priv_t device);
static void ssd1306_mask_page(ssd1306_t device, unsigned page, unsigned offset, unsigned width, uint8_t mask);
static void ssd1306_draw_bt(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed, const ssd1306_bitmap_t* bitmap);
static void ssd1306_grab_bt(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed, ssd1306_bitmap_t* bitmap);
static void ssd1306_set_page_1(ssd1306_t device, unsigned page, unsigned offset, unsigned width, const uint8_t* data, int s_bits);
static void ssd1306_set_page_2(ssd1306_t device, unsigned page, unsigned offset, unsigned width, const uint8_t* data, int s_bits, uint8_t d_mask);
static void ssd1306_get_page_1(ssd1306_t device, unsigned page, unsigned offset, unsigned width, uint8_t* data, int s_bits);
static void ssd1306_get_page_2(ssd1306_t device, unsigned page, unsigned offset, unsigned width, uint8_t* data, int s_bits, uint8_t d_mask);
static bool ssd1306_trim(ssd1306_t device, ssd1306_bounds_t* bounds, const ssd1306_size_t* size);

ssd1306_t ssd1306_init(ssd1306_init_t init)
{
	ssd1306_log_set_level(CONFIG_SSD1306_LOGGING_LEVEL);

	LOG_E("LOG_ERROR");
	LOG_W("LOG_WARN");
	LOG_I("LOG_INFO");
	LOG_D("LOG_DEBUG");
	LOG_V("LOG_VERBOSE");

	// allocate additional bytes for internal buffer and raster
	const unsigned pages = init->height / 8;
	const size_t total = sizeof(ssd1306_priv_s) + pages*init->width;

	ssd1306_t device = malloc(total);

	if( device == NULL ) {
		LOG_E("Cannot allocate memory for new device");

		return NULL;
	}

	ssd1306_priv_t dev = (ssd1306_priv_t)device;

	memcpy(device, init, sizeof(ssd1306_init_s));
	memset((uint8_t*)device + sizeof(ssd1306_init_s), 0, total - sizeof(ssd1306_init_s));

	memcpy((void*)&device->pages, &pages, sizeof(pages));

	dev->head[0] = OLED_CTL_BYTE_DATA_STREAM;

	LOG_I("Allocated %u bytes at %p", total, dev);

	ssd1306_dump(dev, total, "");

	LOG_I("Initialising device");
	LOG_I("    Size: %ux%u", device->width, device->height);
	LOG_I("    Flip: %s", device->flip ? "yes" : "no");

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

	ssd1306_init_screen(dev);

	dev->mutex = xSemaphoreCreateRecursiveMutex();

	configASSERT(dev->mutex);

	static unsigned tasks = 0;

	char task_name[] = "ssd1306-\0";

	task_name[strlen(task_name)] = 'A' + tasks++;

	LOG_I("Creating task %s", task_name);

#if CONFIG_SSD1306_OPTIMIZE
	dev->queue = xQueueCreate(1, sizeof(ssd1306_bounds_t));

	configASSERT(dev->queue);

	xTaskCreate((TaskFunction_t)ssd1306_task, task_name, 1024 * CONFIG_SSD1306_STACK_SIZE, device, CONFIG_SSD1306_PRIORITY, NULL);
#else
	TaskHandle_t task;
	xTaskCreate((TaskFunction_t)ssd1306_task, task_name, 1024 * CONFIG_SSD1306_STACK_SIZE, dev, CONFIG_SSD1306_PRIORITY, &task);
	dev->task = task;
#endif

	while( !dev->active ) {
		vTaskDelay(SSD1306_SEM_TICKS);
	}

#if CONFIG_SSD1306_SPLASH
	assert(device->width >= splash_bmp->width);
	assert(device->height >= splash_bmp->height);

	const ssd1306_bounds_t splash_bnd = {
		x: (device->width - splash_bmp->width) / 2,
		y: (device->height - splash_bmp->height) / 2,
		width: splash_bmp->width,
		height: splash_bmp->height,
	};

	ssd1306_draw_b((ssd1306_t)dev, &splash_bnd, splash_bmp);
#else
	ssd1306_update(device);
#endif

	return device;
}

#if __SSD1306_FREE
void ssd1306_free(ssd1306_t device)
{
	if( device == NULL ) {
		return;
	}

	switch( dev->connection.type ) {
		case ssd1306_type_i2c:
			ssd1306_i2c_free(dev->i2c);
		break;
		case ssd1306_type_spi:
			ssd1306_spi_free(dev->spi);
		break;
	}

	free(device);
}
#endif

bool ssd1306_acquire(ssd1306_t device)
{
	ssd1306_priv_t const dev = (ssd1306_priv_t)device;

	return xSemaphoreTakeRecursive(dev->mutex, SSD1306_SEM_TICKS);
}

void ssd1306_release(ssd1306_t device)
{
	ssd1306_priv_t const dev = (ssd1306_priv_t)device;

	xSemaphoreGiveRecursive(dev->mutex);
}

void ssd1306_update(ssd1306_t device)
{
	ssd1306_priv_t const dev = (ssd1306_priv_t)device;

	if( dev->no_update == 0 ) {
#if CONFIG_SSD1306_OPTIMIZE
		ssd1306_bounds_t m = {
			width: device->width,
			height: device->height,
		};

		if( bounds ) {
			memcpy(&m, bounds, sizeof(m));
		}

		xQueueSend(dev->queue, &m, portMAX_DELAY);
#else
		xTaskNotifyGive(dev->task);
#endif
	}
}

void ssd1306_auto_update(ssd1306_t device, bool on)
{
	ssd1306_priv_t const dev = (ssd1306_priv_t)device;

	dev->no_update += on ? -1 : +1;

	ssd1306_update(device);
}

uint8_t* ssd1306_raster(ssd1306_t device, unsigned page)
{
	ssd1306_priv_t const dev = (ssd1306_priv_t)device;

	return dev->data + page * device->width;
}

void ssd1306_on(ssd1306_t device, bool on) {
	ssd1306_priv_t const dev = (ssd1306_priv_t)device;

	const uint8_t data[] = {
		OLED_CTL_BYTE_CMD_SINGLE,
		OLED_CMD_DISPLAY_OFF | (on ? 0x01 : 0x00),
	};

	if( ssd1306_acquire(device) ) {
		ssd1306_send_data(dev, data, _countof(data));

		ssd1306_release(device);
	} else {
		LOG_W("Couldn't take mutex");
	}
}

void ssd1306_contrast(ssd1306_t device, uint8_t contrast) {
	ssd1306_priv_t const dev = (ssd1306_priv_t)device;

	const uint8_t data[] = {
		OLED_CTL_BYTE_CMD_STREAM,
		OLED_CMD_SET_CONTRAST,
		contrast
	};

	if( ssd1306_acquire(device) ) {
		ssd1306_send_data(dev, data, _countof(data));

		ssd1306_release(device);
	} else {
		LOG_W("Couldn't take mutex");
	}
}

void ssd1306_clear_b(ssd1306_t device, const ssd1306_bounds_t* bounds)
{
	ESP_RETURN_VOID_ON_FALSE(device, LOG_DOMAIN, "device is NULL");

	if( bounds == NULL ) {
		const ssd1306_bounds_t t = {
			width: device->width, height: device->height,
		};

		ssd1306_clear_b(device, &t);

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

	const unsigned t_page = trimmed.y / 8;
	const unsigned t_bits = trimmed.y % 8;

	const unsigned b_page = (trimmed.y + trimmed.height) / 8;
	const unsigned b_bits = (trimmed.y + trimmed.height) % 8;

	const uint8_t t_mask = set_bits(-t_bits);
	const uint8_t b_mask = set_bits(8 - b_bits);

	LOG_D("t_page = %u, t_bits = %u, t_mask = 0x%02x", t_page, t_bits, t_mask);
	LOG_D("b_page = %u, b_bits = %u, b_mask = 0x%02x", b_page, b_bits, b_mask);

	unsigned page = t_page;

	if( t_bits ) {
		ssd1306_mask_page(device, page++, trimmed.x, trimmed.width, t_mask);
	}

	while( page < b_page ) {
		ssd1306_mask_page(device, page++, trimmed.x, trimmed.width, 0);
	}

	if( b_bits ) {
		ssd1306_mask_page(device, page++, trimmed.x, trimmed.width, b_mask);
	}

	ssd1306_update(device);
	ssd1306_release(device);
}

void ssd1306_mask_page(ssd1306_t device, unsigned page, unsigned offset, unsigned width, uint8_t mask)
{
	uint8_t* buff = ssd1306_raster(device, page) + offset;

	if( mask == 0 ) {
		LOG_D("clear page %d", page);

		memset(buff, 0, width);
	} else {
		LOG_D("clear page %d with mask 0x%02x", page, mask);

		for( unsigned x = 0; x < width; x++ ) {
			buff[x] &= mask;
		}
	}
}

void ssd1306_draw_b(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bitmap_t* bitmap)
{
	ESP_RETURN_VOID_ON_FALSE(device, LOG_DOMAIN, "device is NULL");
	ESP_RETURN_VOID_ON_FALSE(bitmap, LOG_DOMAIN, "bitmap is NULL");
	ESP_RETURN_VOID_ON_FALSE(bounds, LOG_DOMAIN, "bounds are NULL");

	ssd1306_bounds_t trimmed = *bounds;

	if( !ssd1306_trim(device, &trimmed, &bitmap->size) ) {
		return;
	}
	if( !ssd1306_acquire(device) ) {
		LOG_W("Couldn't take mutex");

		return;
	}

	ssd1306_draw_bt(device, bounds, &trimmed, bitmap);

	ssd1306_update(device);
	ssd1306_release(device);
}

void ssd1306_draw_bt(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed, const ssd1306_bitmap_t* bitmap)
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

void ssd1306_set_page_1(ssd1306_t device, unsigned page, unsigned offset, unsigned width, const uint8_t* data, int s_bits)
{
	ssd1306_set_page_2(device, page, offset, width, data, s_bits, set_bits(s_bits));
}

void ssd1306_set_page_2(ssd1306_t device, unsigned page, unsigned offset, unsigned width, const uint8_t* data, int s_bits, uint8_t d_mask)
{
	uint8_t* buff = ssd1306_raster(device, page) + offset;

	if( d_mask == 0xff && s_bits == 0 ) {
		LOG_D("set page %d from %p", page, data);

		memcpy(buff, data, width);
	} else {
		const uint8_t s_mask = ~d_mask;

		LOG_D("set page %d with d_mask 0x%02x, s_bits = %+d, s_mask = 0x%02x, from %p", page, d_mask, s_bits, s_mask, data);

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
	ESP_RETURN_VOID_ON_FALSE(bounds, LOG_DOMAIN, "bounds are NULL");

	ssd1306_bounds_t trimmed = *bounds;

	if( !ssd1306_trim(device, &trimmed, &bitmap->size) ) {
		return;
	}
	if( !ssd1306_acquire(device) ) {
		LOG_W("Couldn't take mutex");

		return;
	}

	ssd1306_grab_bt(device, bounds, &trimmed, bitmap);

	ssd1306_update(device);
	ssd1306_release(device);
}

void ssd1306_grab_bt(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed, ssd1306_bitmap_t* bitmap)
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

void ssd1306_get_page_1(ssd1306_t device, unsigned page, unsigned offset, unsigned width, uint8_t* data, int s_bits)
{
	ssd1306_get_page_2(device, page, offset, width, data, s_bits, set_bits(s_bits));
}

void ssd1306_get_page_2(ssd1306_t device, unsigned page, unsigned offset, unsigned width, uint8_t* data, int s_bits, uint8_t d_mask)
{
	const uint8_t* buff = ssd1306_raster(device, page) + offset;

	if( d_mask == 0xff && s_bits == 0 ) {
		LOG_D("get page %d from %p", page, data);

		memcpy(data, buff, width);
	} else {
		const uint8_t s_mask = ~d_mask;

		LOG_D("get page %d with d_mask 0x%02x, s_bits = %+d, s_mask = 0x%02x, from %p", page, d_mask, s_bits, s_mask, data);

		ssd1306_dump(buff, width, "Raster buff");

		for( unsigned x = 0; x < width; x++ ) {
			data[x] = (data[x] & d_mask) | (shift_bits(buff[x], s_bits) & s_mask);
		}

		ssd1306_dump(data, width, "Target buff");
	}
}

void ssd1306_text_b(ssd1306_t device, const ssd1306_bounds_t* bounds, const char* text)
{
	ESP_RETURN_VOID_ON_FALSE(device, LOG_DOMAIN, "device is NULL");

	const ssd1306_size_t size = {
		width: 8 * strlen(text),
		height: 8,
	};

	ssd1306_bounds_t trimmed = *bounds;

	if( !ssd1306_trim(device, &trimmed, &size) ) {
		return;
	}
	if( !ssd1306_acquire(device) ) {
		LOG_W("Couldn't take mutex");

		return;
	}

	bool invert = false;

	ssd1306_bitmap_t* bitmap = ssd1306_create_bitmap(size.width, size.height);

	for( uint8_t* image = (uint8_t*)bitmap->image; *text; text++ ) {
		if( text[0] == device->text_invert.on ) {
			invert = true;

			continue;
		}
		if( text[0] == device->text_invert.off ) {
			invert = false;

			continue;
		}

		const uint8_t cpos = *text;

		// LOG_D("Drawing 0x%02x (%c)", cpos, isprint(cpos) ? cpos : ' ');

		if( invert ) {
			for( int k = 0; k < 8; k++ ) {
				image[k] = ~device->font[cpos].image[k];
			}
		} else {
			memcpy(image, device->font[cpos].image, 8);
		}

		image += 8;
	}

	ssd1306_draw_bt(device, bounds, &trimmed, bitmap);

	free(bitmap);

	ssd1306_update(device);
	ssd1306_release(device);
}

void ssd1306_status(ssd1306_t device, ssd1306_status_t status, const char* format, ...)
{
	unsigned size;

	va_list args;

	va_start(args, format);
	size = vsnprintf(NULL, 0, format, args);
	va_end(args);

	char text[size+1];

	vsnprintf(text, sizeof(text), format, args);

	if( status > ssd1306_status_1 ) {
		status = device->flip ? 1 - ((int)status - (int)ssd1306_status_ext) : (int)status - (int)ssd1306_status_ext;
	}

	if( device->flip ) {
		ssd1306_text(device, 0, device->height - (2-status) * 8, device->width, text);
	} else {
		ssd1306_text(device, 0, status * 8, device->width, text);
	}
}

void ssd1306_init_screen(ssd1306_priv_t dev)
{
	const uint8_t data[] = {
		OLED_CTL_BYTE_CMD_STREAM,

		OLED_CMD_DISPLAY_OFF,
		OLED_CMD_SET_DISPLAY_CLK_DIV, 0x80,
		OLED_CMD_SET_MUX_RATIO, dev->height-1,
		OLED_CMD_SET_DISPLAY_OFFSET, 0x00,
		OLED_CMD_SET_DISPLAY_START_LINE | 0x00,
		OLED_CMD_SET_CHARGE_PUMP, 0x14,

#if !CONFIG_SSD1306_OPTIMIZE
		OLED_CMD_SET_MEMORY_ADDR_MODE, OLED_CMD_SET_HORI_ADDR_MODE,
		OLED_CMD_SET_COLUMN_RANGE, 0, dev->width-1,
		OLED_CMD_SET_PAGE_RANGE, 0, dev->pages-1,
#endif

		OLED_CMD_SET_SEGMENT_REMAP | (dev->flip ? 0x00 : 0x01),
		OLED_CMD_SET_COM_SCAN_MODE | (dev->flip ? 0x00 : 0x08),

		OLED_CMD_SET_COM_PIN_MAP, dev->height == 64 ? 0x12:  0x02,
		OLED_CMD_SET_CONTRAST, 0x7F,
		OLED_CMD_SET_PRECHARGE, 0xf1,
		OLED_CMD_SET_VCOMH_DESELCT, 0x40,

		OLED_CMD_DEACTIVE_SCROLL,
		OLED_CMD_DISPLAY_NORMAL | (dev->invert ? 0x01 : 0x00),
		OLED_CMD_DISPLAY_RAM,
		OLED_CMD_DISPLAY_ON,
	};

	ssd1306_send_data(dev, data, _countof(data));
}

bool ssd1306_trim(ssd1306_t device, ssd1306_bounds_t* bounds, const ssd1306_size_t* size)
{
	LOG_D("Bounds %ux%u%+d%+d", bounds->width, bounds->height, bounds->x, bounds->y);

	if( size ) {
		bounds->width = minu(bounds->width, size->width);
		bounds->height = minu(bounds->height, size->height);
	}

	if( bounds->x < 0 ) {
		unsigned h_shift = -bounds->x;

		if( h_shift >= bounds->width ) {
			LOG_D("Bounds %ux%u%+d%+d not visible", bounds->width, bounds->height, bounds->x, bounds->y);

			return false;
		}

		bounds->x = 0;
		bounds->width -= h_shift;

		LOG_D("Bounds horizontally shifted by %d and shrunk to %u", h_shift, bounds->width);
	} else if( bounds->x >= device->width ) {
		LOG_D("Bounds %ux%u%+d%+d not visible", bounds->width, bounds->height, bounds->x, bounds->y);

		return NULL;
	} else if( bounds->x + bounds->width > device->width ) {
		bounds->width = device->width - bounds->x;

		LOG_D("Bounds horizontally shrunk to %u", bounds->width);
	}

	if( bounds->y < 0 ) {
		unsigned v_shift = -bounds->y;

		if( v_shift >= bounds->height ) {
			LOG_D("Bounds %ux%u%+d%+d not visible", bounds->width, bounds->height, bounds->x, bounds->y);

			return false;
		}

		bounds->y = 0;
		bounds->height -= v_shift;

		LOG_D("Bounds vertically shifted by %d and shrunk to %u", v_shift, bounds->height);

	} else  if( bounds->y >= device->height ) {
		LOG_D("Bounds %ux%u%+d%+d not visible", bounds->width, bounds->height, bounds->x, bounds->y);

		return false;
	} else if( bounds->y + bounds->height > device->height ) {
		bounds->height = device->height - bounds->y;

		LOG_D("Bounds vertically shrunk to %u", bounds->height);
	}

	LOG_D("Trimmed %ux%u%+d%+d", bounds->width, bounds->height, bounds->x, bounds->y);

	return true;
}

ssd1306_bitmap_t* ssd1306_create_bitmap(unsigned width, unsigned height)
{
	const ssd1306_size_t size = {
		width: width,
		height: height,
	};

	const unsigned length = width * (height / 8 + (height % 8 ? 1 : 0));

	LOG_D("Create bitmap of %ux%u, %u bytes", width, height, length);

	ssd1306_bitmap_t* bitmap = calloc(1, sizeof(ssd1306_bitmap_t) + length );

	assert(bitmap != NULL);

	memcpy((void*)&bitmap->size, &size, sizeof(size));

	return bitmap;
}
