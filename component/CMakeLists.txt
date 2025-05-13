FILE(GLOB_RECURSE cmp_sources src/ssd1306*.c src/os*.c)

if(CONFIG_SSD1306_LOGGING_LEVEL_VERBOSE)
	list(APPEND cmp_sources src/dump.c)
endif()

idf_component_register(
	SRCS ${cmp_sources}
	INCLUDE_DIRS include
	REQUIRES driver esp_timer)

idf_build_set_property(COMPILE_OPTIONS "-fms-extensions" APPEND)

add_custom_command(
	OUTPUT bdf2fnt
	WORKING_DIRECTORY ${COMPONENT_DIR}/tools
	COMMAND make OUTPUT=${CMAKE_BINARY_DIR} CFLAGS="-O2"
)
add_custom_command(
	OUTPUT ${CMAKE_BINARY_DIR}/${CONFIG_SSD1306_FONT}.fnt
	COMMAND ${CMAKE_BINARY_DIR}/bdf2fnt
			${COMPONENT_DIR}/ibmfonts/bdf/${CONFIG_SSD1306_FONT}.bdf
	        -o ${CMAKE_BINARY_DIR}/${CONFIG_SSD1306_FONT}.fnt
)

add_custom_target(build_bdf2fnt ALL DEPENDS bdf2fnt)

target_add_binary_data(
	${COMPONENT_LIB}
	${CMAKE_BINARY_DIR}/${CONFIG_SSD1306_FONT}.fnt BINARY)
