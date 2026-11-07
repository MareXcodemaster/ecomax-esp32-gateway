cmake_minimum_required(VERSION 3.16)
set(PROJECT_VER "0.7.0-beta.56")
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components/microlink/components/wireguard_lwip")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(lazar_stage0)

