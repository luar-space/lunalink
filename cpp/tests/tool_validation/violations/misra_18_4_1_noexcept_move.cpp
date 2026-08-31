// Copyright 2025-2026 Marco Fiscato
// SPDX-License-Identifier: Apache-2.0
// MISRA C++:2023 Rule 18.4.1 (Required): Move and swap operations shall be
// declared noexcept. N7 Space coding standard.
// Expected diagnostic: performance-noexcept-move-constructor

class TvsNoexceptMove {
public:
  TvsNoexceptMove() = default;
  TvsNoexceptMove(TvsNoexceptMove&& other) { // Move ctor not noexcept — target violation
    data_ = other.data_;
    other.data_ = 0;
  }
  TvsNoexceptMove& operator=(TvsNoexceptMove&& other) { // Move assign not noexcept
    data_ = other.data_;
    other.data_ = 0;
    return *this;
  }

private:
  int data_ = 0;
};
