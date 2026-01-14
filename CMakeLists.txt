cmake_minimum_required(VERSION 2.8)
project(tumbler-dir-thumbnailer)

find_package(PkgConfig REQUIRED)

# Find the required libraries using pkg-config
# We need tumbler-1, glib, gdk-pixbuf, and gio
pkg_check_modules(DEPS REQUIRED tumbler-1 glib-2.0 gdk-pixbuf-2.0 gio-2.0)

# Add the include directories found by pkg-config
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${DEPS_INCLUDE_DIRS}
)

# Add the library directories found by pkg-config
link_directories(
    ${DEPS_LIBRARY_DIRS}
)

# Define the shared library
add_library(tumbler-dir-thumbnailer SHARED
    dir-thumbnailer.c
    dir-thumbnailer-provider.c
    dir-thumbnailer-plugin.c
)

# Explicitly link the libraries
target_link_libraries(tumbler-dir-thumbnailer
    ${DEPS_LIBRARIES}
)

# Remove the 'lib' prefix so the output is 'tumbler-dir-thumbnailer.so'
set_target_properties(tumbler-dir-thumbnailer PROPERTIES PREFIX "")
