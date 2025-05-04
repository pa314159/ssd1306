#if !defined(__SSD1306_OS_H)
#define __SSD1306_OS_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_check.h>

#define SSD1306_RST_TIMEOUT 100
#define SSD1306_SEM_TIMEOUT 200
#define SSD1306_SEM_TICKS pdMS_TO_TICKS(SSD1306_SEM_TIMEOUT)

#define _countof(x) (sizeof(x)/sizeof(x[0]))

typedef struct ssd1306_i2c_s* ssd1306_i2c_t;
typedef struct ssd1306_spi_s* ssd1306_spi_t;

extern const ssd1306_glyph_t ssd1306_default_font[] asm("_binary_" CONFIG_SSD1306_FONT "_fnt_start");

ssd1306_i2c_t ssd1306_i2c_init(ssd1306_init_t init);
ssd1306_spi_t ssd1306_spi_init(ssd1306_init_t init);

void ssd1306_i2c_send(ssd1306_i2c_t device, const uint8_t* data, size_t size);
void ssd1306_spi_send(ssd1306_spi_t device, const uint8_t* data, size_t size);

void ssd1306_log_level(unsigned level);
void ssd1306_log(unsigned level, const char* function, const char* format, ...);

extern const char* LOG_DOMAIN;

#define LOG_INVOKE(level, format, ...) \
	do { \
		if( _ESP_LOG_ENABLED(level) ) { \
			ssd1306_log(level, __FUNCTION__, format, ##__VA_ARGS__); \
		} \
	} while( 0 )

#define LOG_E(format, ...) LOG_INVOKE(ESP_LOG_ERROR, format, ##__VA_ARGS__)
#define LOG_W(format, ...) LOG_INVOKE(ESP_LOG_WARN, format, ##__VA_ARGS__)
#define LOG_I(format, ...) LOG_INVOKE(ESP_LOG_INFO, format, ##__VA_ARGS__)
#define LOG_D(format, ...) LOG_INVOKE(ESP_LOG_DEBUG, format, ##__VA_ARGS__)
#define LOG_V(format, ...) LOG_INVOKE(ESP_LOG_VERBOSE, format, ##__VA_ARGS__)

#if 0
void ssd1306_i2c_free(ssd1306_i2c_t dev);
void ssd1306_spi_free(ssd1306_spi_t dev);
#endif

#if defined(__cplusplus)
}
#endif

#endif
