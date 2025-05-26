#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-int.h"
#include "ssd1306-defs.h"

#include <esp_timer.h>

#define SCREEN_SCROLL_FPS   25
#define SCREEN_SCROLL_TICKS pdMS_TO_TICKS(1000/SCREEN_SCROLL_FPS)

static void update_region(ssd1306_int_t dev, const ssd1306_bounds_t* bounds);

static const status_info_t* update_status(ssd1306_int_t dev, uint8_t index, ssd1306_bounds_t* bounds);
static void move_status(ssd1306_int_t dev, status_info_t* status, ssd1306_bounds_t* bounds);

inline bool is_move(const status_info_t* status)
{
	return status && status->state == anim_move;
}

void ssd1306_task(ssd1306_int_t dev)
{
	LOG_I("Starting update task for device %u", dev->id);

	dev->active = true;

	TickType_t delay = portMAX_DELAY;

	while( dev->active ) {
		ssd1306_bounds_t bounds = dev->bounds;

#if CONFIG_SSD1306_OPTIMIZE
		bool notified = xQueueReceive(dev->queue, &bounds, delay);
#else
		bool notified = ulTaskNotifyTake(pdTRUE, delay);
#endif
		if( xSemaphoreTake(dev->mutex, portMAX_DELAY) ) {
			const status_info_t* s0 = update_status(dev, 0, &bounds);
			const status_info_t* s1 = update_status(dev, 1, &bounds);

			delay = (s0 || s1) ? SCREEN_SCROLL_TICKS : portMAX_DELAY;

			if( notified || is_move(s0) || is_move(s1) ) {
				update_region(dev, &bounds);
			}

			xSemaphoreGive(dev->mutex);
		}
	}
}

void update_region(ssd1306_int_t dev, const ssd1306_bounds_t* bounds)
{
#if CONFIG_SSD1306_OPTIMIZE
	const uint16_t x0 = bounds->x0;
	const uint16_t x1 = bounds->x1;
	const uint16_t p0 = bounds->y0 / 8;
	const uint16_t p1 = bounds->y1 / 8 + (bounds->y1 % 8 ? 1 : 0);

	LOG_D("x0 = %u, x1 = %u, p0 = %u, p1 = %u", x0, x1, p0, p1);
#endif

	const uint64_t start = esp_timer_get_time();

#if CONFIG_SSD1306_OPTIMIZE
	const uint8_t data[] = {
		OLED_CMD_SET_COLUMN_RANGE, x0, x1 - 1,
		OLED_CMD_SET_PAGE_RANGE, p0, p1 - 1,
	};

	ssd1306_send_buff(dev, OLED_CTL_COMMAND, data, _countof(data));

	for( uint16_t p = p0; p < p1; p++ ) {
		const uint8_t* buff = ssd1306_raster((ssd1306_t)dev, p);

		ssd1306_send_buff(dev, OLED_CTL_DATA, buff + x0, x1 - x0);
	}

	LOG_D("region updated in %u \u03BCs", esp_timer_get_time()-start);
#else
	ssd1306_send_buff(dev, OLED_CTL_DATA, dev->buff, dev->width * dev->pages);

	LOG_D("full region updated in %u \u03BCs", esp_timer_get_time()-start);
#endif
}

const status_info_t* update_status(ssd1306_int_t dev, uint8_t index, ssd1306_bounds_t* bounds)
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

			move_status(dev, status, bounds);

			if( status->offset == 0 ) {
				status->ticks = xTaskGetTickCount();
				status->state = anim_wait;
			}
		}
	}
#pragma GCC diagnostic pop

	return status;
}

void move_status(ssd1306_int_t dev, status_info_t* status, ssd1306_bounds_t* bounds)
{
	int16_t offset = --status->offset;

	if( offset < -(int)status->bitmap->w ) {
		offset = status->offset = 0;
	}

	uint8_t* buff = ssd1306_raster((ssd1306_t)dev, status->y0 / 8);

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

	ssd1306_bounds_union(bounds, (ssd1306_bounds_t*)status);
}

void ssd1306_send_buff(ssd1306_int_t dev, uint8_t ctl, const uint8_t* data, uint16_t size)
{
	LOG_V("data = %p, size = %u", data, size);

	switch( dev->connection.type ) {
		case ssd1306_interface_iic:
			ssd1306_iic_send(dev, ctl, data, size);
		break;
		case ssd1306_interface_spi:
			ssd1306_spi_send(dev, ctl, data, size);
		break;

		default:
			ABORT_IF(false, "unreachable code");
	}
}
