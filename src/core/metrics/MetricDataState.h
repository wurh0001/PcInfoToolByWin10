#pragma once
#include <cmath>
#include <cstdint>
#include <cstddef>

enum class MetricDataState { Reading, Ready, Unsupported, Stale, Disconnected, Partial };

struct MetricSampleStatus {
    MetricDataState state = MetricDataState::Reading;
    uint64_t lastValidMs = 0;
    uint64_t lastAttemptMs = 0;
    // Monotonic milliseconds. A UI refresh is not a new sample.
    template<class History>
    void Record(float sample, float& value, History& history, uint64_t now) {
        if (lastAttemptMs && now <= lastAttemptMs) return;
        if (lastAttemptMs && now > lastAttemptMs + 1500) {
            const auto missed = (now - lastAttemptMs) / 1000 - 1;
            for (uint64_t i = 0; i < missed && i < History::kCapacity; ++i)
                history.Push(NAN);
        }
        lastAttemptMs = now;
        if (std::isfinite(sample)) {
            value = sample; lastValidMs = now; history.Push(sample);
            state = MetricDataState::Ready;
        } else {
            history.Push(NAN);
            state = lastValidMs ? MetricDataState::Stale : MetricDataState::Reading;
        }
    }
    MetricDataState Resolve(bool valid, size_t validCount, uint64_t now) const {
        if (state == MetricDataState::Unsupported || state == MetricDataState::Disconnected) return state;
        if (state == MetricDataState::Stale || (lastValidMs && now > lastValidMs + 3000)) return MetricDataState::Stale;
        if (!valid) return MetricDataState::Reading;
        return validCount < 60 ? MetricDataState::Partial : MetricDataState::Ready;
    }
};
