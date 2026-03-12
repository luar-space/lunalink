#include "lunalink/signal/signal_chain_fsm.hpp"
#include "lunalink/signal/telemetry.hpp"

#include <cstdint>

namespace lunalink::signal {

// Build the legal transition matrix at compile time.
// Any (from, to) pair not listed here is illegal and triggers Zone 3.
// NOLINTBEGIN(cert-err58-cpp,fuchsia-statically-constructed-objects)
const TransitionMatrix<SignalChainState, 8> SignalChainFsm::kLegalTransitions = []() {
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
// NOLINTEND(cert-err58-cpp,fuchsia-statically-constructed-objects)

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
