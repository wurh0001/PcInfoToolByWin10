#pragma once
#include "MetricsSnapshot.h"
#include <memory>

class MetricsCollector {
public:
    MetricsCollector();
    ~MetricsCollector();

    void Start();
    void Stop();

    void SetGpuEnabled(bool enable);

    MetricsSnapshot GetLatestSnapshot();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
