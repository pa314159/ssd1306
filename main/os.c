#include <sdkconfig.h>

#include "ssd1306.h"
#include "os.h"

#include <esp_log_color.h>

const char* LOG_DOMAIN = "SSD1306";

void ssd1306_log_level(unsigned level)
{
	esp_log_level_set(LOG_DOMAIN, (esp_log_level_t)level);
}

void ssd1306_log(unsigned level, const char* function, int line, const char* format, ...)
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

	assert(level < _countof(PREFIXES));
	assert(level < _countof(COLORS));

	esp_log_write(level, LOG_DOMAIN, "%s%c (%s) %s[%s:%d]: ", 
		COLORS[level], PREFIXES[level],  esp_log_system_timestamp(),
		LOG_DOMAIN, function, line);

    va_list list;

    va_start(list, format);
    esp_log_writev(level, LOG_DOMAIN, format, list);
    va_end(list);

	esp_log_write(level, LOG_DOMAIN, "\n" LOG_ANSI_COLOR_RESET);
}
