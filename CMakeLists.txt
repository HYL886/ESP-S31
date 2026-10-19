cmake_minimum_required(VERSION 3.22)

# The board is fixed for this learning project.
set(IDF_TARGET esp32s31)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(s31_display)
