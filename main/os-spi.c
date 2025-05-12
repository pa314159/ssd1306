#include "sdkconfig.h"

#include "ssd1306.h"
#include "os.h"

#include <driver/gpio.h>
#include <driver/spi_master.h>

#if CONFIG_SSD1306_SPI
static const ssd1306_init_s init_default = {
#if CONFIG_SSD1306_FLIP
	flip: true,
#endif
#if CONFIG_SSD1306_INVERT
	invert: true,
#endif

	width: CONFIG_SSD1306_WIDTH,
	height: CONFIG_SSD1306_HEIGHT,

	font: ssd1306_default_font,

	connection: {
		type: ssd1306_type_spi,
		freq: CONFIG_SSD1306_SPI_FREQ,

		rst: CONFIG_SSD1306_SPI_RST_PIN,
		mosi: CONFIG_SSD1306_SPI_MOSI_PIN,
		sclk: CONFIG_SSD1306_SPI_SCLK_PIN,
		cs: CONFIG_SSD1306_SPI_CS_PIN,
		dc: CONFIG_SSD1306_SPI_DC_PIN,

		host: CONFIG_SSD1306_SPI_HOST,
	},
};

ssd1306_init_t ssd1306_create_init()
{
	ssd1306_init_t init = calloc(1, sizeof(init_default));

	memcpy(init, &init_default, sizeof(init_default));

	LOG_I("using default configuration");

	return init;
}
#endif

struct ssd1306_spi_s {
	struct ssd1306_init_s;
	spi_device_handle_t handle;
	spi_transaction_t transaction;
};

ssd1306_spi_t ssd1306_spi_init(ssd1306_init_t init)
{
	esp_log_level_set("i2c.master", (esp_log_level_t)CONFIG_SSD1306_LOGGING_LEVEL);

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

	const spi_bus_config_t spi_bus = {
		.mosi_io_num = init->connection.mosi,
		.sclk_io_num = init->connection.sclk,

		.data1_io_num = -1,
		.data2_io_num = -1,
		.data3_io_num = -1,
		.data4_io_num = -1,
		.data5_io_num = -1,
		.data7_io_num = -1,
	};
	const spi_device_interface_config_t spi_dev = {
		.clock_speed_hz = 1000000 * init->connection.freq,
		.spics_io_num = init->connection.cs,
		.queue_size = 1,
	};

	ssd1306_spi_t spi = calloc(1, sizeof(struct ssd1306_spi_s));

	memcpy(spi, init, sizeof(ssd1306_init_s));

	ESP_ERROR_CHECK(spi_bus_initialize(init->connection.host, &spi_bus, SPI_DMA_CH_AUTO));
	ESP_ERROR_CHECK(spi_bus_add_device(init->connection.host, &spi_dev, &spi->handle));

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

void ssd1306_spi_send(ssd1306_spi_t device, const uint8_t* data, size_t size)
{
}
