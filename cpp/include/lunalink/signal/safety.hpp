#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>

/**
 * @brief Macro for portable memory section pinning of LUTs.
 * ELF (Linux/RTEMS): .lunalink_lut
 * Mach-O (macOS): __TEXT,lunalink_lut
 */
#if defined(__APPLE__)
#define LUNALINK_LUT_SECTION [[gnu::section("__TEXT,lunalink_lut")]]
#else
#define LUNALINK_LUT_SECTION [[gnu::section(".lunalink_lut")]]
#endif

namespace lunalink::signal {

/**
 * @brief Securely scrub sensitive memory to prevent Dead Store Elimination (DSE).
 * Uses volatile pointer to ensure compiler does not optimize away the zeroing.
 * Templated to support scrubbing arrays of any POD type (uint8_t, int16_t, etc).
 */
template <typename T>
inline void secure_scrub(std::span<T> data) noexcept {
  if (data.empty()) {
    return;
  }
  volatile T* p = data.data();
  for (size_t i = 0; i < data.size(); ++i) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    p[i] = T{0};
  }
}

/**
 * @brief Overload for std::array or other containers with data() and size().
 */
template <typename Container>
inline void secure_scrub(Container& c) noexcept {
  secure_scrub(std::span(c.data(), c.size()));
}

/**
 * @brief Temporal "Work-In-Progress" (WIP) tick for Watchdog feeding.
 * High-reliability loops should call this to prove progress.
 */
inline void wip_tick() noexcept {
  // Flight-specific WIP implementation (e.g., poke a hardware register or
  // increment a global) Implementation-defined for the specific hardware
  // digital twin.
  [[maybe_unused]] volatile static uint32_t wip_count = 0;
  wip_count = wip_count + 1U;
}

/**
 * @brief Value-constrained wrapper ensuring a value remains within [Min, Max].
 * Implements "Saturating Logic" per Mission Assurance requirements.
 */
template <typename T, T Min, T Max>
class CheckedRange {
  static_assert(Min <= Max,
                "CheckedRange: Min must be less than or equal to Max");
  T value_;

 public:
  constexpr CheckedRange() noexcept : value_(Min) {}

  explicit constexpr CheckedRange(T v) noexcept : value_(v) { saturate(); }

  // NOLINTBEGIN(fuchsia-overloaded-operator)
  constexpr CheckedRange& operator=(T v) noexcept {
    value_ = v;
    saturate();
    return *this;
  }
  // NOLINTEND(fuchsia-overloaded-operator)

  [[nodiscard]] constexpr T value() const noexcept { return value_; }

  // Implicit conversion is intentionally allowed for ergonomic use in DSP
  // blocks. NOLINTNEXTLINE(hicpp-explicit-conversions)
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
 * @brief Triple Modular Redundancy (TMR) wrapper with Active Repair (Scrubbing).
 * Stores three copies and corrects a single corrupted copy during vote()
 * [NASA-FSW].
 */
template <typename T>
struct TmrValue {
  mutable T v1;
  mutable T v2;
  mutable T v3;

  constexpr TmrValue() noexcept : v1(T{}), v2(T{}), v3(T{}) {}
  explicit constexpr TmrValue(T v) noexcept : v1(v), v2(v), v3(v) {}

  /**
   * @brief Perform majority vote and repair the corrupted copy.
   * Constant-time complexity (branchless if possible, but repair requires
   * assignment).
   * @note Not constexpr because it performs active repair on mutable members.
   */
  [[nodiscard]] T vote() const noexcept {
    if (v1 == v2) {
      v3 = v1;  // Repair v3 if it flipped
      return v1;
    }
    if (v1 == v3) {
      v2 = v1;  // Repair v2 if it flipped
      return v1;
    }
    if (v2 == v3) {
      v1 = v2;  // Repair v1 if it flipped
      return v2;
    }
    // Critical Failure: No majority. In flight, this triggers Safe Mode.
    return v1;
  }

  /**
   * @brief Passive majority vote for constexpr contexts.
   * Does NOT perform active repair.
   */
  [[nodiscard]] constexpr T peek() const noexcept {
    if (v1 == v2 || v1 == v3) {
      return v1;
    }
    return v2;
  }

  constexpr void refresh(T v) noexcept { v1 = v2 = v3 = v; }

  // NOLINTNEXTLINE(fuchsia-overloaded-operator)
  constexpr bool operator==(const TmrValue& other) const noexcept {
    return peek() == other.peek();
  }

  /**
   * @brief Fault Injection helper: Force all copies to a bad value.
   * Internal use only for Mission Assurance coverage testing.
   */
  void inject_fault(T bad_value) noexcept { v1 = v2 = v3 = bad_value; }
};

}  // namespace lunalink::signal
