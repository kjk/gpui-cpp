/* macOS system metrics: Mach for CPU and memory, statfs for the disk, IOKit
   for the battery, libproc for the process table. Objective-C++ so it can use
   the IOKit power-source API, which is CoreFoundation-flavoured. */

#include "sys/SysInfo.h"

#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <time.h>
#include <unistd.h>

namespace gpui {

void SysStateInit(SysState* s) {
    s->prevProcs.Reset();
    s->procs.Reset();
    s->cpu = 0;
    s->mem = 0;
    s->memTotal = 0;
    s->memUsed = 0;
    ZeroStruct(&s->prevCpu);
    ZeroStruct(&s->disk);
    ZeroStruct(&s->battery);
    s->ncpu = PlatCoreCount();
}

void SysStateFree(SysState* s) {
    s->prevProcs.Reset();
    s->procs.Reset();
}

static void RefreshCpu(SysState* s) {
    host_cpu_load_info_data_t load = {};
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO,
                        (host_info_t)&load, &count) != KERN_SUCCESS) {
        return;
    }
    uint64_t idle = load.cpu_ticks[CPU_STATE_IDLE];
    uint64_t total = 0;
    for (int i = 0; i < CPU_STATE_MAX; i++) {
        total += load.cpu_ticks[i];
    }
    SysTimes cur;
    cur.idle = idle;
    // The Windows half splits kernel and user; here the whole total goes in
    // `kernel` (which includes idle there too) and `user` stays 0.
    cur.kernel = total;
    cur.user = 0;
    cur.valid = true;
    if (s->prevCpu.valid) {
        uint64_t idleD = cur.idle - s->prevCpu.idle;
        uint64_t totalD = cur.kernel - s->prevCpu.kernel;
        if (totalD > 0) {
            double used = (double)(totalD - idleD) / (double)totalD;
            s->cpu = (float)(used * 100.0);
        }
    }
    s->prevCpu = cur;
}

static void RefreshMemory(SysState* s) {
    uint64_t total = 0;
    size_t len = sizeof(total);
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    if (sysctl(mib, 2, &total, &len, nullptr, 0) != 0 || total == 0) {
        return;
    }
    vm_statistics64_data_t vm = {};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm,
                          &count) != KERN_SUCCESS) {
        return;
    }
    // Activity Monitor's "memory used": everything but the free and the
    // reclaimable pages. Compressed and wired pages count as used.
    uint64_t page = (uint64_t)getpagesize();
    uint64_t avail = ((uint64_t)vm.free_count + (uint64_t)vm.purgeable_count +
                      (uint64_t)vm.external_page_count) *
                     page;
    if (avail > total) {
        avail = total;
    }
    s->memTotal = total;
    s->memUsed = total - avail;
    s->mem = (float)((double)s->memUsed * 100.0 / (double)total);
}

static void RefreshDisk(SysState* s) {
    struct statfs st = {};
    if (statfs("/", &st) != 0 || st.f_blocks == 0) {
        return;
    }
    uint64_t unit = st.f_bsize;
    s->disk.total = (uint64_t)st.f_blocks * unit;
    uint64_t avail = (uint64_t)st.f_bavail * unit;
    s->disk.used = s->disk.total > avail ? s->disk.total - avail : 0;
    if (s->disk.total > 0) {
        s->disk.usedPct =
            (float)((double)s->disk.used * 100.0 / (double)s->disk.total);
    }
}

static void RefreshBattery(SysState* s) {
    s->battery = {};
    CFTypeRef blob = IOPSCopyPowerSourcesInfo();
    if (!blob) {
        return;
    }
    CFArrayRef list = IOPSCopyPowerSourcesList(blob);
    if (!list) {
        CFRelease(blob);
        return;
    }
    CFIndex n = CFArrayGetCount(list);
    for (CFIndex i = 0; i < n; i++) {
        CFDictionaryRef desc = IOPSGetPowerSourceDescription(
            blob, CFArrayGetValueAtIndex(list, i));
        if (!desc) {
            continue;
        }
        auto type =
            (CFStringRef)CFDictionaryGetValue(desc, CFSTR(kIOPSTypeKey));
        if (!type || CFStringCompare(type, CFSTR(kIOPSInternalBatteryType),
                                     0) != kCFCompareEqualTo) {
            continue;
        }
        auto cur = (CFNumberRef)CFDictionaryGetValue(
            desc, CFSTR(kIOPSCurrentCapacityKey));
        auto max =
            (CFNumberRef)CFDictionaryGetValue(desc, CFSTR(kIOPSMaxCapacityKey));
        int curV = 0;
        int maxV = 0;
        if (cur) {
            CFNumberGetValue(cur, kCFNumberIntType, &curV);
        }
        if (max) {
            CFNumberGetValue(max, kCFNumberIntType, &maxV);
        }
        if (maxV <= 0) {
            continue;
        }
        s->battery.present = true;
        s->battery.pct = (float)((double)curV * 100.0 / (double)maxV);
        auto state = (CFStringRef)CFDictionaryGetValue(
            desc, CFSTR(kIOPSPowerSourceStateKey));
        s->battery.charging =
            state && CFStringCompare(state, CFSTR(kIOPSACPowerValue), 0) ==
                         kCFCompareEqualTo;
        break;
    }
    CFRelease(list);
    CFRelease(blob);
}

static uint64_t FindPrevCpu(const Vec<ProcSample>& prev, uint32_t pid) {
    for (int i = 0; i < prev.len; i++) {
        if (prev[i].pid == pid) {
            return prev[i].cpu100ns;
        }
    }
    return 0;
}

static void RefreshProcesses(SysState* s) {
    int cap = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (cap <= 0) {
        return;
    }
    // The count can grow between the two calls; ask for some slack.
    int nPids = cap / (int)sizeof(pid_t) + 32;
    Vec<pid_t> pids;
    pid_t* buf = pids.AppendBlanks(nPids);
    if (!buf) {
        return;
    }
    int got =
        proc_listpids(PROC_ALL_PIDS, 0, buf, (int)(nPids * (int)sizeof(pid_t)));
    if (got <= 0) {
        return;
    }
    int count = got / (int)sizeof(pid_t);

    Vec<ProcessInfo> next;
    Vec<ProcSample> samples;

    // Wall clock in the same 100 ns units the CPU totals use.
    static uint64_t sPrevWall = 0;
    struct timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t now =
        (uint64_t)ts.tv_sec * 10000000ull + (uint64_t)(ts.tv_nsec / 100);
    uint64_t wallDelta = (sPrevWall && now > sPrevWall) ? (now - sPrevWall) : 0;
    sPrevWall = now;

    for (int i = 0; i < count; i++) {
        pid_t pid = buf[i];
        if (pid <= 0) {
            continue;
        }
        struct proc_taskallinfo info = {};
        int n = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &info, sizeof(info));
        if (n < (int)sizeof(info)) {
            continue;
        }
        ProcessInfo pi;
        pi.pid = (uint32_t)pid;
        StrCopyZ(
            pi.name, (int)sizeof(pi.name),
            info.pbsd.pbi_name[0] ? info.pbsd.pbi_name : info.pbsd.pbi_comm);
        pi.memory = info.ptinfo.pti_resident_size;
        // Mach reports thread times in nanoseconds.
        uint64_t cpu =
            (info.ptinfo.pti_total_user + info.ptinfo.pti_total_system) / 100;
        uint64_t prev = FindPrevCpu(s->prevProcs, pi.pid);
        if (prev && wallDelta > 0) {
            uint64_t delta = cpu >= prev ? cpu - prev : 0;
            pi.cpu = (float)((double)delta * 100.0 /
                             ((double)wallDelta * (double)s->ncpu));
        }
        ProcSample sm;
        sm.pid = pi.pid;
        sm.cpu100ns = cpu;
        samples.Append(sm);
        next.Append(pi);
    }

    s->prevProcs.Reset();
    s->prevProcs = samples;
    s->procs.Reset();
    s->procs = next;
}

void SysRefresh(SysState* s) {
    RefreshCpu(s);
    RefreshMemory(s);
    RefreshDisk(s);
    RefreshBattery(s);
    RefreshProcesses(s);
}

} // namespace gpui
