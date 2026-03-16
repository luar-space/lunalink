#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>

/**
 * @brief Critical-section macros for ISR-safe atomic invariant writes.
 *
 * In the **digital twin** (single-threaded, no RTOS), these expand to no-ops.
 * In **flight** (RTEMS or equivalent RTOS), the flight HAL must define these
 * macros with platform-specific interrupt masking (e.g., RTEMS irq_disable /
 * irq_enable) **before** this header is included.
 *
 * These macros bracket every atomic invariant write involving TMR triplets,
 * mirrored variables, and CFI sentinel counters to prevent false-positive fault
 * events caused by RTOS preemption.  [SW-SAF-02]
 *
 * @par Flight HAL integration note
 * The `#ifndef` guard means the flight HAL header providing the overriding
 * definitions must appear earlier in the include chain than this file.  The
 * recommended pattern is to add a top-level flight BSP header that defines
 * `LUNALINK_CRITICAL_ENTER` / `LUNALINK_CRITICAL_EXIT` before any LunaLink
 * signal header is included, or to pass `-DLUNALINK_CRITICAL_ENTER=...` via
 * the flight build system.  A future refactor (Gateway 3) will introduce an
 * explicit `lunalink/hal/critical_section.hpp` to remove this include-order
 * dependency entirely.
 */
#ifndef LUNALINK_CRITICAL_ENTER
#  define LUNALINK_CRITICAL_ENTER  ((void)0)  // Override in flight HAL (see note above)
#  define LUNALINK_CRITICAL_EXIT   ((void)0)
#endif

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
 * @brief Securely scrub a scalar value.
 */
template <typename T>
inline void secure_scrub_scalar(T& val) noexcept {
  volatile T* p = &val;
  *p = T{0};
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

// Runtime aliasing check: returns true if two byte ranges overlap.
// Used as a runtime guard replacing assert() for Class A compliance.
[[nodiscard]] inline bool spans_overlap(const void* a, size_t a_bytes,
                                        const void* b, size_t b_bytes) noexcept {
    const auto* ab = static_cast<const uint8_t*>(a);
    const auto* bb = static_cast<const uint8_t*>(b);
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return (ab < bb + b_bytes) && (bb < ab + a_bytes);
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

// Convenience template overload for typed spans.
// NOLINTNEXTLINE(fuchsia-trailing-return)
template <typename T, typename U>
[[nodiscard]] inline bool spans_overlap(std::span<T> a, std::span<U> b) noexcept {
    return spans_overlap(static_cast<const void*>(a.data()), a.size_bytes(),
                         static_cast<const void*>(b.data()), b.size_bytes());
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

  /**
   * @brief Force a value into the range, bypassing saturating logic.
   * Internal use only for Mission Assurance fault injection.
   */
  void force_set(T v) noexcept { value_ = v; }

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

#ifdef LUNALINK_FAULT_INJECTION
  /**
   * @brief Single-copy fault injection: corrupt one TMR copy by index (0=v1, 1=v2, 2=v3).
   * Only available when LUNALINK_FAULT_INJECTION=1. [SW-SAF-06]
   */
  void inject_fault(uint8_t copy_index, T corrupted_value) noexcept {
    if (copy_index == 0U) { v1 = corrupted_value; }
    else if (copy_index == 1U) { v2 = corrupted_value; }
    else { v3 = corrupted_value; }
  }
#endif

  /**
   * @brief Force a raw underlying value into all TMR copies.
   * Bypasses CheckedRange saturating logic if T is CheckedRange.
   */
  template<typename RawT>
  void force_fault(RawT raw_bad_value) noexcept {
      if constexpr (requires { v1.force_set(raw_bad_value); }) {
          v1.force_set(raw_bad_value);
          v2.force_set(raw_bad_value);
          v3.force_set(raw_bad_value);
      } else {
          v1 = static_cast<T>(raw_bad_value);
          v2 = static_cast<T>(raw_bad_value);
          v3 = static_cast<T>(raw_bad_value);
      }
  }
};

/**
 * @brief Control-Flow Integrity guard: encapsulates volatile counter + expected terminal value.
 * New components (POST, FSM) use this class. Existing inline CFI sentinels in signal sources
 * are not refactored to avoid regression risk.
 */
class CfiGuard {
  volatile uint32_t count_;
  uint32_t expected_;
 public:
  explicit constexpr CfiGuard(uint32_t magic, uint32_t n) noexcept
      : count_(magic), expected_(magic + n) {}
  void tick() noexcept { count_ = count_ + 1U; }
  [[nodiscard]] bool verify() const noexcept { return count_ == expected_; }
#ifdef LUNALINK_FAULT_INJECTION
  void inject_skip(uint32_t n) noexcept { count_ = count_ - n; }
#endif
};

/**
 * @brief Compile-time legal state transition matrix (edge-list representation).
 * N: maximum number of legal (from→to) edge pairs.
 * State values are cast to uint8_t for storage; use enums with uint8_t underlying type.
 */
template <typename State, size_t N>
class TransitionMatrix {
  struct Edge { uint8_t from; uint8_t to; };
  // NOLINTNEXTLINE(hicpp-avoid-c-arrays)
  Edge edges_[N]{};
  size_t count_{0};

 public:
  constexpr TransitionMatrix() noexcept : edges_{}, count_(0) {}

  constexpr void add(State from, State to) noexcept {
    if (count_ < N) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      edges_[count_++] = {static_cast<uint8_t>(from), static_cast<uint8_t>(to)};
    }
  }

  [[nodiscard]] bool is_legal(State from, State to) const noexcept {
    const auto f = static_cast<uint8_t>(from);
    const auto t = static_cast<uint8_t>(to);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    for (size_t i = 0; i < count_; ++i) {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      if (edges_[i].from == f && edges_[i].to == t) { return true; }
    }
    return false;
  }
};

}  // namespace lunalink::signal
