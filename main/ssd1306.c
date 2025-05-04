#include "sdkconfig.h"
#include "ssd1306.h"
#include "ssd1306-defs.h"
#include "ssd1306-priv.h"

static void ssd1306_init_screen(ssd1306_t device);
static void ssd1306_notify(ssd1306_t device);
static ssd1306_bounds_t* ssd1306_trim(ssd1306_t device, const ssd1306_bounds_t* bounds);

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
	const size_t total = sizeof(ssd1306_s) + pages*(init->width+1);

	ssd1306_t device = malloc(total);

	memcpy(device, init, sizeof(ssd1306_init_s));
	memset((uint8_t*)device + sizeof(ssd1306_init_s), 0, total - sizeof(ssd1306_init_s));

	device->pages = pages;

	for( unsigned page = 0; page < pages; page++ ) {
		raster_page_head(device, page)[0] = OLED_CTL_BYTE_DATA_STREAM;
	}

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

	device->mutex = xSemaphoreCreateMutex();

	configASSERT(device->mutex);

	xTaskCreate((TaskFunction_t)ssd1306_task, "ssd1306", 2048, device, tskIDLE_PRIORITY, &device->task);

	while( !device->active ) {
		vTaskDelay(SSD1306_SEM_TICKS);
	}

	vTaskDelay(pdMS_TO_TICKS(2000));
	// ssd1306_send(device);
	ssd1306_notify(device);

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

void ssd1306_on(ssd1306_t device, bool on) {
	const uint8_t data[] = {
		OLED_CTL_BYTE_CMD_SINGLE,
		OLED_CMD_DISPLAY_OFF | (on ? 0x01 : 0x00),
	};

	if( xSemaphoreTake(device->mutex, SSD1306_SEM_TICKS) ) {
		ssd1306_send_data(device, data, _countof(data));

		xSemaphoreGive(device->mutex);
		ssd1306_notify(device);
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

	if( xSemaphoreTake(device->mutex, SSD1306_SEM_TICKS) ) {
		ssd1306_send_data(device, data, _countof(data));

		xSemaphoreGive(device->mutex);
		ssd1306_notify(device);
	} else {
		LOG_W("Couldn't take mutex");
	}
}

void ssd1306_clear_b(ssd1306_t device, const ssd1306_bounds_t* bounds)
{
	ESP_RETURN_VOID_ON_FALSE(device, LOG_DOMAIN, "device is NULL");

	if( bounds == NULL ) {
		const ssd1306_bounds_t t = {
			w: device->width, h: device->height,
		};

		ssd1306_clear_b(device, &t);

		return;
	}

	ssd1306_bounds_t* trimmed = ssd1306_trim(device, bounds);

	if( !trimmed ) {
		return;
	}

	const unsigned t_rest = (trimmed->y & 7);
	const unsigned b_rest = (trimmed->y + trimmed->h) & 7;

	const unsigned t_page = (trimmed->y >> 3);
	const unsigned b_page = (trimmed->y + trimmed->h) >> 3;

	if( xSemaphoreTake(device->mutex, pdMS_TO_TICKS(SSD1306_SEM_TIMEOUT)) ) {
		unsigned page = t_page;

		if( t_rest ) {
			const uint8_t mask = (uint8_t)(1 << t_rest) - 1;
			uint8_t* buff = raster_page_data(device, page) + trimmed->x;

			for( unsigned x = 0; x < trimmed->w; x++ ) {
				buff[x] &= mask;
			}

			page++;
		}

		for( ; page < b_page; page++ ) {
			memset(raster_page_data(device, page) + trimmed->x, 0, trimmed->w);
		}

		if( b_rest ) {
			const uint8_t mask = (uint8_t)(1 << t_rest) - 1;
			uint8_t* buff = raster_page_data(device, page) + trimmed->x;

			for( unsigned x = 0; x < trimmed->w; x++ ) {
				buff[x] &= mask;
			}

			page++;
		}

		xSemaphoreGive(device->mutex);
		ssd1306_notify(device);
	} else {
		LOG_W("Couldn't take mutex");
	}
}

void ssd1306_bitmap_b(ssd1306_t device, const uint8_t* bitmap, const ssd1306_bounds_t* bounds)
{
	ESP_RETURN_VOID_ON_FALSE(device, LOG_DOMAIN, "device is NULL");
	ESP_RETURN_VOID_ON_FALSE(bitmap, LOG_DOMAIN, "bitmap is NULL");
	ESP_RETURN_VOID_ON_FALSE(bounds, LOG_DOMAIN, "bounds are NULL");

	ssd1306_bounds_t* trimmed = ssd1306_trim(device, bounds);

	if( !trimmed ) {
		return;
	}

	if( bounds->x < 0 ) {
		bitmap += -bounds->x;
	}
	if( bounds->y < 0 ) {
		bitmap += (-bounds->x / 8) * bounds->w;
	}

	const unsigned t_rest = trimmed->y & 7;
	const unsigned b_rest = (trimmed->y + trimmed->h) & 7;

	const unsigned t_page = trimmed->y >> 3;
	const unsigned b_page = (trimmed->y + trimmed->h) >> 3;

	ssd1306_auto_flush(device, false);

	if( t_rest || b_rest ) {
		LOG_W("Unaligned bitmap %ux%u at x = %d, y = %d, t_page = %u, t_rest = %u, b_page = %u, b_rest = %u",
			trimmed->w, trimmed->h, trimmed->x, trimmed->y, t_page, t_rest, b_page, b_rest);

		for( unsigned page = t_page; page < b_page; page++, bitmap += bounds->w) {
			if( page > t_page ) {
				ssd1306_fill_page(device, page - 1, trimmed->x, trimmed->w, bitmap - bounds->w, t_rest - 8);
			}

			ssd1306_fill_page(device, page, trimmed->x, trimmed->w, bitmap, t_rest);
		}

		if( b_rest ) {
			ssd1306_fill_page(device, b_page - 1, trimmed->x, trimmed->w, bitmap - bounds->w, b_rest);
		}
	} else {
		for( unsigned page = t_page; page < b_page; page++, bitmap += bounds->w ) {
			ssd1306_fill_page(device, page, trimmed->x, trimmed->w, bitmap, 0);
		}
	}

	ssd1306_auto_flush(device, true);
}

void ssd1306_text_b(ssd1306_t device, const char* text, const ssd1306_bounds_t* bounds)
{
	ESP_RETURN_VOID_ON_FALSE(device, LOG_DOMAIN, "device is NULL");
}

void ssd1306_fill_page(ssd1306_t device, unsigned page, unsigned offset, unsigned width, const uint8_t* data, int bits)
{
	ESP_RETURN_VOID_ON_FALSE(device, LOG_DOMAIN, "device is NULL");
	ESP_RETURN_VOID_ON_FALSE(data, LOG_DOMAIN, "data is NULL");

	ESP_RETURN_VOID_ON_FALSE(page < device->pages, LOG_DOMAIN, "page invalid page, page = %u, offset = %u, width = %u", page, offset, width);
	ESP_RETURN_VOID_ON_FALSE(offset < device->width, LOG_DOMAIN, "offset is invalid, page = %u, offset = %u, width = %u", page, offset, width);
	ESP_RETURN_VOID_ON_FALSE(width && width <= device->width - offset, LOG_DOMAIN, "width is invalid, page = %u, offset = %u, width = %u", page, offset, width);

	ESP_RETURN_VOID_ON_FALSE(-8 < bits && bits < 8, LOG_DOMAIN, "invalid bits offset %d", bits);

	if( xSemaphoreTake(device->mutex, SSD1306_SEM_TICKS) )  {
		uint8_t* buff = raster_page_data(device, page) + offset;

		if( bits < 0 ) {
			bits = -bits;

			const uint8_t mask = (uint8_t)(1 << bits) - 1;

			for( unsigned k = 0; k < width; k++ ) {
				buff[k] &= mask;
				buff[k] |= data[k] << bits;
			}

			ssd1306_dump(buff, width, "page = %u, offset = %u, width = %u, bits = %d, mask = 0x%02x", page, offset, width, -bits, mask);
		} else if( bits > 0 ) {
			const uint8_t mask = ((int8_t)0x80) >> (bits-1);

			for( unsigned k = 0; k < width; k++ ) {
				buff[k] &= mask;
				buff[k] |= data[k] >> bits;
			}

			ssd1306_dump(buff, width, "page = %u, offset = %u, width = %u, bits = %d, mask = 0x%02x", page, offset, width, bits, mask);
		} else if( bits == 0 ) {
			memcpy(buff, data, width);

			ssd1306_dump(buff, width, "page = %u, offset = %u, width = %u", page, offset, width);
		}

		xSemaphoreGive(device->mutex);
		ssd1306_notify(device);
	} else {
		LOG_W("Couldn't take mutex");
	}
}

void ssd1306_auto_flush(ssd1306_t device, bool on)
{
	device->no_flush += on ? -1 : +1;

	ssd1306_notify(device);
}

static void ssd1306_init_screen(ssd1306_t device)
{
	const uint8_t data[] = {
		OLED_CTL_BYTE_CMD_STREAM,
		OLED_CMD_DISPLAY_OFF,
		OLED_CMD_SET_COLUMN_RANGE, 0, device->width-1,
		OLED_CMD_SET_PAGE_RANGE, 0, device->pages-1,
		OLED_CMD_SET_MUX_RATIO, device->height-1,
		OLED_CMD_SET_DISPLAY_OFFSET, 0x00,
		OLED_CMD_SET_DISPLAY_START_LINE,
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

void ssd1306_notify(ssd1306_t device)
{
	LOG_D("no_flush > 0: %u", device->no_flush);

	if( device->no_flush == 0 ) {
		xTaskNotifyGive(device->task);
	}
}

ssd1306_bounds_t* ssd1306_trim(ssd1306_t device, const ssd1306_bounds_t* bounds)
{
	ssd1306_bounds_t temp;

	memcpy(&temp, bounds, sizeof(ssd1306_bounds_t));

	LOG_D("Bounds %ux%u at x = %d, y = %d", bounds->w, bounds->h, bounds->x, bounds->y);

	if( temp.x < 0 ) {
		unsigned h_shift = -temp.x;

		if( h_shift >= bounds->w ) {
			LOG_I("Bounds %ux%u not visible at x = %d, y = %d", bounds->w, bounds->h, bounds->x, bounds->y);

			return NULL;
		}

		temp.x = 0;
		temp.w -= h_shift;

		LOG_D("Bounds h-shifted by %d and h-shrunk to %u", h_shift, temp.w);
	} else if( temp.x >= device->width ) {
		LOG_I("Bounds %ux%u not visible at x = %d, y = %d", bounds->w, bounds->h, bounds->x, bounds->y);

		return NULL;
	} else if( temp.x + temp.w > device->width ) {
		temp.w = device->width - temp.x;

		LOG_D("Bounds h-shrunk to %u", temp.w);
	}

	if( temp.y < 0 ) {
		unsigned v_shift = -temp.y;

		if( v_shift >= bounds->h ) {
			LOG_I("Bounds %ux%u not visible at x = %d, y = %d", bounds->w, bounds->h, bounds->x, bounds->y);

			return NULL;
		}

		temp.y = 0;
		temp.h -= v_shift;

		LOG_D("Bounds v-shifted by %d and v-shrunk to %u", v_shift, temp.h);

	} else  if( temp.y >= device->height ) {
		LOG_I("Bounds %ux%u not visible at x = %d, y = %d", bounds->w, bounds->h, bounds->x, bounds->y);

		return NULL;
	} else if( temp.y + temp.h > device->height ) {
		temp.h = device->height - temp.y;

		LOG_D("Bounds v-shrunk to %u", temp.h);
	}

	ssd1306_bounds_t* trimmed = malloc(sizeof(ssd1306_bounds_t));

	memcpy(trimmed, &temp, sizeof(ssd1306_bounds_t));

	return trimmed;
}
