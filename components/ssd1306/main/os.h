#if !defined(__SSD1306_OS_H)
#define __SSD1306_OS_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <esp_check.h>

#include "ssd1306-dump.h"

#define SSD1306_RST_TIMEOUT 100
#define SSD1306_SEM_TIMEOUT 500
#define SSD1306_SEM_TICKS pdMS_TO_TICKS(SSD1306_SEM_TIMEOUT)

typedef struct ssd1306_int_s* ssd1306_int_t;
typedef struct ssd1306_iic_s* ssd1306_iic_t;
typedef struct ssd1306_spi_s* ssd1306_spi_t;

extern const ssd1306_glyph_t ssd1306_default_font[] asm("_binary_" CONFIG_SSD1306_FONT_NAME "_fnt_start");

ssd1306_init_t ssd1306_iic_create_init();
ssd1306_init_t ssd1306_spi_create_init();

ssd1306_iic_t ssd1306_iic_init(ssd1306_init_t init);
ssd1306_spi_t ssd1306_spi_init(ssd1306_init_t init);

void ssd1306_iic_send(ssd1306_int_t dev, uint8_t ctl, const uint8_t* data, uint16_t size);
void ssd1306_spi_send(ssd1306_int_t dev, uint8_t ctl, const uint8_t* data, uint16_t size);

#define ABORT_IF(condition, format, ...) \
	do { \
		if( condition ) { \
			LOG_INVOKE(ESP_LOG_ERROR, format, ##__VA_ARGS__); \
			esp_system_abort("execution stopped due to failed condition: '" #condition "'"); \
		} \
	} while( 0 )
#define ABORT_IF_NULL(param) ABORT_IF(param == NULL, "value of '" #param "' is NULL")
#define ABORT_IF_NOT_NULL(param) ABORT_IF(param != NULL, "value of '" #param "' is NOT NULL")

#if 0
void ssd1306_iic_free(ssd1306_iic_t dev);
void ssd1306_spi_free(ssd1306_spi_t dev);
#endif

#if defined(__cplusplus)
}
#endif

#endif
