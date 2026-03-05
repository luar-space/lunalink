#include "lunalink/signal/prn.hpp"
#include "lunalink/signal/prn_table.hpp"

namespace lunalink::signal {

const uint8_t* gold_prn(uint8_t prn_id) noexcept {
    return kGoldPrns[prn_id - 1u];
}

} // namespace lunalink::signal
