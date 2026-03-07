#pragma once
#include <cstdint>
#include <algorithm>
#include <limits>

namespace lunalink::signal {

/**
 * @brief Value-constrained wrapper ensuring a value remains within [Min, Max].
 * Implements "Saturating Logic" per Mission Assurance requirements.
 *
 * @tparam T Underlying type (e.g., uint8_t, uint16_t)
 * @tparam Min Minimum valid value
 * @tparam Max Maximum valid value
 */
template <typename T, T Min, T Max>
class CheckedRange {
  static_assert(Min <= Max, "CheckedRange: Min must be less than or equal to Max");
  T value_;

 public:
  constexpr CheckedRange() noexcept : value_(Min) {}

  explicit constexpr CheckedRange(T v) noexcept : value_(v) {
    saturate();
  }

  // NOLINTBEGIN(fuchsia-overloaded-operator)
  constexpr CheckedRange& operator=(T v) noexcept {
    value_ = v;
    saturate();
    return *this;
  }
  // NOLINTEND(fuchsia-overloaded-operator)

  [[nodiscard]] constexpr T value() const noexcept { return value_; }
  
  // Implicit conversion is intentionally allowed for ergonomic use in DSP blocks.
  // This allows CheckedRange to be used seamlessly in comparisons and arithmetic.
  // NOLINTNEXTLINE(hicpp-explicit-conversions)
  [[nodiscard]] constexpr operator T() const noexcept { return value_; }

 private:
  constexpr void saturate() noexcept {
    if (value_ < Min) {
      value_ = Min;
    }
    if (value_ > Max) {
      value_ = Max;
    }
  }
};

/**
 * @brief Triple Modular Redundancy (TMR) wrapper for mission-critical parameters.
 * Stores three copies and uses majority voting on read to resist SEUs.
 */
template <typename T>
struct TmrValue {
  T v1;
  T v2;
  T v3;

  constexpr TmrValue() noexcept : v1(T{}), v2(T{}), v3(T{}) {}
  explicit constexpr TmrValue(T v) noexcept : v1(v), v2(v), v3(v) {}

  [[nodiscard]] constexpr T vote() const noexcept {
    if (v1 == v2 || v1 == v3) {
      return v1;
    }
    return v2; // Majority 2-of-3
  }

  constexpr void refresh(T v) noexcept {
    v1 = v2 = v3 = v;
  }
};

} // namespace lunalink::signal
