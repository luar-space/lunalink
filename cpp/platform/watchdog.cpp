// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
#include "lunalink/platform/watchdog.hpp"

namespace lunalink::signal {

namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) -- HAL callback,
// runtime-configured by target platform
WindowedWatchdog* g_installed_wdog = nullptr;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables) -- runtime lock state for
// watchdog configuration
bool g_wdog_locked = false;
} // namespace

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

} // namespace lunalink::signal
