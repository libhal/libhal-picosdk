#pragma once

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

#include "rp.hpp"
#include <libhal/initializers.hpp>
#include <libhal/lock.hpp>
#include <libhal/spi.hpp>
#include <libhal/steady_clock.hpp>

namespace hal::rp {
constexpr u8 bus_from_tx_pin(u8 tx)
{
  return (tx / 8) % 2;
}

inline namespace v4 {
/**
 * @brief An SPI peripheral. Does not manage CS.
 * Uses compiletime checking for pin values, see GPIO
 * Functions table in relevant processor datasheet for
 * correct pin assignments.
 */
struct spi final : public hal::spi
{
  spi(pin_param auto copi,
      pin_param auto cipo,
      pin_param auto sck,
      spi::settings const& options = {})
    : spi(bus_from_tx_pin(copi()), copi(), cipo(), sck(), options)
  {
    static_assert(cipo() % 4 == 0, "SPI RX pin is invalid");
    static_assert(sck() % 4 == 2, "SPI SCK pin is invalid");
    static_assert(copi() % 4 == 3, "SPI CS pin is invalid");
  }
  spi(spi&&) = delete;
  ~spi() override;

private:
  spi(u8 bus, u8 copi, u8 cipo, u8 sck, spi::settings const&);
  void driver_configure(settings const&) override;

  void driver_transfer(std::span<byte const> out,
                       std::span<byte> in,
                       byte) override;

  u8 m_bus, m_tx, m_rx, m_sck;
};
}  // namespace v4
namespace v5 {
struct spi_channel;
template<typename Lock = void>
struct spi_bus;
template<>
/**
 * @brief An SPI peripheral. Use via acquiring devices.
 * Uses compiletime checking for pin values, see GPIO
 * Functions table in relevant processor datasheet for
 * correct pin assignments.
 */
struct spi_bus<void>
{
  spi_bus(bus_param auto bus,
          pin_param auto copi,
          pin_param auto cipo,
          pin_param auto sck)
    : spi_bus(bus(), copi(), cipo(), sck())
  {
    static_assert(bus() == bus_from_tx_pin(copi()),
                  "Bus parameter does not match pins");
    static_assert(cipo() % 4 == 0, "SPI RX pin is invalid");
    static_assert(sck() % 4 == 2, "SPI SCK pin is invalid");
    static_assert(copi() % 4 == 3, "SPI CS pin is invalid");
  }
  ~spi_bus();

  /**
   * @brief Acquires a device assciated with a certain pin.
   * Due to usage of software CS, a steady clock is necessary
   * to wait the prerequesite time since SPI peripheral unsets
   * itself a little early.
   */
  spi_channel acquire_device(pin_param auto pin,
                             hal::steady_clock&,
                             hal::v5::spi_channel::settings const& settings);

protected:
  spi_channel acquire_device(u8 pin,
                             hal::pollable_lock* lock,
                             hal::steady_clock&,
                             hal::v5::spi_channel::settings const& settings);
  spi_bus(u8 bus, u8 tx, u8 rx, u8 sck);
  u8 m_bus, m_tx, m_rx, m_sck;
};

/**
 * @brief An SPI peripheral with runtime lock support.
 * Pass any parameters at the end for constructing the lock.
 * The lock inside of spi_bus will be used for all acquired devices.
 */
template<hal::lockable Lock>
struct spi_bus<Lock> : spi_bus<void>
{
  template<typename... Ts>
  spi_bus(bus_param auto bus,
          pin_param auto copi,
          pin_param auto cipo,
          pin_param auto sck,
          Ts... args)
    : spi_bus<void>(bus, copi, cipo, sck)
    , m_lock(std::forward<Ts>(args)...)
  {
  }

  spi_channel acquire_device(pin_param auto pin,
                             hal::steady_clock&,
                             hal::v5::spi_channel::settings const& settings);

  Lock m_lock;
};

/**
 * @brief A device on the SPI data bus, muxed with a CS pin.
 * May lock at runtime if spi_bus was constructed with a lock.
 */
struct spi_channel final : public hal::spi_channel
{
  friend spi_bus<void>;
  ~spi_channel() override;

private:
  spi_channel(u8 bus,
              u8 cs,
              hal::pollable_lock*,
              hal::steady_clock*,
              settings const&);
  void driver_configure(settings const&) override;
  u32 driver_clock_rate() override;
  void driver_chip_select(bool p_select) override;
  /**
   * @brief Transfers bytes with device.
   * Currently only supports 8-bit transactions, see TODO(#1)
   */
  void driver_transfer(std::span<byte const> out,
                       std::span<byte> in,
                       byte) override;
  hal::steady_clock* m_clk;
  hal::pollable_lock* m_lock;
  u8 m_bus, m_cs;
};

inline spi_channel spi_bus<void>::acquire_device(
  pin_param auto pin,
  hal::steady_clock& clk,
  hal::v5::spi_channel::settings const& s)
{
  return acquire_device(pin(), nullptr, clk, s);
}
template<hal::lockable Lock>
spi_channel spi_bus<Lock>::acquire_device(
  pin_param auto pin,
  hal::steady_clock& clk,
  hal::v5::spi_channel::settings const& s)
{
  return acquire_device(pin(), &m_lock, clk, s);
}

inline spi_channel spi_bus<void>::acquire_device(
  u8 pin,
  hal::pollable_lock* lock,
  hal::steady_clock& clk,
  hal::v5::spi_channel::settings const& settings)
{
  return { m_bus, pin, lock, &clk, settings };
}

}  // namespace v5
}  // namespace hal::rp
