#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-int.h"
#include "ssd1306-defs.h"

#include <driver/gpio.h>
#include <driver/spi_master.h>

static const ssd1306_init_s init_default = {
#if CONFIG_SSD1306_FLIP
	flip: true,
#endif
#if CONFIG_SSD1306_INVERT
	invert: true,
#endif
	free: true,
	panel: (ssd1306_panel_t)CONFIG_SSD1306_PANEL_TYPE,

	contrast: CONFIG_SSD1306_CONTRAST,

	font: ssd1306_default_font,

	connection: {
		type: ssd1306_interface_spi,
		freq: CONFIG_SSD1306_SPI_FREQ,

		rst: CONFIG_SSD1306_SPI_RST_PIN,
		mosi: CONFIG_SSD1306_SPI_MOSI_PIN,
		sclk: CONFIG_SSD1306_SPI_SCLK_PIN,
		cs: CONFIG_SSD1306_SPI_CS_PIN,
		dc: CONFIG_SSD1306_SPI_DC_PIN,

		host: CONFIG_SSD1306_SPI_HOST,
	},
};

ssd1306_init_t ssd1306_spi_create_init()
{
	ssd1306_init_t init = malloc(sizeof(init_default));

	ABORT_IF(init == NULL, "cannot allocate memory for ssd1306_init_t");

	memcpy(init, &init_default, sizeof(init_default));

	LOG_I("using default configuration");

	return init;
}

struct ssd1306_spi_s {
	spi_device_handle_t handle;
	spi_transaction_t transaction;
};

ssd1306_spi_t ssd1306_spi_init(ssd1306_init_t init)
{
	esp_log_level_set("spi_master", (esp_log_level_t)CONFIG_SSD1306_LOGGING_LEVEL);

	LOG_I("MOSI PIN: %d", init->connection.mosi);
	LOG_I("SCLK PIN: %d", init->connection.sclk);
	LOG_I("CS   PIN: %d", init->connection.cs);
	LOG_I("DC   PIN: %d", init->connection.dc);
	LOG_I("RST  PIN: %d", init->connection.rst);
	LOG_I("Host: %u", init->connection.host);
	LOG_I("Freq: %u kHz", init->connection.freq);

	gpio_reset_pin(init->connection.cs);
	gpio_set_direction(init->connection.cs, GPIO_MODE_OUTPUT);
	gpio_set_level(init->connection.cs, 0);

	gpio_reset_pin(init->connection.dc);
	gpio_set_direction(init->connection.dc, GPIO_MODE_OUTPUT);
	gpio_set_level(init->connection.dc, 0);

	const spi_bus_config_t bus_cfg = {
		.mosi_io_num = init->connection.mosi,
		.sclk_io_num = init->connection.sclk,

		.data1_io_num = -1,
		.data2_io_num = -1,
		.data3_io_num = -1,
		.data4_io_num = -1,
		.data5_io_num = -1,
		.data7_io_num = -1,
	};
	ssd1306_dump(&bus_cfg, sizeof(bus_cfg), "SPI bus config");

	const spi_device_interface_config_t dev_cfg = {
		.clock_speed_hz = 1000000 * init->connection.freq,
		.spics_io_num = init->connection.cs,
		.queue_size = 1,
	};
	ssd1306_dump(&dev_cfg, sizeof(dev_cfg), "SPI dev config");

	ssd1306_spi_t spi = malloc(sizeof(struct ssd1306_spi_s));

	ABORT_IF(spi == NULL, "cannot allocate memory for ssd1306_spi_t");

	ESP_ERROR_CHECK(spi_bus_initialize(init->connection.host, &bus_cfg, SPI_DMA_CH_AUTO));
	ESP_ERROR_CHECK(spi_bus_add_device(init->connection.host, &dev_cfg, &spi->handle));

	if( init->connection.rst >= 0 ) {
		gpio_reset_pin(init->connection.rst);
		gpio_set_direction(init->connection.rst, GPIO_MODE_OUTPUT);
		gpio_set_level(init->connection.rst, 0);
		vTaskDelay(pdMS_TO_TICKS(SSD1306_RST_TIMEOUT));
		gpio_set_level(init->connection.rst, 1);
	}

	return spi;
}

#if 0
void ssd1306_spi_free(ssd1306_spi_t spi)
{
	free(spi);
}
#endif

#define SPI_COMM_MODE 0
#define SPI_DATA_MODE 1

void ssd1306_spi_send(ssd1306_int_t dev, uint8_t ctl, const uint8_t* data, uint16_t size)
{
	switch( ctl ) {
		case OLED_CTL_DATA: 
			ssd1306_dump(data, size, "SPI buffer type = %u, size = %u, SPI_DATA_MODE", ctl, size);

			gpio_set_level(dev->connection.cs, SPI_DATA_MODE);

			LOG_V("gpio %d set to %d", dev->connection.cs, SPI_DATA_MODE);
			break;

		case OLED_CTL_COMMAND:
			ssd1306_dump(data, size, "SPI buffer type = %u, size = %u, SPI_COMM_MODE", ctl, size);

			gpio_set_level(dev->connection.cs, SPI_COMM_MODE);

			LOG_V("gpio %d set to %d", dev->connection.cs, SPI_COMM_MODE);
			break;

		default: {
			LOG_E("Unkown SPI buffer type %u", ctl);
			abort();
		}
	}

	spi_transaction_t tx = {
		length: 8 * size,
		tx_buffer: data,
	};

	ESP_ERROR_CHECK(spi_device_transmit(dev->spi->handle, &tx));
}
