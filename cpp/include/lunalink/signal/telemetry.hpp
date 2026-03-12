#pragma once
#include <array>
#include <cstdint>

namespace lunalink::signal {

/**
 * @brief Fixed-width telemetry packet emitted before Zone 3 fault halt.
 * 8-byte fixed width is enforced by static_assert. [SW-SAF-03]
 */
struct FaultPacket {
    uint32_t timestamp_ms;      // monotonic millisecond counter (HAL-provided)
    uint8_t  fault_code;        // enum-class value of the originating fault status
    uint8_t  fiz_level;         // 1, 2, or 3
    uint8_t  originating_step;  // POST step (1-5), FSM state, or 0
    uint8_t  reserved;          // padding; must be 0x00
};
static_assert(sizeof(FaultPacket) == 8U, "FaultPacket must be exactly 8 bytes");

/**
 * @brief Telemetry Hardware Abstraction Layer interface.
 * Implementations deliver packets to flight software, ground links, or test harnesses.
 */
// NOLINTBEGIN(hicpp-special-member-functions)
class ITelemetryHal {
 public:
    virtual void emit(const FaultPacket& pkt) noexcept = 0;
    virtual ~ITelemetryHal() = default;
};
// NOLINTEND(hicpp-special-member-functions)

/**
 * @brief No-op implementation: silently discards all packets.
 * Used as the default in non-flight stubs.
 */
class NullTelemetryHal final : public ITelemetryHal {
 public:
    // NOLINTNEXTLINE(hicpp-named-parameter)
    void emit(const FaultPacket& /*pkt*/) noexcept override {}
};

/**
 * @brief Capture implementation: stores up to N packets for test assertions.
 */
template <size_t N>
class CaptureTelemetryHal final : public ITelemetryHal {
 public:
    void emit(const FaultPacket& pkt) noexcept override {
        if (count_ < N) {
            packets_[count_++] = pkt;
        }
    }

    [[nodiscard]] size_t count() const noexcept { return count_; }

    [[nodiscard]] const FaultPacket& packet(size_t idx) const noexcept {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
        return packets_[idx];
    }

    void reset() noexcept { count_ = 0U; }

 private:
    std::array<FaultPacket, N> packets_{};
    size_t count_{0};
};

}  // namespace lunalink::signal
