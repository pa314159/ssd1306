#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-priv.h"
#include "ssd1306-defs.h"

#include <esp_timer.h>

static void ssd1306_send(ssd1306_priv_t dev, const ssd1306_bounds_t* bounds);

void ssd1306_task(ssd1306_priv_t dev)
{
	LOG_I("Starting task %s", pcTaskGetName(xTaskGetCurrentTaskHandle()));

	dev->active = true;

	while( dev->active ) {
		LOG_V("Waiting for event");

#if CONFIG_SSD1306_OPTIMIZE
		ssd1306_bounds_t bounds;

		if( xQueueReceive(dev->queue, &bounds, portMAX_DELAY) == pdFALSE ) {
			continue;
		}

		if( xSemaphoreTake(dev->mutex, portMAX_DELAY) ) {
			ssd1306_send(device, &bounds);

			xSemaphoreGive(dev->mutex);
		}
#else
		if( ulTaskNotifyTake(pdTRUE, portMAX_DELAY) == pdFALSE ) {
			continue;
		}

		LOG_V("Got event");

		if( xSemaphoreTake(dev->mutex, portMAX_DELAY) ) {
			const ssd1306_bounds_t bounds = {
				width: dev->width,
				height: dev->height,
			};

			ssd1306_send(dev, &bounds);

			xSemaphoreGive(dev->mutex);
		}
#endif
	}
}

void ssd1306_send_data(ssd1306_priv_t dev, const uint8_t* data, size_t size)
{
	LOG_V("data = %p, size = %u", data, size);

	switch( dev->connection.type ) {
		case ssd1306_type_i2c:
			ssd1306_i2c_send(dev->i2c, data, size);
		break;
		case ssd1306_type_spi:
			ssd1306_spi_send(dev->spi, data, size);
		break;
	}
}

void ssd1306_send(ssd1306_priv_t dev, const ssd1306_bounds_t* bounds)
{
	const uint64_t start = esp_timer_get_time();

#if CONFIG_SSD1306_OPTIMIZE
	int x0 = bounds->x;
	int y0 = bounds->y;
	int x1 = bounds->x + bounds->width;
	int y1 = bounds->y + bounds->height;

	const uint8_t data[] = {
		OLED_CTL_BYTE_CMD_STREAM,
		OLED_CMD_SET_COLUMN_RANGE, x0, x1 - 1,
		OLED_CMD_SET_PAGE_RANGE, y0 / 8, y1 / 8 - 1,
	};

	ssd1306_dump(data, _countof(data), "");

	ssd1306_send_data(dev, data, _countof(data));
#endif

	ssd1306_send_data(dev, dev->head, dev->width * dev->pages + 1);

	LOG_V("ended after %u \u03BCs", esp_timer_get_time()-start);
}
