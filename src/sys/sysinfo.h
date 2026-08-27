#ifndef GPUI_SYS_SYSINFO_H_
#define GPUI_SYS_SYSINFO_H_

#include "base.h"

namespace gpui {

struct ProcessInfo {
    uint32_t pid = 0;
    char name[260] = {};
    float cpu = 0;
    uint64_t memory = 0;
};

struct ProcSample {
    uint32_t pid = 0;
    uint64_t cpu100ns = 0;
};

struct DiskInfo {
    float usedPct = 0;
    uint64_t total = 0;
    uint64_t used = 0;
};

struct BatteryInfo {
    bool present = false;
    bool charging = false;
    float pct = 0;
};

struct SysTimes {
    uint64_t idle = 0;
    uint64_t kernel = 0;
    uint64_t user = 0;
    bool valid = false;
};

struct SysState {
    SysTimes prevCpu;
    Vec<ProcSample> prevProcs;
    float cpu;
    float mem;
    uint64_t memTotal;
    uint64_t memUsed;
    DiskInfo disk;
    BatteryInfo battery;
    Vec<ProcessInfo> procs;
    int ncpu;

    SysState() : cpu(0), mem(0), memTotal(0), memUsed(0), ncpu(1) {
        ZeroStruct(&prevCpu);
        ZeroStruct(&disk);
        ZeroStruct(&battery);
    }
};

void SysStateInit(SysState* s);
void SysStateFree(SysState* s);
void SysRefresh(SysState* s);

enum class ProcessSort : int32_t {
    Pid = 0,
    Name = 1,
    Cpu = 2,
    Memory = 3
};
void SysSortProcesses(SysState* s, ProcessSort field, bool descending,
                      int keepTop);

TempStr FormatBytes(uint64_t bytes);
TempStr FormatPct(float v, int decimals);
} // namespace gpui
#endif // GPUI_SYS_SYSINFO_H_
