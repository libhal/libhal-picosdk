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

#include <libhal/units.hpp>

#include <hardware/platform_defs.h>
#include <hardware/timer.h>
#include <pico/time.h>

#include "libhal-arm-mcu/rp/time.hpp"

namespace hal::rp::inline v4 {

hertz clock::driver_frequency()
{
  return 1'000'000;
}

u64 clock::driver_uptime()
{
  return time_us_64();
}

hertz core_clock()
{
  return SYS_CLK_HZ;
}

void sleep(std::chrono::duration<u64, std::micro> time) noexcept
{
  sleep_us(time.count());
}

}  // namespace hal::rp::inline v4
