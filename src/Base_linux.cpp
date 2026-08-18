/* Linux half of the Base platform layer: mmap-backed virtual memory and the
   POSIX spellings of the case-insensitive string calls. */

#include "Base.h"

#include <dirent.h>
#include <stdio.h>
#include <strings.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

namespace gpui {

uint64_t PlatPageSize() {
    static uint64_t pageSize = 0;
    if (pageSize == 0) {
        long n = sysconf(_SC_PAGESIZE);
        pageSize = n > 0 ? (uint64_t)n : 4096;
    }
    return pageSize;
}

// Transparent huge pages are 2 MB on x86-64 and arm64. The arena only uses
// this to round a reservation, and MAP_HUGETLB needs a preallocated pool, so
// large pages are never actually requested below.
uint64_t PlatLargePageSize() {
    return 2ull * 1024ull * 1024ull;
}

// mmap has no reserve/commit split: PROT_NONE reserves the address range, and
// mprotect on a subrange is the commit.
void* PlatMemReserve(uint64_t size) {
    if (size == 0) {
        return nullptr;
    }
    void* p = mmap(nullptr, (size_t)size, PROT_NONE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    return p == MAP_FAILED ? nullptr : p;
}

bool PlatMemCommit(void* base, uint64_t size, bool largePages) {
    (void)largePages;
    if (size == 0) {
        return true;
    }
    if (!base) {
        return false;
    }
    // Round out to whole pages: the arena commits in its own chunk size, which
    // is not required to be page-aligned at the tail.
    uint64_t page = PlatPageSize();
    uintptr_t start = (uintptr_t)base & ~(uintptr_t)(page - 1);
    uintptr_t end =
        ((uintptr_t)base + (uintptr_t)size + page - 1) & ~(uintptr_t)(page - 1);
    return mprotect((void*)start, (size_t)(end - start),
                    PROT_READ | PROT_WRITE) == 0;
}

void* PlatMemReserveCommit(uint64_t size, bool largePages) {
    (void)largePages;
    if (size == 0) {
        return nullptr;
    }
    void* p = mmap(nullptr, (size_t)size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? nullptr : p;
}

void PlatMemRelease(void* base, uint64_t size) {
    if (base && size > 0) {
        munmap(base, (size_t)size);
    }
}

int StrCmpI(const char* a, const char* b) {
    return strcasecmp(a ? a : "", b ? b : "");
}

int StrCmpNI(const char* a, const char* b, int n) {
    if (n <= 0) {
        return 0;
    }
    return strncasecmp(a ? a : "", b ? b : "", (size_t)n);
}

void StrCopyZ(char* dst, int cap, const char* src) {
    if (!dst || cap <= 0) {
        return;
    }
    if (!src) {
        dst[0] = 0;
        return;
    }
    int n = (int)strlen(src);
    if (n > cap - 1) {
        n = cap - 1;
    }
    memcpy(dst, src, (size_t)n);
    dst[n] = 0;
}

bool PlatDirExists(const char* path) {
    if (!path || !path[0]) {
        return false;
    }
    struct stat st = {};
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

void PlatGetCwd(char* out, int cap) {
    if (!out || cap <= 0) {
        return;
    }
    out[0] = 0;
    if (!getcwd(out, (size_t)cap)) {
        out[0] = 0;
    }
}

void PlatGetExeDir(char* out, int cap) {
    if (!out || cap <= 0) {
        return;
    }
    out[0] = 0;
    ssize_t n = readlink("/proc/self/exe", out, (size_t)cap - 1);
    if (n <= 0) {
        return;
    }
    out[n] = 0;
    int i = (int)n;
    while (i > 0 && out[i - 1] != '/') {
        out[--i] = 0;
    }
    while (i > 1 && out[i - 1] == '/') {
        out[--i] = 0;
    }
}

int PlatListDir(const char* dir, DirEntry* out, int max) {
    if (!dir || !out || max <= 0) {
        return 0;
    }
    DIR* d = opendir(dir);
    if (!d) {
        return 0;
    }
    int n = 0;
    struct dirent* ent = nullptr;
    while (n < max && (ent = readdir(d)) != nullptr) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        DirEntry& e = out[n];
        StrCopyZ(e.name, (int)sizeof(e.name), ent->d_name);
        if (ent->d_type == DT_DIR) {
            e.isDir = true;
        } else if (ent->d_type == DT_UNKNOWN) {
            // Some filesystems do not fill d_type; ask the hard way.
            char full[kMaxPath];
            snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
            e.isDir = PlatDirExists(full);
        } else {
            e.isDir = false;
        }
        n++;
    }
    closedir(d);
    return n;
}

int PlatCoreCount() {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (int)n : 1;
}

bool PlatSelfUsage(uint64_t* cpu100ns, uint64_t* memBytes) {
    // getrusage gives the CPU time; the resident set only comes out of
    // /proc/self/statm, whose second field is pages.
    struct rusage ru = {};
    if (getrusage(RUSAGE_SELF, &ru) != 0) {
        return false;
    }
    if (cpu100ns) {
        uint64_t us = (uint64_t)ru.ru_utime.tv_sec * 1000000ull +
                      (uint64_t)ru.ru_utime.tv_usec +
                      (uint64_t)ru.ru_stime.tv_sec * 1000000ull +
                      (uint64_t)ru.ru_stime.tv_usec;
        *cpu100ns = us * 10ull;
    }
    if (memBytes) {
        *memBytes = 0;
        FILE* f = fopen("/proc/self/statm", "rb");
        if (f) {
            unsigned long total = 0, resident = 0;
            if (fscanf(f, "%lu %lu", &total, &resident) == 2) {
                long page = sysconf(_SC_PAGESIZE);
                *memBytes =
                    (uint64_t)resident * (uint64_t)(page > 0 ? page : 4096);
            }
            fclose(f);
        }
    }
    return true;
}

} // namespace gpui
