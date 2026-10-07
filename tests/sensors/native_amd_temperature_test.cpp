#include "core/collectors/NativeAmdTemperature.h"
#include <cstdio>
int main() {
    NativeAmdTemperature sensor;
    for(int i=0;i<3;++i) {
        const auto r=sensor.Sample();
        std::printf("status=0x%08lx temperature=%.3f uptime=%llu\n",static_cast<unsigned long>(r.status),r.temperature,static_cast<unsigned long long>(r.sampleTime));
        if(FAILED(r.status)||r.temperature<=0) return 1;
        if(i<2) Sleep(1000);
    }
    return 0;
}
