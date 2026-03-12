#include "lunalink/signal/watchdog.hpp"

namespace lunalink::signal {

namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
WindowedWatchdog* g_installed_wdog = nullptr;
}  // namespace

void install_watchdog(WindowedWatchdog* wdog) noexcept {
    g_installed_wdog = wdog;
}

WindowedWatchdog* get_installed_watchdog() noexcept {
    return g_installed_wdog;
}

}  // namespace lunalink::signal
