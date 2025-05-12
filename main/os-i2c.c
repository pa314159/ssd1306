#include "sdkconfig.h"

#include "ssd1306.h"
#include "os.h"

#include <driver/gpio.h>
#include <driver/i2c_master.h>

#define SSD1306_I2C_ADDRESS 0x3C
#define SSD1306_I2C_TIMEOUT 100 /* ms */

#if CONFIG_SSD1306_I2C
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

	text_invert: {
		off: 0x0e,
		on: 0x0f,
	},

	connection: {
		type: ssd1306_type_i2c,
		freq: CONFIG_SSD1306_I2C_FREQ,

		rst: CONFIG_SSD1306_I2C_RST_PIN,
		scl: CONFIG_SSD1306_I2C_SCL_PIN,
		sda: CONFIG_SSD1306_I2C_SDA_PIN,

		port: CONFIG_SSD1306_I2C_PORT,
	},
};

ssd1306_init_t ssd1306_create_init()
{
	ssd1306_init_t init = calloc(1, sizeof(ssd1306_init_s));

	memcpy(init, &init_default, sizeof(ssd1306_init_s));

	LOG_I("using default configuration");

	return init;
}
#endif

struct ssd1306_i2c_s {
	struct ssd1306_init_s;

	i2c_master_bus_handle_t bus_handle;
	i2c_master_dev_handle_t dev_handle;
};

// bus_cfg
//  00 00 00 00 16 00 00 00
//  15 00 00 00 04 00 00 00
//  07 00 00 00 00 00 00 00
//  00 00 00 00 01 00 00 00

// dev_cfg
//  00 00 00 00 3c 00 00 00
//  80 1a 06 00 00 00 00 00
//  00 00 00 00

ssd1306_i2c_t ssd1306_i2c_init(ssd1306_init_t init)
{
	// esp_log_level_set("i2c.master", (esp_log_level_t)CONFIG_SSD1306_LOGGING_LEVEL);

	LOG_I("SCL PIN: %d", init->connection.scl);
	LOG_I("SDA PIN: %d", init->connection.sda);
	LOG_I("RST PIN: %d", init->connection.rst);
	LOG_I("Port: %u", init->connection.port);
	LOG_I("Freq: %u kHz", init->connection.freq);

	const i2c_master_bus_config_t bus_cfg = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.i2c_port = init->connection.port,
		.scl_io_num = init->connection.scl,
		.sda_io_num = init->connection.sda,
		.flags.enable_internal_pullup = true,
	};
	ssd1306_dump(&bus_cfg, sizeof(bus_cfg), "I2C bus config");

	const i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = SSD1306_I2C_ADDRESS,
		.scl_speed_hz = 1000 * init->connection.freq,
	};
	ssd1306_dump(&dev_cfg, sizeof(dev_cfg), "I2C dev config");

	ssd1306_i2c_t i2c = calloc(1, sizeof(struct ssd1306_i2c_s));

	memcpy(i2c, init, sizeof(ssd1306_init_s));

	ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &i2c->bus_handle));
	ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c->bus_handle, &dev_cfg, &i2c->dev_handle));

	if( init->connection.rst >= 0 ) {
		gpio_reset_pin(init->connection.rst);
		gpio_set_direction(init->connection.rst, GPIO_MODE_OUTPUT);
		gpio_set_level(init->connection.rst, 0);
		vTaskDelay(pdMS_TO_TICKS(SSD1306_RST_TIMEOUT));
		gpio_set_level(init->connection.rst, 1);
	}

	return i2c;
}

#if 0
void ssd1306_i2c_free(ssd1306_i2c_t i2c)
{
	free(i2c);
}
#endif

void ssd1306_i2c_send(ssd1306_i2c_t device, const uint8_t* data, size_t size)
{
	ESP_ERROR_CHECK(i2c_master_transmit(device->dev_handle, data, size, SSD1306_I2C_TIMEOUT));
}
