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
#include "time.hpp"
#include <libhal/adc.hpp>
#include <libhal/units.hpp>
#include <span>

namespace hal::rp {

inline namespace v4 {

/**
 * @brief A class that represents a single ADC pin
 *
 * The RP-series ADC is muxed amongst the last few pins,
 * and the adc class implementation simply switches the
 * mux whenever an ADC measurement needs to be done.
 *
 */
struct adc final : public hal::adc
{
  /**
   * @brief Constructs an ADC pin from a pin constant
   * Defers to the runtime constructor to reduce code bloat,
   * only really doing compiletime error checking.
   */
  adc(pin_param auto pin)
    : adc(pin())
  {
    constexpr int adc_base = internal::pin_max == 30 ? 26 : 40;
    static_assert(adc_base <= pin() && pin() < internal::pin_max,
                  "ADC pin is invalid!");
  }
  adc(adc&&) = delete;
  ~adc() override;

private:
  adc(u8 pin);
  /**
   * @brief Reads the ADC and returns the value as a ratio.
   * Returns the ADC read, normalized to a range of [0.0, 1.0].
   * May spurously throw hal::io_error if the ADC encounters an
   * error for some reason. See table 1120 in RP2350 datasheet
   * and table 568 in RPP2040 datasheet for ADC ERR bit.
   */
  float driver_read() override;
  u8 m_pin;
};
}  // namespace v4

namespace v5 {
/**
 * @brief A class that represents a single ADC pin
 *
 * The RP-series ADC is 12-bit, but we use hal::upscale
 * to increase the output to 16-bit.
 */
struct adc16 final : public hal::adc16
{
  adc16(pin_param auto pin)
    : adc16(pin())
  {
    static_assert(internal::pin_max != 30 || (pin() >= 26 && pin() < 30),
                  "ADC pin is invalid!");
    static_assert(internal::pin_max != 48 || (pin() >= 40 && pin() < 48),
                  "ADC pin is invalid!");
  }
  ~adc16() override;

private:
  adc16(u8 gpio);
  /**
   * @brief Reads the ADC and returns the value as a 16 bit integer.
   * Returns the ADC read, normalized to a range of [0, 2^16-1].
   * May spurously throw hal::io_error if the ADC encounters an
   * error for some reason. See table 1120 in RP2350 datasheet
   * and table 568 in RPP2040 datasheet for ADC ERR bit.
   */
  u16 driver_read() override;
  u8 m_pin;
};
}  // namespace v5

namespace nonstandard {
/**
 * @brief A class that represents a set of ADC pins
 *
 * Allows multiple ADC reads in one function call, reducing function
 * call overhead.
 */
struct adc16_pack
{
  struct read_session;

  template<pin_param... Pins>
  adc16_pack(Pins... ps)
    : adc16_pack((to_mask(ps) | ...))
  {
  }
  adc16_pack(adc16_pack const&) = delete;
  adc16_pack(adc16_pack&&) = delete;

  void read_many_now(std::span<u16>);
  /**
   * Allows many ADC reads asynchronously via DMA.
   * Currently broken, requires further investigation.
   * TODO(#3)
   */
  read_session async();

private:
  adc16_pack(u8 pin_mask);
  u8 to_mask(pin_param auto pin)
  {
    if constexpr (internal::pin_max == 30) {
      static_assert(pin() >= 26 && pin() < 30);
      return 1 << (pin() - 26);
    } else if constexpr (internal::pin_max == 48) {
      static_assert(pin() >= 40 && pin() < 48);
      return 1 << (pin() - 40);
    }
  }
  u8 m_read_size, m_first_pin;
};

struct adc16_pack::read_session
{
  struct promise;
  promise read(std::span<u16>);
  ~read_session();
  read_session(read_session const&) = delete;
  read_session(read_session&&) = delete;

  struct promise
  {
    microseconds poll();
    promise(promise const&) = default;

  private:
    friend read_session;
    promise(u8 dma, u8 first_pin)  // NOLINT
      : m_dma(dma)
      , m_first_pin(first_pin)
    {
    }
    u8 m_dma, m_first_pin;
  };

private:
  friend adc16_pack;
  read_session(u8 dma, u8 read_size, u8 first_pin)  // NOLINT
    : m_dma(dma)
    , m_read_size(read_size)
    , m_first_pin(first_pin)
  {
  }
  u8 m_dma, m_read_size, m_first_pin;
};

}  // namespace nonstandard

}  // namespace hal::rp
