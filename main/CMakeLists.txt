FILE(GLOB_RECURSE app_sources *.c)
#FILE(GLOB_RECURSE app_sources ../examples/spaceship-1/main/*.c)

idf_component_register(
	SRCS ${app_sources}
	INCLUDE_DIRS . ../components/ssd1306/main
	REQUIRES ssd1306
)
