/* The benchmark runner. Builds like the test runner — it implements GpuiMain
   and links the same amalgam, with a console subsystem on Windows so its
   report reaches the terminal.

   This file is criterion's replacement: the sample loop, the timer and the
   report. The benchmarks themselves are in the files beside it: the layout
   ones each named after the one in taffy's `benches/benches/` it came from,
   and `MarkdownBench.cpp`, which is ours. */

#include "Bench.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gBenchSamples = 10;
bool gBenchSmall = false;
bool gBenchLarge = false;
const char* gBenchFilter = nullptr;

// The last group printed, so a group heading appears once above its rows.
static const char* gLastGroup = nullptr;

static volatile const void* gSink = nullptr;

void BenchKeep(const void* p) {
    gSink = p;
}

static bool Contains(const char* haystack, const char* needle) {
    return haystack && needle && strstr(haystack, needle) != nullptr;
}

void BenchMem(const char* group, const char* name, int64_t param,
              uint64_t bytes) {
    BenchMemAs(group, name, "mem", param, bytes);
}

void BenchMemAs(const char* group, const char* name, const char* what,
                int64_t param, uint64_t bytes) {
    if (!BenchWanted(group, name)) {
        return;
    }
    double kb = (double)bytes / 1024.0;
    double ratio = param > 0 ? (double)bytes / (double)param : 0.0;
    printf("  %-34s %14s   %-7s %8.1f KB   %5.2fx source\n", name, "", what,
           kb, ratio);
}

bool BenchWanted(const char* group, const char* name) {
    if (!gBenchFilter) {
        return true;
    }
    return Contains(group, gBenchFilter) || Contains(name, gBenchFilter);
}

// Insertion sort: the sample count is ten, or whatever -n says, and this runs
// once per benchmark.
static void SortTimes(double* v, int n) {
    for (int i = 1; i < n; i++) {
        double x = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > x) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = x;
    }
}

// Milliseconds, with enough digits to see a small change at either scale.
static void FormatMs(double secs, char* buf, int bufSize) {
    double ms = secs * 1000.0;
    if (ms < 1.0) {
        snprintf(buf, (size_t)bufSize, "%.3f", ms);
    } else if (ms < 100.0) {
        snprintf(buf, (size_t)bufSize, "%.2f", ms);
    } else {
        snprintf(buf, (size_t)bufSize, "%.1f", ms);
    }
}

void BenchCase(const char* group, const char* name, const char* unit,
               int64_t param, Func0 setup, Func0 run) {
    if (!BenchWanted(group, name)) {
        return;
    }
    if (gLastGroup != group) {
        printf("\n%s\n", group);
        gLastGroup = group;
    }

    int n = gBenchSamples;
    Vec<double> times;
    VecReserve(times, n);

    for (int i = 0; i < n; i++) {
        // Rust's `iter_batched`: the setup is outside the measurement.
        setup.Call();
        double start = TimeNow();
        run.Call();
        double elapsed = TimeNow() - start;
        times.Append(elapsed);
    }

    SortTimes(times.els, times.len);
    double med = times.len & 1
                     ? times[times.len / 2]
                     : (times[times.len / 2 - 1] + times[times.len / 2]) / 2.0;

    char medBuf[32];
    char minBuf[32];
    FormatMs(med, medBuf, sizeof(medBuf));
    FormatMs(times[0], minBuf, sizeof(minBuf));

    char paramBuf[64];
    snprintf(paramBuf, sizeof(paramBuf), "%lld %s", (long long)param, unit);
    printf("  %-34s %14s   med %8s ms   min %8s ms\n", name, paramBuf, medBuf,
           minBuf);
    fflush(stdout);
}

static const char* kUsage =
    "Usage: bun cmd/bench.ts [-rel|-dbg] [-small] [-large] [-n=<samples>]\n"
    "                        [<filter>]\n"
    "\n"
    "  -small       add the 1,000-node and depth-50 cases (the crate's\n"
    "               `small` cargo feature)\n"
    "  -large       add the 100,000-node and depth-200 cases (`large`)\n"
    "  -n=<count>   samples per benchmark; default 10\n"
    "  <filter>     run only benchmarks whose group or name contains this\n";

int GpuiMain(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (strcmp(a, "-small") == 0 || strcmp(a, "--small") == 0) {
            gBenchSmall = true;
        } else if (strcmp(a, "-large") == 0 || strcmp(a, "--large") == 0) {
            gBenchLarge = true;
        } else if (strncmp(a, "-n=", 3) == 0) {
            gBenchSamples = atoi(a + 3);
            if (gBenchSamples < 1) {
                gBenchSamples = 1;
            }
        } else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            printf("%s", kUsage);
            return 0;
        } else if (a[0] == '-') {
            printf("Unknown flag: %s\n\n%s", a, kUsage);
            return 1;
        } else {
            gBenchFilter = a;
        }
    }

    printf("benchmarks: %d samples", gBenchSamples);
    if (gBenchSmall) {
        printf(", +small");
    }
    if (gBenchLarge) {
        printf(", +large");
    }
    if (gBenchFilter) {
        printf(", filter \"%s\"", gBenchFilter);
    }
    printf("\n");

    double started = TimeNow();
    BenchTreeCreation();
    BenchFlexbox();
    BenchGrid();
    BenchMarkdown();
    printf("\nelapsed %.1fs\n", TimeNow() - started);
    return 0;
}
