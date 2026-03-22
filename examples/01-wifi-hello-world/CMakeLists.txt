cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "精简"构建。仅包含最小组件集、main 组件及其依赖项。
idf_build_set_property(MINIMAL_BUILD ON)
project(esp32_wifi_demo)