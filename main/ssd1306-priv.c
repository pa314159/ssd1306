#include "sdkconfig.h"
#include "ssd1306.h"
#include "ssd1306-priv.h"
#include "ssd1306-defs.h"

#include <esp_timer.h>

static void ssd1306_send(ssd1306_t device, const ssd1306_bounds_t* bounds);

void ssd1306_task(ssd1306_t device)
{
	LOG_I("Starting task %s", pcTaskGetName(xTaskGetCurrentTaskHandle()));

	device->active = true;

	while( device->active ) {
		LOG_V("Waiting for event");

#if CONFIG_SSD1306_OPTIMIZE
		ssd1306_bounds_t bounds;

		if( xQueueReceive(device->queue, &bounds, portMAX_DELAY) == pdFALSE ) {
			continue;
		}

		if( xSemaphoreTake(device->mutex, portMAX_DELAY) ) {
			ssd1306_send(device, &bounds);

			xSemaphoreGive(device->mutex);
		}
#else
		if( ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdFALSE ) {
			continue;
		}

		LOG_V("Got event");

		if( xSemaphoreTake(device->mutex, portMAX_DELAY) ) {
			const ssd1306_bounds_t bounds = {
				width: device->width,
				height: device->height,
			};

			ssd1306_send(device, &bounds);

			xSemaphoreGive(device->mutex);
		}
#endif
	}
}

void ssd1306_send_data(ssd1306_t device, const uint8_t* data, size_t size)
{
	LOG_V("data = %p, size = %u", data, size);

	switch( device->connection.type ) {
		case ssd1306_type_i2c:
			ssd1306_i2c_send(device->i2c, data, size);
		break;
		case ssd1306_type_spi:
			ssd1306_spi_send(device->spi, data, size);
		break;
	}
}

void ssd1306_send(ssd1306_t device, const ssd1306_bounds_t* bounds)
{
	int x0 = bounds->x;
	int y0 = bounds->y;
	int x1 = bounds->x + bounds->width;
	int y1 = bounds->y + bounds->height;

	const uint8_t data[] = {
		OLED_CTL_BYTE_CMD_STREAM,
		OLED_CMD_SET_MEMORY_ADDR_MODE, OLED_CMD_SET_HORI_ADDR_MODE,
		OLED_CMD_SET_COLUMN_RANGE, x0, x1 - 1,
		OLED_CMD_SET_PAGE_RANGE, y0 / 8, y1 / 8 - 1,
	};

	ssd1306_dump(data, _countof(data), "");

	uint64_t start = esp_timer_get_time();

	ssd1306_send_data(device, data, _countof(data));
	ssd1306_send_data(device, raster_head(device), device->width * device->pages + 1);

	LOG_V("ended after %u \u03BCs", esp_timer_get_time()-start);
}
