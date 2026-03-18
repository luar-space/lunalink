#include "lunalink/platform/watchdog.hpp"

namespace lunalink::signal {

namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
WindowedWatchdog* g_installed_wdog = nullptr;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
bool g_wdog_locked = false;
}  // namespace

bool install_watchdog(WindowedWatchdog* wdog) noexcept {
    if (g_wdog_locked) [[unlikely]] {
        return false;
    }
    g_installed_wdog = wdog;
    return true;
}

void lock_watchdog_config() noexcept {
    g_wdog_locked = true;
}

WindowedWatchdog* get_installed_watchdog() noexcept {
    return g_installed_wdog;
}

}  // namespace lunalink::signal
