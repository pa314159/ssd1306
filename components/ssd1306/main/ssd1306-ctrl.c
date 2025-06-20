#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-int.h"
#include "ssd1306-defs.h"

void ssd1306_auto_update(ssd1306_t device, bool on)
{
	ABORT_IF_NULL(device);

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	dev->defer_update += on ? -1 : +1;

	if( dev->defer_update == 0 ) {
		ssd1306_update(device, NULL);
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
