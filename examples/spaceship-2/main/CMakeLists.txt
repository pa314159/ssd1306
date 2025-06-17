FILE(GLOB_RECURSE app_sources *.c)

idf_component_register(SRCS "spaceship-2.c" "${app_sources}" INCLUDE_DIRS ".")
