cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ssd1306)

idf_build_set_property(C_COMPILE_OPTIONS "-fms-extensions" APPEND)

target_add_binary_data(
	${CMAKE_PROJECT_NAME}.elf 
	${CMAKE_BINARY_DIR}/${CONFIG_SSD1306_FONT}.fnt BINARY)

add_custom_command(
	OUTPUT bdf2fnt
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tools
	COMMAND make OUTPUT=${CMAKE_BINARY_DIR} CFLAGS="-O2"
)
add_custom_target(build_bdf2fnt ALL DEPENDS bdf2fnt)
add_custom_command(
	OUTPUT ${CONFIG_SSD1306_FONT}.fnt
	COMMAND ${CMAKE_BINARY_DIR}/bdf2fnt
			../ibmfonts/bdf/${CONFIG_SSD1306_FONT}.bdf
	        -o ${CONFIG_SSD1306_FONT}.fnt
)

target_add_binary_data(
	${CMAKE_PROJECT_NAME}.elf 
	${CMAKE_BINARY_DIR}/${CONFIG_SSD1306_FONT}.fnt BINARY)
