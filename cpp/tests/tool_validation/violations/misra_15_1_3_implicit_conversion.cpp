// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// MISRA C++:2023 Rule 15.1.3 (Required): Conversion operators and constructors
// that are callable with a single argument shall be explicit.
// Expected diagnostic: google-explicit-constructor

class TvsVoltage {
public:
  TvsVoltage(double v) : volts_(v) {} // Implicit single-arg ctor — target violation
  double get() const {
    return volts_;
  }

private:
  double volts_;
};
