FILE(GLOB_RECURSE app_sources ssd*.c os*.c)

if(CONFIG_SSD1306_SPLASH)
	list(APPEND app_sources splash.c)
endif()
if(CONFIG_SSD1306_LOGGING_LEVEL_VERBOSE)
	list(APPEND app_sources dump.c)
endif()

list(APPEND app_sources main.c)

idf_component_register(
	SRCS ${app_sources}
	INCLUDE_DIRS .
	REQUIRES driver esp_timer)
