#include "sys/SysInfo.h"

namespace gpui {

static int CmpStrI(const char* a, const char* b) {
    return StrCmpI(a, b);
}

struct SortCtx {
    ProcessSort field;
    bool desc;
};

static int CmpProc(const ProcessInfo* a, const ProcessInfo* b,
                   ProcessSort field, bool desc) {
    int c = 0;
    switch (field) {
        case ProcessSort::Pid:
            c = (a->pid > b->pid) - (a->pid < b->pid);
            break;
        case ProcessSort::Name:
            c = CmpStrI(a->name, b->name);
            break;
        case ProcessSort::Cpu:
            c = (a->cpu > b->cpu) - (a->cpu < b->cpu);
            if (c == 0) {
                c = (a->memory > b->memory) - (a->memory < b->memory);
            }
            break;
        case ProcessSort::Memory:
            c = (a->memory > b->memory) - (a->memory < b->memory);
            break;
    }
    return desc ? -c : c;
}

static ProcessSort gSortField = ProcessSort::Cpu;
static bool gSortDesc = true;

static int QsortProc(const void* x, const void* y) {
    return CmpProc((const ProcessInfo*)x, (const ProcessInfo*)y, gSortField,
                   gSortDesc);
}

void SysSortProcesses(SysState* s, ProcessSort field, bool descending,
                      int keepTop) {
    if (s->procs.len <= 1) {
        return;
    }
    gSortField = field;
    gSortDesc = descending;
    qsort(s->procs.els, (size_t)s->procs.len, sizeof(ProcessInfo), QsortProc);
    if (keepTop > 0 && s->procs.len > keepTop) {
        s->procs.len = keepTop;
    }
}

TempStr FormatBytes(uint64_t bytes) {
    const uint64_t KB = 1024;
    const uint64_t MB = KB * 1024;
    const uint64_t GB = MB * 1024;
    if (bytes >= GB) {
        return fmt("%.1f GB", (double)bytes / (double)GB);
    }
    if (bytes >= MB) {
        return fmt("%.1f MB", (double)bytes / (double)MB);
    }
    if (bytes >= KB) {
        return fmt("%.1f KB", (double)bytes / (double)KB);
    }
    return fmt("%d B", (int)bytes);
}

TempStr FormatPct(float v, int decimals) {
    if (decimals <= 0) {
        return fmt("%.0f%%", v);
    }
    return fmt("%.1f%%", v);
}
} // namespace gpui
