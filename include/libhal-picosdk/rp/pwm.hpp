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
#include <libhal/error.hpp>
#include <libhal/initializers.hpp>
#include <libhal/pwm.hpp>
#include <libhal/units.hpp>

namespace hal::rp {

inline namespace v4 {
/**
 * @brief A PWM class. Do not use if possible.
 */
struct pwm_pin final : hal::pwm
{
  /**
   *  By constructing this class you forfeit all claims to any
   *  warranty, implied or otherwise. You acknowledge you will
   *  read the relevant datasheet (RP2350 or RP2040 datasheet)
   *  and verify they are on different PWM slices. You waive
   *  any right to complain about two different PWM pins interfering
   *  with each other because they are on the same slice.
   */
  pwm_pin(hal::unsafe, u8 pin);
  pwm_pin(pwm_pin&&) = delete;
  ~pwm_pin() override;

private:
  void driver_frequency(hertz p_frequency) override;
  void driver_duty_cycle(float p_duty_cycle) override;

  u8 m_pin;
};
}  // namespace v4

namespace v5 {
/**
 * @brief Represents the A/B channels on each PWM slice
 */
enum struct pwm_ch : u8
{
  a,
  b
};

struct pwm_pin;

struct pwm_pin_configuration
{
  u16 duty_cycle = 0;
  bool autostart = true;
};

/**
 *  This is the base runtime class for a PWM slice.
 *  Construct pwm_slice instead.
 */
struct pwm_slice_runtime : hal::pwm_group_manager
{

  struct configuration
  {
    bool phase_correct = false;
  };

  pwm_slice_runtime(pwm_slice_runtime&&) = delete;
  ~pwm_slice_runtime() override;

  void enable(bool enable = true);

protected:
  pwm_slice_runtime(u8 slice_num, configuration const&);

  pwm_pin get_pin_raw(u8 pin, pwm_pin_configuration const&);
  /*
  At frequencies above ~2288 Hz, the wrap counter
  begins to lose resolution, reducing the duty cycle resolution
  down from its theoretical 16 bits of resolution.
  */
  void driver_frequency(u32 p_frequency) final;
  u8 m_number;
  bool m_phase_correct;
};

/* This cannot be constructed normally, and needs to be
 * obtained from a pwm_slice type. */
struct pwm_pin final : hal::pwm16_channel
{

  pwm_pin(pwm_pin&&) = delete;
  ~pwm_pin() override;

  friend pwm_slice_runtime;

private:
  pwm_pin(u8 pin, pwm_pin_configuration const& c, hal::unsafe);
  u32 driver_frequency() override;

  /**
   * When set to a 0% duty cycle, it disables the timer completely.
   */
  void driver_duty_cycle(u16 p_duty_cycle) override;

  u8 m_pin;
  u8 m_slice;
  bool m_autostart;
};

/**
 * This globally starts all of the timers at once, which may be useful for
 * aligning their phases. Set argument to false to stop all of them at once */
void enable_all_pwm(bool start = true);

/**
 * PWM slices are fine-grained PWM peripherals with 2 comparators each, A
 * and B. The PWM slice manages the clock divider and counter, while each
 * pin compares the register to a value to determine output.
 */
template<u64 channel>
struct pwm_slice final : pwm_slice_runtime
{

  pwm_slice(pwm_slice&&) = delete;
  pwm_slice(channel_param auto ch,
            pwm_slice_runtime::configuration const& cfg = {})
    : pwm_slice_runtime(ch(), cfg)
  {
    using enum internal::processor_type;
    static_assert(internal::type == rp2040 || internal::type == rp2350,
                  "Unknown RP type!");
    static_assert(ch() < 8 || internal::pin_max != 30,
                  "PWM channel is invalid!");
    static_assert(ch() < 11 || internal::pin_max != 48,
                  "PWM channel is invalid!");
    static_assert(ch() < max_slices(), "Invalid PWM slice!");
  }

  /**
   * @brief Gets pwm pin from pin parameter.
   * See relevant processor datasheet for correct pin-to-slice mapping.
   */
  pwm_pin get_pin(pin_param auto pin, pwm_pin_configuration const& config = {})
  {
    static_assert(get_slice_number(pin) == channel, "Slice pin is incorrect!");
    return get_pin_raw(pin(), config);
  }

  static constexpr u8 max_slices()
  {
    using enum hal::rp::internal::processor_type;
    if constexpr (hal::rp::internal::type == rp2040) {
      return 8;
    } else if constexpr (hal::rp::internal::type == rp2350) {
      return 12;
    }
    static_assert(hal::rp::internal::type == rp2040 or
                    hal::rp::internal::type == rp2350,
                  "Unknown RP type");
  }

  static constexpr u8 get_slice_number(pin_param auto pin)
  {
    if (pin() < 32) {
      return (pin() / 2) % 8;
    } else {
      return 8 + ((pin() >> 1) & 3);
    }
  }
};
template<channel_param p>
pwm_slice(p pin) -> pwm_slice<p::val>;
}  // namespace v5
}  // namespace hal::rp
