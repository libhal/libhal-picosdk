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

#include <libhal-util/bit.hpp>
#include <libhal-util/math.hpp>
#include <libhal/error.hpp>
#include <libhal/units.hpp>

#include <hardware/adc.h>
#include <hardware/address_mapped.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/platform_defs.h>

#include "libhal-picosdk/adc.hpp"
#include "libhal-picosdk/rp.hpp"
#include "libhal-picosdk/time.hpp"

namespace hal::rp {

inline namespace v4 {
adc::adc(u8 pin)
  : m_pin(pin)
{
  constexpr auto last_pin = ADC_BASE_PIN + NUM_ADC_CHANNELS;
  if (not(ADC_BASE_PIN <= pin && pin < last_pin)) {
    hal::safe_throw(
      hal::argument_out_of_domain(reinterpret_cast<void*>(pin)));  // NOLINT
  }
  adc_init();
  adc_gpio_init(pin);
}

adc::~adc()
{
  gpio_deinit(m_pin);
}

float adc::driver_read()
{
  adc_select_input(m_pin - ADC_BASE_PIN);
  u16 const result = adc_read();
  // See table 1120 in RP2350 datasheet
  // / table 568 in RPP2040 datasheet for ADC ERR bit:
  // "The most recent ADC conversion encountered an error; result is
  // undefined or noisy"
  if (adc_hw->cs & ADC_CS_ERR_BITS) {
    hal::safe_throw(hal::io_error(this));
  }

  return float(result) / float((1 << 12) - 1);
}
};  // namespace v4

namespace v5 {

adc16::adc16(u8 pin)
  : m_pin(pin)
{
  adc_init();
  if (pin < ADC_BASE_PIN || pin >= ADC_BASE_PIN + NUM_ADC_CHANNELS - 1) {
    hal::safe_throw(
      hal::argument_out_of_domain(reinterpret_cast<void*>(pin)));  // NOLINT
  }
  adc_gpio_init(pin);
}

adc16::~adc16()
{
  gpio_deinit(m_pin);
}

u16 adc16::driver_read()
{
  adc_select_input(m_pin - ADC_BASE_PIN);
  u16 result = adc_read();
  // weirdly enough the sdk doesn't provide a function to read the error bits
  if (adc_hw->cs & ADC_CS_ERR_BITS) {
    hal::safe_throw(hal::io_error(this));
  }
  return hal::upscale<u16, 12>(result);
}

}  // namespace v5

}  // namespace hal::rp
