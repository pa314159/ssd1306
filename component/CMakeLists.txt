FILE(GLOB_RECURSE cmp_sources main/*.c)

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

if(CONFIG_SSD1306_FONT_CUSTOM)
	add_custom_command(
		OUTPUT ${CMAKE_BINARY_DIR}/${CONFIG_SSD1306_FONT_NAME}.fnt
		COMMAND ${CMAKE_BINARY_DIR}/bdf2fnt
			${CMAKE_SOURCE_DIR}/${CONFIG_SSD1306_FONT_NAME}.bdf
			-o ${CMAKE_BINARY_DIR}/${CONFIG_SSD1306_FONT_NAME}.fnt)
else()
	add_custom_command(
		OUTPUT ${CMAKE_BINARY_DIR}/${CONFIG_SSD1306_FONT_NAME}.fnt
		COMMAND ${CMAKE_BINARY_DIR}/bdf2fnt
			${COMPONENT_DIR}/ibmfonts/bdf/${CONFIG_SSD1306_FONT_NAME}.bdf
			-o ${CMAKE_BINARY_DIR}/${CONFIG_SSD1306_FONT_NAME}.fnt)
endif()

add_custom_target(build_bdf2fnt ALL DEPENDS bdf2fnt)

target_add_binary_data(
	${COMPONENT_LIB}
	${CMAKE_BINARY_DIR}/${CONFIG_SSD1306_FONT_NAME}.fnt BINARY)
