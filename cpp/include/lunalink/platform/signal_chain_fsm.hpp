#pragma once
#include <cstdint>
#include "lunalink/core/safety.hpp"
#include "lunalink/core/telemetry.hpp"

namespace lunalink::signal {

/**
 * @brief Signal chain operational states.
 * Non-contiguous bit patterns resist SEU-induced state confusion. [FM-031]
 */
enum class SignalChainState : uint8_t {
    kUninitialized = 0x11U,  // Before POST
    kPostRunning   = 0x22U,  // POST in progress
    kOperational   = 0x44U,  // Nominal operation
    kFrameFault    = 0x55U,  // Zone 2: single frame fault
    kSafeMode      = 0x99U,  // Zone 3: system-level halt
};

/**
 * @brief Signal chain finite state machine with compile-time legal transition enforcement.
 * Illegal transitions trigger Zone 3 (FIZ-3) and emit a telemetry packet. [SW-SAF-02, FM-031, FM-032]
 */
class SignalChainFsm {
 public:
    explicit SignalChainFsm(ITelemetryHal& hal) noexcept;

    /**
     * @brief Attempt a state transition. Returns false (FIZ-3) if the transition is
     * illegal; emits a telemetry packet and stays in kSafeMode. Returns true on success.
     */
    [[nodiscard]] bool transition(SignalChainState next) noexcept;

    /**
     * @brief Record a frame-level fault (Zone 2). Escalates to kSafeMode (Zone 3)
     * after two consecutive frame faults. [FM-032]
     * Returns false if escalation to kSafeMode occurred.
     *
     * @pre state() == kOperational. If called from any other state, emits FIZ-3
     *      telemetry and locks to kSafeMode to contain radiation-induced CFI diversion.
     */
    [[nodiscard]] bool on_frame_fault() noexcept;

    /**
     * @brief Record a successful frame commit; resets the consecutive-fault counter.
     */
    void on_frame_ok() noexcept;

    [[nodiscard]] SignalChainState state() const noexcept { return state_; }

 private:
    ITelemetryHal&   hal_;
    SignalChainState state_{SignalChainState::kUninitialized};
    uint8_t          consecutive_fault_count_{0};

    // Legal transition matrix — built at construction, checked on every transition().
    static const TransitionMatrix<SignalChainState, 8> kLegalTransitions;
};

}  // namespace lunalink::signal
