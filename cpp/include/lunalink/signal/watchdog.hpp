#pragma once
#include <cstdint>
#include "lunalink/signal/telemetry.hpp"

namespace lunalink::signal {

/**
 * @brief Fault code for FM-033 windowed watchdog lower-bound violation.
 * 0xCC (11001100): high Hamming distance from kSafeMode (0x99) and all other
 * module fault codes. Emitted in FaultPacket.fault_code when a lower-bound trip
 * is detected.
 */
inline constexpr uint8_t kWatchdogLowerBoundFaultCode = 0xCCU;

/**
 * @brief Windowed watchdog monitor for radiation-induced logic-skip detection. [FM-033]
 *
 * A windowed watchdog has two bounds:
 *  - Upper bound (t_max_ticks): must NOT be exceeded between reset() and evaluate().
 *    Exceeding it means the loop hung.  Hardware watchdog handles this path.
 *  - Lower bound (t_min_ticks): must NOT be under-shot between reset() and evaluate().
 *    Under-shooting it means the loop completed impossibly fast — indicative of a
 *    radiation-induced logic skip bypassing wip_tick() call sites.
 *
 * This implementation is fully software-based (no hardware timer required) and
 * testable in the Digital Twin.  Instantiate on the stack or as a named member;
 * no dynamic allocation.
 *
 * Canonical usage pattern:
 * @code
 *   WindowedWatchdog wdog(kMinExpectedTicks, kMaxExpectedTicks);
 *   wdog.reset();
 *   // ... inner loop that calls wdog.tick() at each work step ...
 *   if (!wdog.evaluate(hal, timestamp_ms)) {
 *       fsm.transition(SignalChainState::kSafeMode);  // Zone 3 escalation
 *   }
 * @endcode
 */
class WindowedWatchdog {
 public:
  /**
   * @param t_min_ticks  Minimum tick() calls expected between reset() and evaluate().
   *                     Under-shot → lower-bound trip → FaultPacket emitted (FIZ-2).
   * @param t_max_ticks  Maximum tick() calls tolerated.  Over-shot is informational;
   *                     hardware watchdog handles the true hang path.
   */
  constexpr WindowedWatchdog(uint32_t t_min_ticks, uint32_t t_max_ticks) noexcept
      : t_min_(t_min_ticks), t_max_(t_max_ticks), count_(0U) {}

  /**
   * @brief Record one unit of progress.  Call inside the monitored work block.
   * Safe to call from any context; uses only a volatile increment.
   */
  void tick() noexcept {
    count_ = count_ + 1U;
  }

  /**
   * @brief Reset the progress counter.  Call immediately before the work block.
   */
  void reset() noexcept {
    count_ = 0U;
  }

  /**
   * @brief Evaluate the window after the work block completes.
   *
   * @param hal           Telemetry sink — receives FaultPacket on lower-bound trip.
   * @param timestamp_ms  Monotonic millisecond counter for FaultPacket.timestamp_ms.
   * @returns true   Count within [t_min, t_max] — nominal.
   * @returns false  Count < t_min — lower-bound violated; FaultPacket emitted (fiz_level=2).
   *                 Caller MUST escalate to Zone 3 (transition FSM to kSafeMode). // FIZ-2
   */
  [[nodiscard]] bool evaluate(ITelemetryHal& hal, uint32_t timestamp_ms) noexcept {
    const uint32_t n = count_;  // snapshot volatile before any emission
    if (n < t_min_) {
      const FaultPacket pkt{
          timestamp_ms,
          kWatchdogLowerBoundFaultCode,
          2U,   // fiz_level: Zone 3 escalation is the caller's responsibility
          0U,   // originating_step: 0 = watchdog monitor (not a POST step)
          0U,   // reserved
      };
      hal.emit(pkt);
      return false;  // FIZ-2
    }
    return true;
  }

  /**
   * @brief Return the current raw tick count (test accessor).
   */
  [[nodiscard]] uint32_t tick_count() const noexcept {
    return count_;
  }

  /**
   * @brief Return true if the tick count has exceeded the upper bound.
   * Informational only — hardware watchdog owns the upper-bound response path.
   */
  [[nodiscard]] bool upper_bound_exceeded() const noexcept {
    return count_ > t_max_;
  }

#ifdef LUNALINK_FAULT_INJECTION
  /**
   * @brief Force the internal counter to an arbitrary value for SWIFI testing.
   * Only available when LUNALINK_FAULT_INJECTION=1.
   */
  void inject_tick_count(uint32_t forced_count) noexcept {
    count_ = forced_count;
  }
#endif

 private:
  uint32_t          t_min_;
  uint32_t          t_max_;
  volatile uint32_t count_;
};

/**
 * @brief Register a watchdog instance to receive optional global notifications.
 * Pass nullptr to uninstall. NOT thread-safe; call only during initialisation,
 * before lock_watchdog_config() is called.
 *
 * @return true if the pointer was accepted; false if configuration is locked.
 */
[[nodiscard]] bool install_watchdog(WindowedWatchdog* wdog) noexcept;

/**
 * @brief Lock the watchdog configuration. After this call, install_watchdog()
 * will reject further writes. Called by POST after init-phase setup is complete.
 * Irreversible until the next cold boot.
 */
void lock_watchdog_config() noexcept;

/**
 * @brief Retrieve the currently installed global watchdog (may be nullptr).
 */
[[nodiscard]] WindowedWatchdog* get_installed_watchdog() noexcept;

}  // namespace lunalink::signal
