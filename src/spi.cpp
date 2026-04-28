// Copyright 2026 - Shin Umeda & LibHAL contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <algorithm>

#include <libhal-util/steady_clock.hpp>
#include <libhal/error.hpp>

#include <hardware/gpio.h>
#include <hardware/spi.h>
#include <pico/time.h>

#include "libhal-arm-mcu/rp/spi.hpp"

namespace {
auto get_bus(hal::u8 busnum)
{
  switch (busnum) {
    case 0:
      return spi0;
    case 1:
      return spi1;
    default:
      hal::safe_throw(hal::io_error(nullptr));
  }
}
}  // namespace
namespace hal::rp {

inline namespace v4 {
// This constructor is private and therefore does not represent a user hazard
spi::spi(u8 bus, u8 tx, u8 rx, u8 sck, spi::settings const& s)  // NOLINT
  : m_bus(bus)
  , m_tx(tx)
  , m_rx(rx)
  , m_sck(sck)
{
  driver_configure(s);
  gpio_set_function(tx, GPIO_FUNC_SPI);
  gpio_set_function(rx, GPIO_FUNC_SPI);
  gpio_set_function(sck, GPIO_FUNC_SPI);
}

void spi::driver_configure(spi::settings const& s)
{
  spi_cpol_t polarity = s.clock_polarity ? SPI_CPOL_1 : SPI_CPOL_0;
  spi_cpha_t phase = s.clock_phase ? SPI_CPHA_1 : SPI_CPHA_0;

  spi_init(get_bus(m_bus), static_cast<unsigned int>(s.clock_rate));
  spi_set_format(get_bus(m_bus), 8, polarity, phase, SPI_MSB_FIRST);
}

void spi::driver_transfer(std::span<byte const> out,
                          std::span<byte> in,
                          byte filler)
{
  auto out_size = out.size_bytes();
  auto in_size = in.size_bytes();
  auto size = std::min(out_size, in_size);
  spi_write_read_blocking(get_bus(m_bus), out.data(), in.data(), size);
  if (out_size > in_size) {
    spi_write_blocking(
      get_bus(m_bus), out.data() + in_size, out_size - in_size);
  } else if (in_size > out_size) {
    spi_read_blocking(
      get_bus(m_bus), filler, in.data() + out_size, in_size - out_size);
  }
  // Does this wreck performance? probably
  // Is this a bad idea? probably
  // Is it necessary? Absolutely. See below for implementation details
  sleep_us(10);
}

spi::~spi()
{
  gpio_deinit(m_tx);
  gpio_deinit(m_rx);
  gpio_deinit(m_sck);
  spi_deinit(get_bus(m_bus));
}

}  // namespace v4

namespace v5 {

spi_bus<void>::spi_bus(u8 bus, u8 tx, u8 rx, u8 sck)  // NOLINT
  : m_bus(bus)
  , m_tx(tx)
  , m_rx(rx)
  , m_sck(sck)
{
  gpio_set_function(tx, GPIO_FUNC_SPI);
  gpio_set_function(rx, GPIO_FUNC_SPI);
  gpio_set_function(sck, GPIO_FUNC_SPI);
}

spi_bus<void>::~spi_bus()
{
  gpio_deinit(m_tx);
  gpio_deinit(m_rx);
  gpio_deinit(m_sck);
  spi_deinit(get_bus(m_bus));
}

spi_channel::spi_channel(u8 bus,  // NOLINT
                         u8 cs,
                         hal::pollable_lock* lock,
                         hal::steady_clock* clock,
                         settings const& s)
  : m_clk(clock)
  , m_lock(lock)
  , m_bus(bus)
  , m_cs(cs)
{
  gpio_init(cs);
  gpio_set_dir(cs, GPIO_OUT);
  driver_configure(s);
}

void spi_channel::driver_configure(spi_channel::settings const& s)
{
  spi_cpol_t polarity = SPI_CPOL_0;
  spi_cpha_t phase = SPI_CPHA_0;
  switch (s.bus_mode) {
    case spi_channel::mode::m0:
      polarity = SPI_CPOL_0;
      phase = SPI_CPHA_0;
      break;
    case spi_channel::mode::m1:
      polarity = SPI_CPOL_0;
      phase = SPI_CPHA_1;
      break;
    case spi_channel::mode::m2:
      polarity = SPI_CPOL_1;
      phase = SPI_CPHA_0;
      break;
    case spi_channel::mode::m3:
      polarity = SPI_CPOL_1;
      phase = SPI_CPHA_1;
      break;
    default:
      break;
  }
  spi_init(get_bus(m_bus), s.clock_rate);
  spi_set_format(get_bus(m_bus), 8, polarity, phase, SPI_MSB_FIRST);
}

u32 spi_channel::driver_clock_rate()
{
  return spi_get_baudrate(get_bus(m_bus));
}

void spi_channel::driver_chip_select(bool sel)
{
  // https://github.com/raspberrypi/pico-examples/tree/master/spi
  // The pico examples add NOPs explicitly, which seems to be to guard against
  // the CS deasserting early. We wait explicitly, so that won't be necessary
  using namespace std::chrono_literals;
  if (sel) {
    if (m_lock) {
      m_lock->lock();
    }
    gpio_put(m_cs, false);
  } else if (!sel) {
    // Arm Primecell Synchronous Serial Port :tm: unsets the busy bit early,
    // necessitating this
    auto freq = this->driver_clock_rate();
    auto sleep_time = std::chrono::duration<u32, std::micro>(1'000'000 / freq);
    hal::delay(*m_clk, sleep_time);
    gpio_put(m_cs, true);

    if (m_lock)
      m_lock->unlock();
  }
}

void spi_channel::driver_transfer(std::span<byte const> out,
                                  std::span<byte> in,
                                  byte filler)
{
  auto out_size = out.size_bytes();
  auto in_size = in.size_bytes();
  auto size = std::min(out_size, in_size);
  spi_write_read_blocking(get_bus(m_bus), out.data(), in.data(), size);
  if (out_size > in_size) {
    spi_write_blocking(
      get_bus(m_bus), out.data() + in_size, out_size - in_size);
  } else if (in_size > out_size) {
    spi_read_blocking(
      get_bus(m_bus), filler, in.data() + out_size, in_size - out_size);
  }
}

spi_channel::~spi_channel()
{
  gpio_deinit(m_cs);
}
}  // namespace v5
}  // namespace hal::rp
