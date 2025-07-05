#if !defined(__SSD1306_LOG_H)
#define __SSD1306_LOG_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>

typedef enum ssd1306_log_level_t
{
	SSD1306_LOG_OFF,

	SSD1306_LOG_FATAL,
	SSD1306_LOG_ERROR,
	SSD1306_LOG_WARN,
	SSD1306_LOG_INFO,
	SSD1306_LOG_DEBUG,
	SSD1306_LOG_TRACE,

	SSD1306_LOG_ALL,
} ssd1306_log_level_t;

void ssd1306_log(ssd1306_log_level_t level, const char* function, int line, const char* format, ...);
void ssd1306_log_level(ssd1306_log_level_t level);
bool ssd1306_log_enabled(ssd1306_log_level_t level);

#define LOG_ENABLED(level) \
	OS_LOG_ENABLED(level) && ssd1306_log_enabled(level)
#define LOG_INVOKE(level, format, ...) \
	do { \
		if( LOG_ENABLED(level) ) { \
			ssd1306_log(level, __FUNCTION__, __LINE__, format, ##__VA_ARGS__); \
		} \
	} while( 0 )

#define LOG_E(format, ...) LOG_INVOKE(SSD1306_LOG_ERROR, format, ##__VA_ARGS__)
#define LOG_W(format, ...) LOG_INVOKE(SSD1306_LOG_WARN, format, ##__VA_ARGS__)
#define LOG_I(format, ...) LOG_INVOKE(SSD1306_LOG_INFO, format, ##__VA_ARGS__)
#define LOG_D(format, ...) LOG_INVOKE(SSD1306_LOG_DEBUG, format, ##__VA_ARGS__)
#define LOG_T(format, ...) LOG_INVOKE(SSD1306_LOG_TRACE, format, ##__VA_ARGS__)

#define LOG_BOUNDS_W(message, bounds) \
	LOG_W(message ": [%+d%+d, %+d%+d)", (bounds)->x0, (bounds)->y0, (bounds)->x1, (bounds)->y1)
#define LOG_BOUNDS_I(message, bounds) \
	LOG_I(message ": [%+d%+d, %+d%+d)", (bounds)->x0, (bounds)->y0, (bounds)->x1, (bounds)->y1)
#define LOG_BOUNDS_D(message, bounds) \
	LOG_D(message ": [%+d%+d, %+d%+d)", (bounds)->x0, (bounds)->y0, (bounds)->x1, (bounds)->y1)
#define LOG_BOUNDS_T(message, bounds) \
	LOG_T(message ": [%+d%+d, %+d%+d)", (bounds)->x0, (bounds)->y0, (bounds)->x1, (bounds)->y1)

#if __XTENSA__
#include <esp_log.h>
#define OS_LOG_ENABLED(level) _ESP_LOG_ENABLED(level)
#else
#warn undefined
#define OS_LOG_ENABLED(level) (0)
#endif

#if defined(__cplusplus)
}
#endif

#endif
