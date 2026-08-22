/* The threading the fetch table needs, on both POSIX platforms. The GET
   itself differs — NSURLSession on macOS, libcurl on Linux — so it lives in
   http_mac.cpp and http_linux.cpp beside this. */

#include "sys/http.h"

#include <time.h>

namespace gpui {

struct ThreadStart {
    void (*fn)(void*) = nullptr;
    void* arg = nullptr;
};

static void* ThreadEntry(void* p) {
    ThreadStart* t = (ThreadStart*)p;
    void (*fn)(void*) = t->fn;
    void* arg = t->arg;
    Free(nullptr, t);
    fn(arg);
    return nullptr;
}

void ThreadRunDetached(void (*fn)(void*), void* arg) {
    ThreadStart* t = AllocArray<ThreadStart>(1);
    if (!t) {
        return;
    }
    t->fn = fn;
    t->arg = arg;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    // Nothing joins it: filling in the fetch table is how a worker reports.
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t th;
    int err = pthread_create(&th, &attr, ThreadEntry, t);
    pthread_attr_destroy(&attr);
    if (err != 0) {
        Free(nullptr, t);
    }
}

void ThreadSleepMs(int ms) {
    if (ms <= 0) {
        return;
    }
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, nullptr);
}

} // namespace gpui
