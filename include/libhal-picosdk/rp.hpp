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

#include <libhal/initializers.hpp>
#include <libhal/units.hpp>

namespace hal::rp {

namespace internal {

#if defined(LIBHAL_PLATFORM_RP2040) || defined(LIBHAL_VARIANT_RP2350A)
constexpr u8 pin_max = 30;
#elif defined(LIBHAL_VARIANT_RP2350B)
constexpr u8 pin_max = 48;
#else
#error "Unknown pico variant"
#endif

enum struct processor_type : uint8_t
{
  rp2040,
  rp2350
};

#if defined(LIBHAL_PLATFORM_RP2040)
constexpr inline processor_type type = type::rp2040;
#elif defined(LIBHAL_PLATFORM_RP2350_ARM_S)
constexpr inline processor_type type = processor_type::rp2350;
#else
#error "Unknown Pico model"
#endif

}  // namespace internal
template<typename T>
concept pin_param =
  std::is_same_v<pin_t<T::val>, T> && (T::val < internal::pin_max);
}  // namespace hal::rp
