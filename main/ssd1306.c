#include "sdkconfig.h"
#include "ssd1306.h"
#include "ssd1306-defs.h"
#include "ssd1306-priv.h"

static void ssd1306_init_screen(ssd1306_t device);
static void ssd1306_mask_page(ssd1306_t device, unsigned page, unsigned offset, unsigned width, uint8_t mask);
static void ssd1306_fill_page_1(ssd1306_t device, unsigned page, unsigned offset, unsigned width, const uint8_t* data, int s_bits);
static void ssd1306_fill_page_2(ssd1306_t device, unsigned page, unsigned offset, unsigned width, const uint8_t* data, int s_bits, uint8_t d_mask);
static bool ssd1306_trim(ssd1306_t device, ssd1306_bounds_t* bounds, const ssd1306_size_t* size);

ssd1306_t ssd1306_init(ssd1306_init_t init)
{
	ssd1306_log_level(CONFIG_SSD1306_LOGGING_LEVEL);

	LOG_E("LOG_ERROR");
	LOG_W("LOG_WARN");
	LOG_I("LOG_INFO");
	LOG_D("LOG_DEBUG");
	LOG_V("LOG_VERBOSE");

	// allocate additional bytes for internal buffer and raster
	const unsigned pages = init->height / 8;
	const size_t total = sizeof(ssd1306_s) + pages*init->width;

	ssd1306_t device = malloc(total);

	if( device == NULL ) {
		LOG_E("Cannot allocate memory for new device");

		return NULL;
	}

	memcpy(device, init, sizeof(ssd1306_init_s));
	memset((uint8_t*)device + sizeof(ssd1306_init_s), 0, total - sizeof(ssd1306_init_s));

	device->pages = pages;
	device->head[0] = OLED_CTL_BYTE_DATA_STREAM;

	LOG_I("Allocated %u bytes at %p", total, device);

	ssd1306_dump(device, total, "");

	LOG_I("Initialising device");
	LOG_I("    Size: %ux%u", device->width, device->height);
	LOG_I("    Flip: %s", device->flip ? "yes" : "no");

	switch( init->connection.type ) {
		case ssd1306_type_i2c:
			LOG_I("    Type: I2C");
			device->i2c = ssd1306_i2c_init(init);
		break;
		case ssd1306_type_spi:
			LOG_I("    Type: SPI");
			device->spi = ssd1306_spi_init(init);
		break;
	}

	ssd1306_init_screen(device);

	device->mutex = xSemaphoreCreateRecursiveMutex();

	configASSERT(device->mutex);

	static unsigned tasks = 0;

	char task_name[] = "ssd1306-\0";

	task_name[strlen(task_name)] = 'A' + tasks++;

	LOG_I("Creating task %s", task_name);

#if CONFIG_SSD1306_OPTIMIZE
	device->queue = xQueueCreate(1, sizeof(ssd1306_bounds_t));

	configASSERT(device->queue);

	xTaskCreate((TaskFunction_t)ssd1306_task, task_name, 1024 * CONFIG_SSD1306_STACK_SIZE, device, CONFIG_SSD1306_PRIORITY, NULL);
#else
	TaskHandle_t task;
	xTaskCreate((TaskFunction_t)ssd1306_task, task_name,  1024 * CONFIG_SSD1306_STACK_SIZE, device, CONFIG_SSD1306_PRIORITY, &task);
	device->task = task;
#endif

	while( !device->active ) {
		vTaskDelay(SSD1306_SEM_TICKS);
	}

	vTaskDelay(pdMS_TO_TICKS(500));

	ssd1306_update(device);

#if CONFIG_SSD1306_SPLASH
	ssd1306_show_splash(device);
#endif

	return device;
}

#if __SSD1306_FREE
void ssd1306_free(ssd1306_t device)
{
	if( device == NULL ) {
		return;
	}

	switch( device->connection.type ) {
		case ssd1306_type_i2c:
			ssd1306_i2c_free(device->i2c);
		break;
		case ssd1306_type_spi:
			ssd1306_spi_free(device->spi);
		break;
	}

	free(device);
}
#endif

bool ssd1306_acquire(ssd1306_t device)
{
	return xSemaphoreTakeRecursive(device->mutex, SSD1306_SEM_TICKS);
}

void ssd1306_release(ssd1306_t device)
{
	xSemaphoreGiveRecursive(device->mutex);
}

void ssd1306_update(ssd1306_t device)
{
	if( device->no_update == 0 ) {
#if CONFIG_SSD1306_OPTIMIZE
		ssd1306_bounds_t m = {
			width: device->width,
			height: device->height,
		};

		if( bounds ) {
			memcpy(&m, bounds, sizeof(m));
		}

		xQueueSend(device->queue, &m, portMAX_DELAY);
#else
		xTaskNotifyGive(device->task);
#endif
	}
}

void ssd1306_auto_update(ssd1306_t device, bool on)
{
	device->no_update += on ? -1 : +1;

	ssd1306_update(device);
}

void ssd1306_on(ssd1306_t device, bool on) {
	const uint8_t data[] = {
		OLED_CTL_BYTE_CMD_SINGLE,
		OLED_CMD_DISPLAY_OFF | (on ? 0x01 : 0x00),
	};

	if( ssd1306_acquire(device) ) {
		ssd1306_send_data(device, data, _countof(data));

		ssd1306_release(device);
	} else {
		LOG_W("Couldn't take mutex");
	}
}

void ssd1306_contrast(ssd1306_t device, uint8_t contrast) {
	const uint8_t data[] = {
		OLED_CTL_BYTE_CMD_STREAM,
		OLED_CMD_SET_CONTRAST,
		contrast
	};

	if( ssd1306_acquire(device) ) {
		ssd1306_send_data(device, data, _countof(data));

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

	ssd1306_release(device);
	ssd1306_update(device);
}

void ssd1306_mask_page(ssd1306_t device, unsigned page, unsigned offset, unsigned width, uint8_t mask)
{
	uint8_t* buff = raster_page(device, page) + offset;

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

static void ssd1306_bitmap_bt(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bounds_t* trimmed, const ssd1306_bitmap_t* bitmap)
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
			ssd1306_fill_page_1(device, page, trimmed->x, trimmed->width, image, -t_bits);
		}

		page++;

		if( t_bits && (page < b_page) ) {
			ssd1306_fill_page_1(device, page, trimmed->x, trimmed->width, image, 8-t_bits);
		}
		if( b_bits && (page == b_page) ) {
			ssd1306_fill_page_2(device, page, trimmed->x, trimmed->width, image, (8-t_bits) & 7, set_bits(8-b_bits));
		}
	}
}

void ssd1306_bitmap_b(ssd1306_t device, const ssd1306_bounds_t* bounds, const ssd1306_bitmap_t* bitmap)
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

	ssd1306_bitmap_bt(device, bounds, &trimmed, bitmap);

	ssd1306_release(device);
	ssd1306_update(device);
}

void ssd1306_fill_page_1(ssd1306_t device, unsigned page, unsigned offset, unsigned width, const uint8_t* data, int s_bits)
{
	ssd1306_fill_page_2(device, page, offset, width, data, s_bits, set_bits(s_bits));
}

void ssd1306_fill_page_2(ssd1306_t device, unsigned page, unsigned offset, unsigned width, const uint8_t* data, int s_bits, uint8_t d_mask)
{
	uint8_t* buff = raster_page(device, page) + offset;

	if( d_mask == 0xff && s_bits == 0 ) {
		LOG_D("fill page %d from %p", page, data);

		memcpy(buff, data, width);
	} else {
		const uint8_t s_mask = ~d_mask;

		LOG_D("fill page %d with d_mask 0x%02x, s_bits = %+d, s_mask = 0x%02x, from %p",
			page, d_mask, s_bits, s_mask, data);

		for( unsigned x = 0; x < width; x++ ) {
			buff[x] = (buff[x] & d_mask) | (shift_bits(data[x], s_bits) & s_mask);
		}

		ssd1306_dump(buff, width, "");
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

	ssd1306_bitmap_t* bitmap = calloc(1, sizeof(ssd1306_bitmap_t) + size.width);

	bitmap->size = size;

	for( uint8_t* image = (uint8_t*)bitmap->image; *text; text++, image += 8 ) {
		if( text[0] == device->text_invert.on ) {
			invert = true;

			continue;
		}
		if( text[0] == device->text_invert.off ) {
			invert = false;

			continue;
		}

		if( invert ) {
			for( int k = 0; k < 8; k++ ) {
				image[k] = ~device->font[*text & 0xFF].image[k];
			}
		} else {
			memcpy(image, device->font[*text & 0xFF].image, 8);
		}
	}

	ssd1306_bitmap_bt(device, bounds, &trimmed, bitmap);

	free(bitmap);

	ssd1306_release(device);
	ssd1306_update(device);
}

void ssd1306_status(ssd1306_t device, ssd1306_status_t status, const char* format, ...)
{
	unsigned size = device->width / 8;
	char text[size];

	va_list args;

	va_start(args, format);
	vsnprintf(text, size, format, args);
	va_end(args);

	if( device->flip ) {
		ssd1306_text(device, 0, status * 8, device->width, text);
	} else {
		ssd1306_text(device, 0, device->height - (2-status) * 8, device->width, text);
	}
}

void ssd1306_init_screen(ssd1306_t device)
{
	const uint8_t data[] = {
		OLED_CTL_BYTE_CMD_STREAM,
		OLED_CMD_DISPLAY_OFF,
		OLED_CMD_SET_COLUMN_RANGE, 0, device->width-1,
		OLED_CMD_SET_PAGE_RANGE, 0, device->pages-1,
		OLED_CMD_SET_MUX_RATIO, device->height-1,
		OLED_CMD_SET_DISPLAY_OFFSET, 0x00,
		OLED_CMD_SET_DISPLAY_START_LINE | 0x00,
		OLED_CMD_SET_CONTRAST, 0x7F,
		OLED_CMD_SET_SEGMENT_REMAP | (device->flip ? 0x01 : 0x00),
		OLED_CMD_SET_COM_SCAN_MODE | (device->flip ? 0x08 : 0x00),

		OLED_CMD_SET_DISPLAY_CLK_DIV, 0x80,
		OLED_CMD_SET_PRECHARGE, 0xf1,
		OLED_CMD_SET_CHARGE_PUMP, 0x14,
		OLED_CMD_SET_VCOMH_DESELCT, 0x30,
		OLED_CMD_SET_COM_PIN_MAP, device->height == 64 ? 0x12:  0x02,

		OLED_CMD_DEACTIVE_SCROLL,
		OLED_CMD_DISPLAY_NORMAL | (device->invert ? 0x01 : 0x00),
		OLED_CMD_DISPLAY_RAM,
		OLED_CMD_DISPLAY_ON,
	};

	ssd1306_send_data(device, data, _countof(data));
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
