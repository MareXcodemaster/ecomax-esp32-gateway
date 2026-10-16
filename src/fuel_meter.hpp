#pragma once
#include <cmath>
#include <cstdint>

namespace pellet {
enum class Source : uint8_t { unavailable, reported, feeder_estimate };
inline bool valid_rate(double value) { return std::isfinite(value) && value >= 0 && value <= 100; }
inline double calibration_rate(double kg, double seconds) {
    if (!std::isfinite(kg) || !std::isfinite(seconds) || kg < 0.01 || kg > 25 || seconds < 10 || seconds > 3600) return NAN;
    double rate = kg * 3600 / seconds;
    return rate >= 0.01 && valid_rate(rate) ? rate : NAN;
}
struct Totals {
    double reported_kg = 0, estimated_kg = 0;
    uint64_t counted_ms = 0, omitted_ms = 0, reported_ms = 0, estimated_ms = 0;
};
struct Meter {
    Totals totals;
    double previous_rate = NAN;
    Source previous_source = Source::unavailable;
    uint64_t previous_ms = 0;
    bool have_previous = false;
    void discontinuity() { have_previous = false; }
    void sample(uint64_t ms, double rate, Source source) {
        if (!valid_rate(rate)) { rate = NAN; source = Source::unavailable; }
        if (have_previous && ms > previous_ms) {
            uint64_t dt = ms - previous_ms;
            // Never extend a stale sample or bridge source/calibration changes.
            if (dt <= 5000 && source != Source::unavailable && source == previous_source) {
                double kg = (previous_rate + rate) * 0.5 * dt / 3600000;
                if (source == Source::reported) { totals.reported_kg += kg; totals.reported_ms += dt; }
                else { totals.estimated_kg += kg; totals.estimated_ms += dt; }
                totals.counted_ms += dt;
            } else totals.omitted_ms += dt;
        }
        previous_rate = rate; previous_source = source; previous_ms = ms; have_previous = true;
    }
};
}
