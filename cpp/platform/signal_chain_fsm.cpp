#include "lunalink/platform/signal_chain_fsm.hpp"
#include "lunalink/core/telemetry.hpp"

#include <cstdint>

namespace lunalink::signal {

// Build the legal transition matrix at compile time.
// Any (from, to) pair not listed here is illegal and triggers Zone 3.
// constinit guarantees static initialisation — no hidden __cxa_guard or
// dynamic constructor; the table is present in .rodata at boot.
// NOLINTNEXTLINE(fuchsia-statically-constructed-objects) — constinit guarantees static init
constinit const TransitionMatrix<SignalChainState, 8> SignalChainFsm::kLegalTransitions = []() {
    TransitionMatrix<SignalChainState, 8> m;
    using S = SignalChainState;
    m.add(S::kUninitialized, S::kPostRunning);
    m.add(S::kPostRunning,   S::kOperational);   // POST pass
    m.add(S::kPostRunning,   S::kSafeMode);      // POST fail
    m.add(S::kOperational,   S::kFrameFault);    // Zone 2 fault
    m.add(S::kOperational,   S::kSafeMode);      // Zone 3 fault
    m.add(S::kFrameFault,    S::kOperational);   // next frame OK
    m.add(S::kFrameFault,    S::kSafeMode);      // two consecutive faults
    m.add(S::kSafeMode,      S::kUninitialized); // OBC commanded restart
    return m;
}();

SignalChainFsm::SignalChainFsm(ITelemetryHal& hal) noexcept
    : hal_(hal), state_(SignalChainState::kUninitialized), consecutive_fault_count_(0) {}

bool SignalChainFsm::transition(SignalChainState next) noexcept {
    if (!kLegalTransitions.is_legal(state_, next)) {
        // Illegal transition → Zone 3. Emit telemetry and lock into kSafeMode. // FIZ-3
        const FaultPacket pkt{
            0U,
            static_cast<uint8_t>(SignalChainState::kSafeMode),
            3U,
            0U,
            0U,
        };
        hal_.emit(pkt);
        state_ = SignalChainState::kSafeMode;
        return false;
    }
    state_ = next;
    consecutive_fault_count_ = 0U;
    return true;
}

bool SignalChainFsm::on_frame_fault() noexcept {
    // Precondition: must only be called from kOperational. [SW-SAF-02, FM-031]
    // Radiation-induced control-flow diversion to this function from an illegal
    // state is caught here and locked to kSafeMode with FIZ-3 telemetry.
    //
    // Note: kFrameFault is also excluded by this guard (it is != kOperational).
    // A double-fault escalation path kFrameFault → kSafeMode is handled
    // exclusively by the second call to on_frame_fault() from the frame
    // assembly layer, which transitions the FSM through kOperational → kFrameFault
    // on the first fault and then calls this function again on the next bad frame.
    // A CFI diversion delivering on_frame_fault() while already in kFrameFault
    // is treated conservatively as a Zone 3 event (immediate kSafeMode lock),
    // which is the safe-side failure.  consecutive_fault_count_ is reset to zero
    // so that any OBC-commanded restart begins from a clean counter state.
    if (state_ != SignalChainState::kOperational) {
        const FaultPacket pkt{0U, static_cast<uint8_t>(SignalChainState::kSafeMode), 3U, 0U, 0U};
        hal_.emit(pkt);
        state_ = SignalChainState::kSafeMode;
        consecutive_fault_count_ = 0U;
        return false;
    }
    consecutive_fault_count_++;
    if (consecutive_fault_count_ >= 2U) {
        // Two consecutive frame faults → Zone 3. // FIZ-3
        const FaultPacket pkt{
            0U,
            static_cast<uint8_t>(SignalChainState::kSafeMode),
            3U,
            0U,
            0U,
        };
        hal_.emit(pkt);
        state_ = SignalChainState::kSafeMode;
        consecutive_fault_count_ = 0U;
        return false;
    }
    // First frame fault → Zone 2. // FIZ-2
    state_ = SignalChainState::kFrameFault;
    return true;
}

void SignalChainFsm::on_frame_ok() noexcept {
    consecutive_fault_count_ = 0U;
}

}  // namespace lunalink::signal
