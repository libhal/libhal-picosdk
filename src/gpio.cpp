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
#include <array>
#include <cstdint>
#include <optional>
#include <utility>

#include <libhal/error.hpp>
#include <libhal/functional.hpp>
#include <libhal/interrupt_pin.hpp>
#include <libhal/units.hpp>

#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <pico.h>
#include <pico/sync.h>
#include <pico/time.h>

#include "libhal-arm-mcu/rp/input_pin.hpp"
#include "libhal-arm-mcu/rp/interrupt_pin.hpp"
#include "libhal-arm-mcu/rp/output_pin.hpp"
#include "libhal-arm-mcu/rp/rp.hpp"

namespace {
struct interrupt_manager
{

  struct lock
  {

    interrupt_manager* operator->()
    {
      return m_ptr;
    }

    interrupt_manager& operator*()
    {
      return *m_ptr;
    }

    // locks are uncopiable
    lock(lock const&) = delete;
    // locks are immovable to prevent moved-from states
    lock(lock&&) = delete;

    ~lock()
    {
      restore_interrupts_from_disabled(m_status);
    }

  private:
    interrupt_manager* m_ptr;
    uint32_t m_status;
    lock(interrupt_manager* m)
      : m_ptr(m)
      , m_status(save_and_disable_interrupts())
    {
    }
    friend struct interrupt_manager;
  };

  struct interrupt
  {
    hal::interrupt_pin::trigger_edge edge;
    hal::callback<void(bool)> callback;
  };

  static lock get()
  {
    // GPIO callbacks are core-specific
    auto& manager = global[get_core_num()];
    if (!manager) {
      manager = new interrupt_manager();
      gpio_set_irq_callback(&irq);
    }
    return { manager };
  }

  void insert(hal::u8 pin, interrupt handler)
  {
    m_interrupts[pin] = std::move(handler);
  }

  interrupt& at(hal::u8 pin)
  {
    return m_interrupts[pin].value();
  }

  void remove(hal::u8 pin)
  {
    m_interrupts[pin].reset();
  }

  // I don't control the callback types clang-tidy
  static void irq(uint gpio, std::uint32_t event)  // NOLINT
  {
    using enum hal::interrupt_pin::trigger_edge;
    auto g = get();

    // If there is no handler then we don't do anything
    if (!g->m_interrupts[gpio].has_value()) {
      return;
    }

    auto& handler = *g->m_interrupts[gpio];

    bool should_activate = false;
    switch (handler.edge) {
      case rising:
        should_activate = event & GPIO_IRQ_EDGE_RISE;
        break;
      case falling:
        should_activate = event & GPIO_IRQ_EDGE_FALL;
        break;
      case both:
        should_activate = event & (GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL);
        break;
    }
    if (should_activate) {
      bool value = gpio_get(gpio);
      handler.callback(value);
    }
  }

private:
  std::array<std::optional<interrupt>, hal::rp::internal::pin_max> m_interrupts;
  interrupt_manager() = default;
  ~interrupt_manager() = default;
  static inline std::array<interrupt_manager*, 2> global = {};
};

}  // namespace

namespace hal::rp::inline v4 {

input_pin::input_pin(u8 pin, settings const& options)
  : m_pin(pin)
{
  if (pin >= NUM_BANK0_GPIOS) {
    hal::safe_throw(hal::argument_out_of_domain(this));
  }

  gpio_init(pin);
  gpio_set_function(pin, gpio_function_t::GPIO_FUNC_SIO);
  gpio_set_dir(pin, GPIO_IN);

  driver_configure(options);
}

input_pin::~input_pin()
{
  gpio_deinit(m_pin);
}

void input_pin::driver_configure(settings const& p_settings)
{
  switch (p_settings.resistor) {
    case pin_resistor::pull_down:
      gpio_pull_down(m_pin);
      break;
    case pin_resistor::pull_up:
      gpio_pull_up(m_pin);
      break;
    default:
      [[fallthrough]];
    case pin_resistor::none:
      gpio_disable_pulls(m_pin);
      break;
  }
}

bool input_pin::driver_level()
{
  return gpio_get(m_pin);
}

output_pin::output_pin(u8 pin, settings const& options)
  : m_pin(pin)
{
  if (pin >= NUM_BANK0_GPIOS) {
    hal::safe_throw(hal::argument_out_of_domain(this));
  }

  gpio_init(pin);
  gpio_set_function(pin, gpio_function_t::GPIO_FUNC_SIO);
  gpio_set_dir(pin, GPIO_OUT);

  driver_configure(options);
}

output_pin::~output_pin()
{
  gpio_deinit(m_pin);
}

void output_pin::driver_configure(settings const& options)
{
  // RP2* series chips don't seem to have any explicit support for
  // open drain mode, so we fail loud rather than silently
  if (options.open_drain) {
    hal::safe_throw(hal::operation_not_supported(this));
  }

  switch (options.resistor) {
    case pin_resistor::pull_down:
      gpio_pull_down(m_pin);
      break;
    case pin_resistor::pull_up:
      gpio_pull_up(m_pin);
      break;
    default:
      [[fallthrough]];
    case pin_resistor::none:
      gpio_disable_pulls(m_pin);
      break;
  }
}

void output_pin::driver_level(bool level)
{
  gpio_put(m_pin, level);
}

bool output_pin::driver_level()
{
  return gpio_get(m_pin);
}

interrupt_pin::interrupt_pin(u8 pin, settings const& options)
  : m_pin(pin)
{
  gpio_init(pin);
  gpio_set_dir(pin, false);
  gpio_set_function(pin, gpio_function_t::GPIO_FUNC_SIO);
  {
    auto g = interrupt_manager::get();
    g->insert(m_pin, { .edge = options.trigger, .callback = {} });
    // drop the lock
  }
  driver_configure(options);
}

interrupt_pin::~interrupt_pin()
{
  auto g = interrupt_manager::get();
  g->remove(m_pin);
}

void interrupt_pin::driver_configure(settings const& options)
{
  auto g = interrupt_manager::get();
  switch (options.resistor) {
    case pin_resistor::none:
      gpio_disable_pulls(m_pin);
      break;
    case pin_resistor::pull_up:
      gpio_pull_up(m_pin);
      break;
      // pulldown seems reasonable, although this should never trigger anyways
    default:
      [[fallthrough]];
    case pin_resistor::pull_down:
      gpio_pull_down(m_pin);
      break;
  }
  g->at(m_pin).edge = options.trigger;
}

void interrupt_pin::driver_on_trigger(hal::callback<handler> callback)
{
  auto g = interrupt_manager::get();
  std::swap(g->at(m_pin).callback, callback);
}

}  // namespace hal::rp::inline v4
