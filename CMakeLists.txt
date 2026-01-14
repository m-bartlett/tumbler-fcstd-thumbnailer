cmake_minimum_required(VERSION 2.8)
project(tumbler-fcstd-thumbnailer)

find_package(PkgConfig REQUIRED)

# Find the required libraries using pkg-config
# We need tumbler-1, glib, and libgsf-1
pkg_check_modules(DEPS REQUIRED tumbler-1 glib-2.0 gdk-pixbuf-2.0 libgsf-1)

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
add_library(tumbler-fcstd-thumbnailer SHARED
    fcstd-thumbnailer.c
    fcstd-thumbnailer-provider.c
    fcstd-thumbnailer-plugin.c
)

# Explicitly link the libraries
# This ensures the symbol gsf_input_mmap_new is found
target_link_libraries(tumbler-fcstd-thumbnailer
    ${DEPS_LIBRARIES}
)

# Remove the 'lib' prefix so the output is 'tumbler-fcstd-thumbnailer.so'
set_target_properties(tumbler-fcstd-thumbnailer PROPERTIES PREFIX "")
