#include <sdkconfig.h>
#include <ssd1306.h>

#include "ssd1306-int.h"

#include <esp_log.h>
#include <esp_log_color.h>

const char LOG_DOMAIN[] = "SSD1306";

static inline esp_log_level_t to_esp_log_level(ssd1306_log_level_t level)
{
	if( level <= SSD1306_LOG_OFF ) {
		return ESP_LOG_NONE;
	}
	if( level <= SSD1306_LOG_ERROR ) {
		return ESP_LOG_ERROR;
	}
	if( level >= SSD1306_LOG_ALL ) {
		return ESP_LOG_MAX;
	}

	return ESP_LOG_ERROR + (level - SSD1306_LOG_ERROR);
}

void ssd1306_log_level(ssd1306_log_level_t level)
{
	const esp_log_level_t esp_log_level = to_esp_log_level(level);

	esp_log_level_set(LOG_DOMAIN, esp_log_level);
}

bool ssd1306_log_enabled(ssd1306_log_level_t level)
{
	const esp_log_level_t check = to_esp_log_level(level);
	const esp_log_level_t limit = esp_log_level_get(LOG_DOMAIN);

	return check <= limit;
}

void ssd1306_log(ssd1306_log_level_t level, const char* function, int line, const char* format, ...)
{
	static const char PREFIXES[] = { '\0', 'E', 'W', 'I', 'D', 'V' };
	static const char* COLORS[] = {
		"",
#if CONFIG_LOG_COLORS
		LOG_ANSI_COLOR_REGULAR(LOG_ANSI_COLOR_RED),
		LOG_ANSI_COLOR_REGULAR(LOG_ANSI_COLOR_YELLOW),
		LOG_ANSI_COLOR_REGULAR(LOG_ANSI_COLOR_GREEN),
		LOG_ANSI_COLOR_REGULAR(LOG_ANSI_COLOR_WHITE),
		LOG_ANSI_COLOR_ITALIC(LOG_ANSI_COLOR_WHITE),
#else
		"",
		"",
		"",
		"",
		LOG_ANSI_COLOR_ITALIC(LOG_ANSI_COLOR_WHITE),
#endif
	};

	const esp_log_level_t esp_log_level = to_esp_log_level(level);

	ABORT_IF(esp_log_level >= _countof(PREFIXES), "level %d is exceeding prefixes count", esp_log_level);
	ABORT_IF(esp_log_level >= _countof(COLORS), "level %d is exceeding colors count", esp_log_level);

	const char* task_name = pcTaskGetName(NULL);

	esp_log_write(esp_log_level, LOG_DOMAIN, "%s%c [%s] (%s) %s[%s:%d]: ", 
		COLORS[esp_log_level], PREFIXES[esp_log_level],  esp_log_system_timestamp(),
		task_name, LOG_DOMAIN, function, line);

    va_list list;

    va_start(list, format);
    esp_log_writev(esp_log_level, LOG_DOMAIN, format, list);
    va_end(list);

	esp_log_write(esp_log_level, LOG_DOMAIN, "\n" LOG_ANSI_COLOR_RESET);
}
