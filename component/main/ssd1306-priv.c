#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-priv.h"
#include "ssd1306-defs.h"

#include <esp_timer.h>

#define SCREEN_SCROLL_FPS   30
#define SCREEN_SCROLL_TICKS pdMS_TO_TICKS(1000/SCREEN_SCROLL_FPS)

static void update_display(ssd1306_priv_t dev);
static const status_info_t* update_status(ssd1306_priv_t dev, uint8_t index);
static void move_status(ssd1306_priv_t dev, status_info_t* status);

inline bool is_move(const status_info_t* status)
{
	return status && status->state == anim_move;
}

static void ssd1306_send(ssd1306_priv_t dev, const ssd1306_bounds_t* bounds);

void ssd1306_task(ssd1306_priv_t dev)
{
	LOG_I("Starting task %s, sizeof(int) = %d", pcTaskGetName(xTaskGetCurrentTaskHandle()), sizeof(int));

	dev->active = true;

	TickType_t delay = portMAX_DELAY;

	while( dev->active ) {
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
		bool notified = ulTaskNotifyTake(pdTRUE, delay);

		if( xSemaphoreTake(dev->mutex, portMAX_DELAY) ) {
			const status_info_t* s0 = update_status(dev, 0);
			const status_info_t* s1 = update_status(dev, 1);

			delay = (s0 || s1) ? SCREEN_SCROLL_TICKS : portMAX_DELAY;

			if( notified || is_move(s0) || is_move(s1) ) {
				update_display(dev);
			}

			xSemaphoreGive(dev->mutex);
		}
#endif
	}
}

void update_display(ssd1306_priv_t dev)
{
	const ssd1306_bounds_t bounds = {
		width: dev->width,
		height: dev->height,
	};

	ssd1306_send(dev, &bounds);
}

const status_info_t* update_status(ssd1306_priv_t dev, uint8_t index)
{
	status_info_t* status = dev->statuses + index;

	if( status->bitmap == NULL ) {
		return NULL;
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="
	switch( status->state ) {
		case anim_init: {
			LOG_V("state = anim_init");

			status->offset = 0;
			status->ticks = xTaskGetTickCount();
			status->state = anim_wait;
		} // break-through

		case anim_wait: {
			TickType_t ticks = xTaskGetTickCount();
			TickType_t milli = pdTICKS_TO_MS(ticks - status->ticks);

			if( milli < 2500 ) {
				break;
			}

			status->ticks = ticks;
			status->state = anim_move;
		} // break-through

		case anim_move: {
			// LOG_V("state = anim_move, offset = %d", status->offset);

			move_status(dev, status);

			if( status->offset == 0 ) {
				status->ticks = xTaskGetTickCount();
				status->state = anim_wait;
			}
		}
	}
#pragma GCC diagnostic pop

	return status;
}

void move_status(ssd1306_priv_t dev, status_info_t* status)
{
	int16_t offset = --status->offset;

	if( offset < -(int)status->bitmap->w ) {
		offset = status->offset = 0;
	}

	uint8_t* buff = ssd1306_raster((ssd1306_t)dev, status->page);

	memset(buff, 0, dev->width);

	if( offset <= 0 ) {
		uint16_t width = minu(dev->width, status->bitmap->width - -offset);

		memcpy(buff, status->bitmap->image + -offset, width);

		if( width < dev->width ) {
			memcpy(buff + width, status->bitmap->image, dev->width - width);
		}
	} else {
		memcpy(buff + offset, status->bitmap->image, dev->width - offset);
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
