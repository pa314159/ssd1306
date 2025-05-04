#include "sdkconfig.h"
#include "ssd1306.h"
#include "ssd1306-priv.h"
#include "ssd1306-defs.h"

#include <esp_timer.h>

// static void ssd1306_send(ssd1306_t device);

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

void ssd1306_task(ssd1306_t device)
{
	LOG_I("Starting ssd1306 task");

	device->active = true;

	while( device->active ) {
		LOG_D("Waiting for event");

		if( ulTaskNotifyTake(pdTRUE, portMAX_DELAY) ) {
			LOG_D("Got event");

			if( xSemaphoreTake(device->mutex, portMAX_DELAY) ) {
				ssd1306_send(device);

				xSemaphoreGive(device->mutex);
			}
		}
	}
}

void ssd1306_send(ssd1306_t device)
{
	uint64_t start = esp_timer_get_time();
	
	const uint8_t data[] = {
		OLED_CTL_BYTE_CMD_STREAM,
		OLED_CMD_SET_MEMORY_ADDR_MODE | OLED_CMD_SET_HORI_ADDR_MODE,
		OLED_CMD_SET_COLUMN_RANGE, 0, device->width - 1,
		OLED_CMD_SET_PAGE_RANGE, 0, device->pages - 1,
	};

	ssd1306_send_data(device, data, _countof(data));
	ssd1306_send_data(device, raster_page_head(device, 0), device->width * device->pages);

	LOG_I("ended after %uμs", esp_timer_get_time()-start);
}
