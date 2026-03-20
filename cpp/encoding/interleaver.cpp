#include "lunalink/encoding/interleaver.hpp"

namespace lunalink::signal {

namespace {

// Validate interleaver input/output span sizes and aliasing.
template <typename T, typename U>
InterleaverStatus validate_interleaver_spans(std::span<T> in, std::span<U> out) noexcept {
  if (in.size() < kInterleaverSize) {
    return InterleaverStatus::kInputTooSmall;
  }
  if (out.size() < kInterleaverSize) {
    return InterleaverStatus::kOutputTooSmall;
  }
  if (spans_overlap(static_cast<const void*>(in.data()),
                    in.size_bytes(),
                    static_cast<const void*>(out.data()),
                    out.size_bytes())) {
    return InterleaverStatus::kFaultDetected; // FIZ-1 [MA §11]
  }
  return InterleaverStatus::kOk;
}

// Verify CFI counter and accumulator match expected values.
bool verify_interleaver_cfi(uint32_t cfi_count, size_t in_idx_acc) noexcept {
  return (cfi_count == (0xA5A5A5A5U + kInterleaverSize)) && (in_idx_acc == kInterleaverSize);
}

} // namespace

InterleaverStatus block_interleave(std::span<const uint8_t> in, std::span<uint8_t> out) noexcept {
  InterleaverStatus status = validate_interleaver_spans(in, out);
  // cppcheck-suppress variableScope -- volatile CFI counter must outlive the guarded block
  volatile uint32_t cfi_count = 0xA5A5A5A5U;

  // Write row-wise (in[r*98 + c]), read column-wise (out[c*60 + r])
  // MA §6: Mirror index computation to detect ALU corruption.
  size_t in_idx_acc = 0;

  for (uint16_t r = 0; r < kInterleaverRows && status == InterleaverStatus::kOk; ++r) {
    for (uint16_t c = 0; c < kInterleaverCols; ++c) {
      const size_t in_idx = (static_cast<size_t>(r) * kInterleaverCols) + c;
      const size_t out_idx = (static_cast<size_t>(c) * kInterleaverRows) + r;

      // Cross-check: sequential accumulator must match computed index.
      if (in_idx != in_idx_acc) [[unlikely]] {
        status = InterleaverStatus::kFaultDetected; // FIZ-1
        break;
      }

      out[out_idx] = in[in_idx];
      in_idx_acc++;
      cfi_count = cfi_count + 1U;
      wip_tick();
    }
  }

  if (status == InterleaverStatus::kOk && !verify_interleaver_cfi(cfi_count, in_idx_acc))
      [[unlikely]] {
    status = InterleaverStatus::kFaultDetected; // FIZ-1
  }

  if (status != InterleaverStatus::kOk) {
    secure_scrub(out);
  }

  return status;
}

InterleaverStatus block_deinterleave(std::span<const uint8_t> in, std::span<uint8_t> out) noexcept {
  InterleaverStatus status = validate_interleaver_spans(in, out);
  // cppcheck-suppress variableScope -- volatile CFI counter must outlive the guarded block
  volatile uint32_t cfi_count = 0xA5A5A5A5U;

  // Write column-wise, read row-wise
  size_t in_idx_acc = 0;

  for (uint16_t c = 0; c < kInterleaverCols && status == InterleaverStatus::kOk; ++c) {
    for (uint16_t r = 0; r < kInterleaverRows; ++r) {
      const size_t in_idx = (static_cast<size_t>(c) * kInterleaverRows) + r;
      const size_t out_idx = (static_cast<size_t>(r) * kInterleaverCols) + c;

      if (in_idx != in_idx_acc) [[unlikely]] {
        status = InterleaverStatus::kFaultDetected; // FIZ-1
        break;
      }

      out[out_idx] = in[in_idx];
      in_idx_acc++;
      cfi_count = cfi_count + 1U;
      wip_tick();
    }
  }

  if (status == InterleaverStatus::kOk && !verify_interleaver_cfi(cfi_count, in_idx_acc))
      [[unlikely]] {
    status = InterleaverStatus::kFaultDetected; // FIZ-1
  }

  if (status != InterleaverStatus::kOk) {
    secure_scrub(out);
  }

  return status;
}

// [LSIS-AFS-471]
InterleaverStatus block_deinterleave_soft(std::span<const float> in,
                                          std::span<float> out) noexcept {
  InterleaverStatus status = validate_interleaver_spans(in, out);
  // cppcheck-suppress variableScope -- volatile CFI counter must outlive the guarded block
  volatile uint32_t cfi_count = 0xA5A5A5A5U;

  // Write column-wise, read row-wise (same permutation as hard deinterleave)
  size_t in_idx_acc = 0;

  for (uint16_t c = 0; c < kInterleaverCols && status == InterleaverStatus::kOk; ++c) {
    for (uint16_t r = 0; r < kInterleaverRows; ++r) {
      const size_t in_idx = (static_cast<size_t>(c) * kInterleaverRows) + r;
      const size_t out_idx = (static_cast<size_t>(r) * kInterleaverCols) + c;

      if (in_idx != in_idx_acc) [[unlikely]] {
        status = InterleaverStatus::kFaultDetected; // FIZ-1
        break;
      }

      out[out_idx] = in[in_idx];
      in_idx_acc++;
      cfi_count = cfi_count + 1U;
      wip_tick();
    }
  }

  if (status == InterleaverStatus::kOk && !verify_interleaver_cfi(cfi_count, in_idx_acc))
      [[unlikely]] {
    status = InterleaverStatus::kFaultDetected; // FIZ-1
  }

  if (status != InterleaverStatus::kOk) {
    secure_scrub(out);
  }

  return status;
}

} // namespace lunalink::signal
