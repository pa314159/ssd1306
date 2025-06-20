#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

extern const char LOG_DOMAIN[];

void ssd1306_dump_it(const void* data, uint16_t size, const char* format, ...);

#define ssd1306_dump(data, size, format, ...) \
	do { \
		if( _ESP_LOG_ENABLED(ESP_LOG_VERBOSE) && (esp_log_level_get(LOG_DOMAIN) >= ESP_LOG_VERBOSE) ) { \
			ssd1306_dump_it(data, size, format, ##__VA_ARGS__); \
		} \
	} while( 0 )

#if defined(__cplusplus)
}
#endif
