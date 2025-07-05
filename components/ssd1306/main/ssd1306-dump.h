#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

extern const char LOG_DOMAIN[];

void ssd1306_dump_it(const void* data, uint16_t size, const char* format, ...);

#define ssd1306_dump(data, size, format, ...) \
	do { \
		if( LOG_ENABLED(SSD1306_LOG_TRACE) ) { \
			ssd1306_dump_it(data, size, format, ##__VA_ARGS__); \
		} \
	} while( 0 )

#if defined(__cplusplus)
}
#endif
