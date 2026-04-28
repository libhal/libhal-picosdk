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
#include <libhal/interrupt_pin.hpp>
#include <libhal/units.hpp>

namespace hal::rp::inline v4 {
/**
 * @brief A class that enables registering interrupts for gpio changes.
 * The RP2350 has only 1 interrupt for all GPIO changes, per core, so the
 * interrupt pin class must create a core-local global to register all
 * all interrupts. This becomes a 1.5kB global variable allocated via malloc.
 * ISRs registered must be reentrant.
 */
struct interrupt_pin final : public hal::interrupt_pin
{
  interrupt_pin(pin_param auto pin, settings const& options = {})
    : interrupt_pin(pin(), options)
  {
  }
  interrupt_pin(interrupt_pin&&) = delete;
  ~interrupt_pin() override;

private:
  interrupt_pin(u8 pin, settings const&);
  void driver_configure(settings const&) override;
  void driver_on_trigger(hal::callback<handler>) override;
  u8 m_pin;
};
}  // namespace hal::rp::inline v4
