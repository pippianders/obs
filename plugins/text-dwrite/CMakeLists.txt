project(text-dwrite)

set(text-dwrite_HEADERS
	text-dwrite.hpp)

set(text-dwrite_SOURCES
	text-dwrite.cpp
	text-dwrite-main.cpp)

add_library(text-dwrite MODULE
	${text-dwrite_SOURCES}
	${text-dwrite_HEADERS})
target_link_libraries(text-dwrite
	libobs
	windowscodecs
	d2d1
	dwrite)

install_obs_plugin_with_data(text-dwrite data)
