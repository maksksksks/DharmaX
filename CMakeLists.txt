# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/esp_driver_gpio/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/esp_driver_i2c/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/esp_driver_spi/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/i2c/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/espressif__ds18b20/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/managed_components/espressif__onewire_bus/include"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(factory)