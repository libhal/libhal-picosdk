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

#include <chrono>
#include <libhal/steady_clock.hpp>

namespace hal::rp::inline v4 {

/**
 * @brief A 1 MHz timer
 * The default 1 MHz timer used frequently inside of picosdk itself.
 * On the RP2350, the timer is in the always-on domain, meaning it is
 * always available.
 */
struct clock final : public hal::steady_clock
{
  clock() = default;
  clock(clock const&) = default;
  /**
   * @brief Returns clock frequency, which is always 1 MHz
   */
  hertz driver_frequency() override;
  /**
   * @brief Returns clock uptime, which is always in us
   */
  u64 driver_uptime() override;
};

/**
 * @brief Returns SYS_CLK_HZ, intended to be used with dwt_counter
 */
hertz core_clock();

using microseconds = std::chrono::duration<u64, std::micro>;
/**
 * @brief A sleep function optimized for power.
 * Unlike hal::delay, which busy waits, sleep may cause a low-power
 * mode to be entered, reducing power consumption.
 */
void sleep(std::chrono::duration<u64, std::micro> time) noexcept;

}  // namespace hal::rp::inline v4
