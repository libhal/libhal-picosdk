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
#include <libhal/input_pin.hpp>

namespace hal::rp::inline v4 {
/**
 * @brief A class that allows reading from a GPIO pin.
 */
struct input_pin final : public hal::input_pin
{
  input_pin(pin_param auto pin, settings const& s = {})
    : input_pin(pin(), s)
  {
  }
  input_pin(input_pin&&) = delete;
  ~input_pin() override;

private:
  input_pin(u8, settings const&);
  void driver_configure(settings const&) override;
  bool driver_level() override;

  u8 m_pin{};
};
}  // namespace hal::rp::inline v4
