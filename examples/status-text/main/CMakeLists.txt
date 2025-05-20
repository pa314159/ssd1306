FILE(GLOB_RECURSE app_sources *.c)

idf_component_register(SRCS "status-text.c" "${app_sources}" INCLUDE_DIRS ".")
