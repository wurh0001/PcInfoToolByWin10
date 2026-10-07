#include "core/collectors/PhysicalDiskSampler.h"
#include <cstdio>
int main() {
    PhysicalDiskSampler sampler;
    const auto disks=sampler.Sample({});
    if(disks.empty()) return 2;
    bool complete=true;
    for(const auto& disk:disks) {
        std::printf("Disk %lu capacity=%llu bytes\n",static_cast<unsigned long>(disk.number),static_cast<unsigned long long>(disk.capacityBytes));
        complete=complete&&disk.capacityBytes>0;
    }
    return complete?0:1;
}
