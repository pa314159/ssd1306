#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-defs.h"
#include "ssd1306-int.h"

#include <assert.h>
#include <ctype.h>

static void ssd1306_init_private(ssd1306_int_t dev, const ssd1306_init_t init, uint8_t pages);
static void ssd1306_init_status(ssd1306_int_t dev, ssd1306_status_t status);
static void ssd1306_init_screen(ssd1306_int_t dev, const ssd1306_init_t ini);

ssd1306_init_t ssd1306_create_init(ssd1306_interface_t type)
{
	if( type == ssd1306_interface_any ) {
		type = CONFIG_SSD1306_INTERFACE;	
	}

	switch( type ) {
		case ssd1306_interface_iic: {
			return ssd1306_iic_create_init();
		}
		
		case ssd1306_interface_spi: {
			return ssd1306_spi_create_init();
		}

		default:
			ABORT_IF(false, "unreachable code");
			return NULL;
	}
}

ssd1306_t ssd1306_init(ssd1306_init_t init)
{
	ssd1306_log_set_level(CONFIG_SSD1306_LOGGING_LEVEL);

	LOG_E("LOG_E is active");
	LOG_W("LOG_W is active");
	LOG_I("LOG_I is active");
	LOG_D("LOG_D is active");
	LOG_V("LOG_V is active");

	if( init == NULL ) {
		init = ssd1306_create_init(ssd1306_interface_any);
	}

	// allocate additional bytes for internal buffer and raster
	const uint8_t pages = 4 * ((int)init->panel + 1);
	const size_t total = sizeof(ssd1306_int_s) + pages * CONFIG_SSD1306_WIDTH;

	ssd1306_int_t dev = calloc(1, total);

	ABORT_IF(dev == NULL, "cannot allocate memory for ssd1306_t");

	ssd1306_init_private(dev, init, pages);
	ssd1306_init_status(dev, ssd1306_status_0);
	ssd1306_init_status(dev, ssd1306_status_1);

	LOG_I("Allocated %u bytes at %p", total, dev);

	ssd1306_dump(dev, sizeof(ssd1306_int_s), "Initialisation structure");

	LOG_I("Initialising device");
	LOG_I("    Size: %ux%u (%u pages)", dev->w, dev->h, dev->pages);
	LOG_I("    Bounds: [%+d%+d, %+d%+d]", dev->x0, dev->y0, dev->x1, dev->y1);
	LOG_I("    Flip: %s", dev->flip ? "yes" : "no");
	LOG_I("    Contrast: %d", init->contrast);
	LOG_I("    Invert: %d", init->invert);

	switch( init->connection.type ) {
		case ssd1306_interface_iic:
			LOG_I("    Type: IIC");
			dev->i2c = ssd1306_iic_init(init);
		break;
		case ssd1306_interface_spi:
			LOG_I("    Type: SPI");
			dev->spi = ssd1306_spi_init(init);
		break;

		default:
			ABORT_IF(false, "unreachable code");
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

	if( init->free ) {
		free(init);
	}

#if CONFIG_SSD1306_OPTIMIZE || !CONFIG_SSD1306_SPLASH
	ssd1306_update((ssd1306_t)dev, NULL);
#endif

#if CONFIG_SSD1306_SPLASH > 0
	ssd1306_draw_c((ssd1306_t)dev, splash_bmp);
	vTaskDelay(pdMS_TO_TICKS(CONFIG_SSD1306_SPLASH));
#endif

	return (ssd1306_t)dev;
}

#if __SSD1306_FREE
void ssd1306_free(ssd1306_t device)
{
	if( device == NULL ) {
		return;
	}

	ssd1306_int_t const dev = (ssd1306_int_t)device;

	switch( dev->connection.type ) {
		case ssd1306_interface_i2c:
			ssd1306_iic_free(dev->i2c);
		break;
		case ssd1306_interface_spi:
			ssd1306_spi_free(dev->spi);
		break;
	}

	free(dev->data)
	free(dev);
}
#endif

void ssd1306_init_private(ssd1306_int_t dev, const ssd1306_init_t ini, uint8_t pages)
{
	dev->flip = ini->flip;
	dev->x1 = dev->size.w = CONFIG_SSD1306_WIDTH;
	dev->y1 = dev->size.h = pages * SSD1306_PAGE_HEIGHT;
	dev->pages = pages;
	dev->font = ini->font;
	
	memcpy((void*)&dev->connection, &ini->connection, sizeof(dev->connection));
}

void ssd1306_init_status(ssd1306_int_t dev, ssd1306_status_t status)
{
	uint16_t index = ssd1306_status_index(dev, status);
	ssd1306_bounds_t* bounds = (ssd1306_bounds_t*)&dev->statuses[index];

	bounds->x0 = 0;
	bounds->y0 = dev->flip ? dev->h - SSD1306_TEXT_HEIGHT * (2-index) : SSD1306_TEXT_HEIGHT * index;
	bounds->x1 = dev->w;
	bounds->y1 = bounds->y0 + SSD1306_TEXT_HEIGHT;
}

void ssd1306_init_screen(ssd1306_int_t dev, const ssd1306_init_t ini)
{
	const uint8_t data[] = {
		OLED_CMD0(DISPLAY_OFF),

		OLED_CMD2(SET_MUX_RATIO, dev->h-1),
		OLED_CMD1(SET_SEGMENT_REMAP, dev->flip ? 0x00 : 0x01),
		OLED_CMD1(SET_COM_SCAN_MODE, dev->flip ? 0x00 : 0x08),
		OLED_CMD2(SET_DISPLAY_CLK_DIV, 0x80),

		OLED_CMD2(SET_COM_PIN_MAP, dev->h == 64 ? 0x12:  0x02),
		OLED_CMD2(SET_VCOMH_DESELCT, 0x40),
		OLED_CMD2(SET_CHARGE_PUMP, 0x14),
		OLED_CMD2(SET_PRECHARGE, 0xf1),

		OLED_CMD0(DISPLAY_RAM),
		OLED_CMD2(SET_DISPLAY_OFFSET, 0x00),
		OLED_CMD1(SET_DISPLAY_START_LINE, 0x00),

		OLED_CMD2(SET_MEMORY_ADDR_MODE, OLED_HORI_ADDR_MODE),

#if !CONFIG_SSD1306_OPTIMIZE
		OLED_CMD3(SET_COLUMN_RANGE, 0, dev->w-1),
		OLED_CMD3(SET_PAGE_RANGE, 0, dev->pages-1),
#endif

		OLED_CMD0(DEACTIVE_SCROLL),
		OLED_CMD2(SET_CONTRAST, ini->contrast),
		OLED_CMD1(DISPLAY_NORMAL, ini->invert ? 0x01 : 0x00),

		OLED_CMD0(DISPLAY_ON),

	};

	ssd1306_send_buff(dev, OLED_CTL_COMMAND, data, _countof(data));
}
