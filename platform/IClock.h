// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <chrono>

namespace fl {

// Monotonic clock seam for testable time-dependent logic. Production components default to
// SystemClock::instance(); tests inject a ManualClock to control time without real sleeps.
// Components hold a `const IClock*` (defaulting to the SystemClock singleton) and expose setClock();
// a pointer (not a reference) keeps the holding class copyable/assignable (e.g. AuthTracker).
struct IClock {
    virtual ~IClock() = default;
    virtual std::chrono::steady_clock::time_point now() const = 0;
};

// Process-wide real clock. The default for every clock-using component.
class SystemClock final : public IClock {
  public:
    std::chrono::steady_clock::time_point now() const override {
        return std::chrono::steady_clock::now();
    }
    static const SystemClock& instance() {
        static const SystemClock s;
        return s;
    }
};

// Test double: time only moves when the test moves it.
class ManualClock final : public IClock {
  public:
    explicit ManualClock(std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now())
        : m_now(start) {}
    std::chrono::steady_clock::time_point now() const override {
        return m_now;
    }
    void set(std::chrono::steady_clock::time_point t) {
        m_now = t;
    }
    void advance(std::chrono::steady_clock::duration d) {
        m_now += d;
    }

  private:
    std::chrono::steady_clock::time_point m_now;
};

} // namespace fl
