// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <type_traits>

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
#define LUNALINK_CRITICAL_ENTER ((void)0) // Override in flight HAL (see note above)
#define LUNALINK_CRITICAL_EXIT  ((void)0)
#endif

// ── Compiler abstraction macros ──────────────────────────────────────────────
// These isolate GCC/Clang-specific constructs behind portable macros so
// the same source compiles under MSVC for the Tier 3 (Python wheel) build.
// Flight builds (Tier 1/2) always use GCC/Clang and get the real attributes.
// See docs/assurance/build_policy.rst for the three-tier model.

/**
 * @brief Portable memory section pinning for LUTs and constant tables.
 *
 * - ELF (Linux/RTEMS):  `.lunalink_lut`
 * - Mach-O (macOS):     `__TEXT,lunalink_lut`
 * - MSVC (Windows):     no-op (section pinning is a flight/linker-script concern)
 */
#if defined(_MSC_VER)
#define LUNALINK_LUT_SECTION /* no-op on MSVC — section pinning is Tier 1 only */
#elif defined(__APPLE__)
#define LUNALINK_LUT_SECTION [[gnu::section("__TEXT,lunalink_lut")]]
#else
#define LUNALINK_LUT_SECTION [[gnu::section(".lunalink_lut")]]
#endif

/**
 * @brief Portable struct packing.
 *
 * Usage:
 *   LUNALINK_PACKED_BEGIN
 *   struct Foo { ... };
 *   LUNALINK_PACKED_END
 *
 * GCC/Clang: `__attribute__((packed))` via pragma.
 * MSVC: `#pragma pack(push, 1)` / `#pragma pack(pop)`.
 */
#if defined(_MSC_VER)
#define LUNALINK_PACKED_BEGIN __pragma(pack(push, 1))
#define LUNALINK_PACKED_END   __pragma(pack(pop))
#define LUNALINK_PACKED_ATTR  /* attribute on struct not needed with pragma */
#else
#define LUNALINK_PACKED_BEGIN /* GCC/Clang use attribute syntax directly */
#define LUNALINK_PACKED_END
#define LUNALINK_PACKED_ATTR [[gnu::packed]]
#endif

/**
 * @brief SFINAE helper: detect if T has a `force_set(U)` method.
 *
 * Replaces `if constexpr (requires { ... })` which MSVC < 17.4 rejects.
 */
template <typename T, typename U, typename = void> struct has_force_set : std::false_type {};
template <typename T, typename U>
struct has_force_set<T, U, std::void_t<decltype(std::declval<T&>().force_set(std::declval<U>()))>>
    : std::true_type {};
template <typename T, typename U>
inline constexpr bool has_force_set_v = has_force_set<T, U>::value;

namespace lunalink::signal {

/**
 * @brief Securely scrub sensitive memory to prevent Dead Store Elimination (DSE).
 * Uses volatile pointer to ensure compiler does not optimize away the zeroing.
 * Templated to support scrubbing arrays of any POD type (uint8_t, int16_t, etc).
 */
template <typename T> inline void secure_scrub(std::span<T> data) noexcept {
  if (data.empty()) {
    return;
  }
  volatile T* p = data.data();
  for (size_t i = 0; i < data.size(); ++i) {
    // Volatile pointer arithmetic required for DSE-proof scrubbing —
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    p[i] = T{0};
  }
}

/**
 * @brief Securely scrub a scalar value.
 */
template <typename T> inline void secure_scrub_scalar(T& val) noexcept {
  volatile T* p = &val;
  *p = T{0};
}

/**
 * @brief Overload for std::array or other containers with data() and size().
 */
template <typename Container> inline void secure_scrub(Container& c) noexcept {
  secure_scrub(std::span(c.data(), c.size()));
}

/**
 * @brief File-scope WIP counter for the digital twin.
 *
 * In **flight**, the flight HAL must define `LUNALINK_WIP_TICK` before this
 * header is included to redirect to a hardware watchdog poke.
 *
 * Wrap-around is acceptable for the mission profile: the 12 s frame period
 * yields ~10 M ticks maximum per frame, well within the uint32_t 4 G limit.
 * The watchdog only checks that the counter *changes* between polls, not
 * monotonicity, so unsigned wrap at 2^32 is benign.  Saturating arithmetic
 * is intentionally NOT used — a saturated counter would stop changing and
 * falsely trigger the watchdog.
 */
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) -- HAL-replaceable WIP counter
inline volatile uint32_t g_wip_count = 0;

#ifndef LUNALINK_WIP_TICK
inline void wip_tick() noexcept {
  g_wip_count = g_wip_count + 1U;
}
#else
inline void wip_tick() noexcept {
  LUNALINK_WIP_TICK;
}
#endif

/**
 * @brief Runtime aliasing check: returns true if two byte ranges overlap.
 *
 * Used as a runtime guard replacing assert() for Class A compliance.
 * Prevents undefined behaviour from overlapping input/output buffers
 * in encoding and signal-processing functions.
 */
[[nodiscard("spans_overlap returns true when input and output buffers alias; ignoring "
            "the result permits undefined behaviour from overlapping reads/writes — "
            "Category-A aliasing guard, do not discard")]] inline bool
spans_overlap(const void* a, size_t a_bytes, const void* b, size_t b_bytes) noexcept {
  if (a == nullptr || b == nullptr) {
    return false;
  }
  if (a_bytes == 0 || b_bytes == 0) {
    return false;
  }
  const auto* ab = static_cast<const uint8_t*>(a);
  const auto* bb = static_cast<const uint8_t*>(b);
  // Pointer arithmetic needed for byte-level overlap comparison —
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return (ab < bb + b_bytes) && (bb < ab + a_bytes);
  // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

// Convenience template overload for typed spans.
// Template return type deduced by compiler — NOLINTNEXTLINE(fuchsia-trailing-return)
template <typename T, typename U>
[[nodiscard("spans_overlap returns true when input and output spans alias; ignoring "
            "the result permits undefined behaviour from overlapping reads/writes — "
            "Category-A aliasing guard, do not discard")]] inline bool
spans_overlap(std::span<T> a, std::span<U> b) noexcept {
  return spans_overlap(static_cast<const void*>(a.data()),
                       a.size_bytes(),
                       static_cast<const void*>(b.data()),
                       b.size_bytes());
}

/**
 * @brief Value-constrained wrapper ensuring a value remains within [Min, Max].
 * Implements "Saturating Logic" per Mission Assurance requirements.
 */
template <typename T, T Min, T Max> class CheckedRange {
  static_assert(Min <= Max, "CheckedRange: Min must be less than or equal to Max");
  T value_;

public:
  constexpr CheckedRange() noexcept : value_(Min) {}

  explicit constexpr CheckedRange(T v) noexcept : value_(v) {
    saturate();
  }

  // Value-type assignment operator required for saturating semantics —
  // NOLINTBEGIN(fuchsia-overloaded-operator)
  constexpr CheckedRange& operator=(T v) noexcept {
    value_ = v;
    saturate();
    return *this;
  }
  // NOLINTEND(fuchsia-overloaded-operator)

  /// Return the current saturated value (passive read, no repair).
  [[nodiscard]] constexpr T value() const noexcept {
    return value_;
  }

  /**
   * @brief Force a value into the range, bypassing saturating logic.
   * Internal use only for Mission Assurance fault injection.
   */
  void force_set(T v) noexcept {
    value_ = v;
  }

  // Implicit conversion is intentionally allowed for ergonomic use in DSP
  // blocks. NOLINTNEXTLINE(hicpp-explicit-conversions)
  [[nodiscard]] constexpr operator T() const noexcept {
    return value_;
  }

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
template <typename T> struct TmrValue {
  mutable T v1;
  mutable T v2;
  mutable T v3;

  constexpr TmrValue() noexcept : v1(T{}), v2(T{}), v3(T{}) {}
  explicit constexpr TmrValue(T v) noexcept : v1(v), v2(v), v3(v) {}

  /**
   * @brief Perform majority vote and repair the corrupted copy.
   * Constant-time complexity (branchless if possible, but repair requires
   * assignment).
   * @param[out] fault  Set to true if no majority exists (triple-corruption).
   *                    Caller MUST check this and escalate to Safe Mode.
   * @note Not constexpr because it performs active repair on mutable members.
   */
  [[nodiscard]] T vote(bool& fault) const noexcept {
    fault = false;
    if (v1 == v2) {
      LUNALINK_CRITICAL_ENTER;
      v3 = v1; // Repair v3 if it flipped
      LUNALINK_CRITICAL_EXIT;
      return v1;
    }
    if (v1 == v3) {
      LUNALINK_CRITICAL_ENTER;
      v2 = v1; // Repair v2 if it flipped
      LUNALINK_CRITICAL_EXIT;
      return v1;
    }
    if (v2 == v3) {
      LUNALINK_CRITICAL_ENTER;
      v1 = v2; // Repair v1 if it flipped
      LUNALINK_CRITICAL_EXIT;
      return v2;
    }
    // Critical Failure: No majority — triple-corruption detected.
    fault = true;
    return v1;
  }

  /**
   * @brief Convenience overload: majority vote with repair, no fault output.
   * Use only when the caller already has a separate integrity check path
   * (e.g., CFI terminal verification covers the downstream computation).
   */
  [[nodiscard]] T vote() const noexcept {
    bool fault = false;
    return vote(fault);
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

  void refresh(T v) noexcept {
    LUNALINK_CRITICAL_ENTER;
    v1 = v;
    v2 = v;
    v3 = v;
    LUNALINK_CRITICAL_EXIT;
  }

  // Value-type equality comparison required — NOLINTNEXTLINE(fuchsia-overloaded-operator)
  constexpr bool operator==(const TmrValue& other) const noexcept {
    return peek() == other.peek();
  }

  /**
   * @brief Fault Injection helper: Force all copies to a bad value.
   * Internal use only for Mission Assurance coverage testing.
   */
  void inject_fault(T bad_value) noexcept {
    v1 = v2 = v3 = bad_value;
  }

#ifdef LUNALINK_FAULT_INJECTION
  /**
   * @brief Single-copy fault injection: corrupt one TMR copy by index (0=v1, 1=v2, 2=v3).
   * Only available when LUNALINK_FAULT_INJECTION=1. [SW-SAF-06]
   */
  void inject_fault(uint8_t copy_index, T corrupted_value) noexcept {
    if (copy_index == 0U) {
      v1 = corrupted_value;
    } else if (copy_index == 1U) {
      v2 = corrupted_value;
    } else {
      v3 = corrupted_value;
    }
  }
#endif

  /**
   * @brief Force a raw underlying value into all TMR copies.
   * Bypasses CheckedRange saturating logic if T is CheckedRange.
   */
  template <typename RawT> void force_fault(RawT raw_bad_value) noexcept {
    if constexpr (has_force_set_v<T, RawT>) {
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
 * @brief Strong-typed TMR-protected identifier with compile-time range [Min, Max].
 *
 * Eliminates copy-paste boilerplate for Fid, Toi, PrnId and any future
 * ranged identifiers.  Tag type provides nominal type safety (prevents
 * Fid/Toi/PrnId confusion at compile time).
 *
 * Storage is `TmrValue<CheckedRange<uint8_t, Min, Max>>` — triple modular
 * redundant with saturating logic.  `repair()` performs active TMR voting;
 * `value()` is a passive majority peek.
 */
template <uint8_t Min, uint8_t Max, typename Tag> struct TmrCheckedId {
  TmrValue<CheckedRange<uint8_t, Min, Max>> storage{CheckedRange<uint8_t, Min, Max>{Min}};

  constexpr TmrCheckedId() noexcept = default;
  explicit constexpr TmrCheckedId(uint8_t v) noexcept
      : storage(CheckedRange<uint8_t, Min, Max>{v}) {}

  [[nodiscard]] constexpr uint8_t value() const noexcept {
    return static_cast<uint8_t>(storage.peek());
  }

  /**
   * @brief Perform active repair of the TMR triplets.
   * @return The repaired majority value.
   */
  uint8_t repair() const noexcept {
    return static_cast<uint8_t>(storage.vote());
  }

  [[nodiscard]] constexpr bool valid() const noexcept {
    const uint8_t v = value();
    return v >= Min && v <= Max;
  }

  explicit constexpr operator uint8_t() const noexcept {
    return value();
  }

  // Value-type equality comparison required — NOLINTNEXTLINE(fuchsia-overloaded-operator)
  constexpr bool operator==(const TmrCheckedId& other) const noexcept {
    return value() == other.value();
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
  void tick() noexcept {
    LUNALINK_CRITICAL_ENTER;
    count_ = count_ + 1U;
    LUNALINK_CRITICAL_EXIT;
  }
  [[nodiscard("CfiGuard.verify() returns false when the count differs from the "
              "expected magic+N terminal value — a skipped tick is a control-flow "
              "fault; ignoring the result disables the CFI gate entirely")]] bool
  verify() const noexcept {
    return count_ == expected_;
  }
#ifdef LUNALINK_FAULT_INJECTION
  void inject_skip(uint32_t n) noexcept {
    count_ = count_ - n;
  }
#endif
};

/**
 * @brief Compile-time legal state transition matrix (edge-list representation).
 * N: maximum number of legal (from→to) edge pairs.
 * State values are cast to uint8_t for storage; use enums with uint8_t underlying type.
 */
template <typename State, size_t N> class TransitionMatrix {
  struct Edge {
    uint8_t from;
    uint8_t to;
  };
  // C-style array required for constexpr packed layout — NOLINTNEXTLINE(hicpp-avoid-c-arrays)
  Edge edges_[N]{};
  size_t count_{0};

public:
  constexpr TransitionMatrix() noexcept : edges_{}, count_(0) {}

  constexpr void add(State from, State to) noexcept { // NOCOVER_BEGIN(CONSTEXPR_INIT): called
                                                      // during constinit static initialization
    if (count_ < N) {
      // Index bounds-checked by guard clause above —
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      edges_[count_++] = {static_cast<uint8_t>(from), static_cast<uint8_t>(to)};
    }
  } // NOCOVER_END

  [[nodiscard("is_legal returns false for unregistered FSM edges — an illegal "
              "transition is a CFI fault; ignoring the result disables the legal-edge "
              "gate (SW-SAF-02, FIZ-3)")]] bool
  is_legal(State from, State to) const noexcept {
    const auto f = static_cast<uint8_t>(from);
    const auto t = static_cast<uint8_t>(to);
    // Loop bound checked against count_ <= N —
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    for (size_t i = 0; i < count_; ++i) {
      // Index bounded by count_ < N invariant —
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
      if (edges_[i].from == f && edges_[i].to == t) {
        return true;
      }
    }
    return false;
  }
};

} // namespace lunalink::signal
