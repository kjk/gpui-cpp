
#include "Base.h"

namespace gpui {

struct ProcessInfo {
    u32 pid = 0;
    char name[260] = {};
    float cpu = 0;
    u64 memory = 0;
};

struct ProcSample {
    u32 pid = 0;
    u64 cpu100ns = 0;
};

struct DiskInfo {
    float usedPct = 0;
    u64 total = 0;
    u64 used = 0;
};

struct BatteryInfo {
    bool present = false;
    bool charging = false;
    float pct = 0;
};

struct SysTimes {
    u64 idle = 0;
    u64 kernel = 0;
    u64 user = 0;
    bool valid = false;
};

struct SysState {
    SysTimes prevCpu;
    Vec<ProcSample> prevProcs;
    float cpu;
    float mem;
    u64 memTotal;
    u64 memUsed;
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

enum class ProcessSort : i32 {
    Pid = 0,
    Name = 1,
    Cpu = 2,
    Memory = 3
};
void SysSortProcesses(SysState* s, ProcessSort field, bool descending,
                      int keepTop);

TempStr FormatBytes(u64 bytes);
TempStr FormatPct(float v, int decimals);
} // namespace gpui
