# Copyright 2024 - 2025 Khalil Estell and the libhal contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.15)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)
set(PICO_NO_PICOTOOL 1)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
set(CMAKE_COLOR_DIAGNOSTICS ON)

project(libhal-picosdk LANGUAGES CXX C ASM)

pico_sdk_init()

libhal_test_and_make_library(
  LIBRARY_NAME libhal-picosdk

  SOURCES
  src/gpio.cpp
  src/serial.cpp
  src/i2c.cpp
  src/pwm.cpp
  src/adc.cpp
  src/spi.cpp
  src/time.cpp
  src/dwt_counter.cpp
  src/terminate_handler.cpp

  TEST_SOURCES
  ${test_sources}

  PACKAGES
  libhal
  libhal-util
  ring-span-lite
  scope-lite

  LINK_LIBRARIES
  libhal::libhal
  libhal::util
  nonstd::ring-span-lite
  nonstd::scope-lite
  hardware_gpio_headers 
  pico_time_headers 
  pico_stdio_headers 
  hardware_sync_headers
  hardware_i2c_headers
  hardware_pwm_headers
  hardware_adc_headers
  hardware_spi_headers
  hardware_uart_headers
  hardware_timer_headers
  hardware_dma_headers
)

