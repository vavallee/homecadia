# Build scaffolding follows the esp-matter icd_app example (v1.6).
cmake_minimum_required(VERSION 3.5)

if(NOT DEFINED ENV{ESP_MATTER_PATH})
    message(FATAL_ERROR "Please set ESP_MATTER_PATH to the path of the esp-matter repo (source esp-matter/export.sh)")
endif()

# PROJECT_VER -> Matter SoftwareVersionString, PROJECT_VER_NUMBER -> the
# SoftwareVersion integer OTA compares. Both come from the git tag so they
# cannot drift; see version.cmake for the encoding and why dev builds do not
# move the number.
include(${CMAKE_CURRENT_LIST_DIR}/version.cmake)
homecadia_derive_version(PROJECT_VER PROJECT_VER_NUMBER)
message(STATUS "homecadia version: ${PROJECT_VER} (SoftwareVersion=${PROJECT_VER_NUMBER})")

set(ESP_MATTER_PATH $ENV{ESP_MATTER_PATH})
set(MATTER_SDK_PATH ${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip)

# This should be done before using the IDF_TARGET variable.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/../components"
    "${ESP_MATTER_PATH}/examples/common"
    "${MATTER_SDK_PATH}/config/esp32/components"
    "${ESP_MATTER_PATH}/components")

project(homecadia-sensor-01)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H;-DCONFIG_OPENTHREAD_CONFIG_SRP_CLIENT_BUFFERS_MAX_SERVICES=5;-Wno-overloaded-virtual" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os;-DCONFIG_OPENTHREAD_CONFIG_SRP_CLIENT_BUFFERS_MAX_SERVICES=5" APPEND)
# For RISCV chips, project_include.cmake sets -Wno-format, but does not clear various
# flags that depend on -Wformat
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security" APPEND)
