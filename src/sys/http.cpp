/* The portable half of the network: what a URL is, and the table that lets a
   paint ask for one without waiting for it.

   The fetching itself is `HttpGet` in http_win.cpp / http_mac.cpp /
   http_linux.cpp, one per platform's own library. Everything here is the same
   on all three. */

#include "sys/http.h"
#include "sys/executor.h"

namespace gpui {

void HttpRspFree(HttpRsp* r) {
    if (!r) {
        return;
    }
    r->body.Reset();
    if (r->contentType.s) {
        StrFree(r->contentType);
        r->contentType = {};
    }
    StrFree(r->redirectUrl);
    r->redirectUrl = {};
    r->status = 0;
}

bool HttpUrlIsRemote(Str url) {
    if (!url.s || url.len <= 0) {
        return false;
    }
    return base::StrStartsWithI(url, "http://") ||
           base::StrStartsWithI(url, "https://");
}

// ─── the table ────────────────────────────────────────────────────────────
//
// A document holds a handful of pictures, so this is a handful of slots and
// no more. A slot the worker still owns is never reused; everything else is
// fair game, oldest first, and evicting one drops the bytes it held.

struct FetchSlot {
    Str url = {}; // owned; stable for as long as the slot is Pending
    FetchState state = FetchState::None;
    uint8_t* body = nullptr; // owned
    int len = 0;
};

constexpr int kFetchSlots = 24;
// Enough that a page of pictures arrives together, few enough that a document
// pointing at a hundred URLs does not open a hundred connections.
constexpr int kMaxConcurrent = 4;

static FetchSlot gFetch[kFetchSlots];
static Mutex gFetchLock;
static int gFetchPending = 0;
static int gFetchNext = 0;
static bool gHttpEnabled = true;
// Read on the main thread when a fetch is started and run on the main thread
// when it lands, so it needs no lock of its own.
static Func0 gOnFetchDone;

void HttpSetOnFetchDone(Func0 f) {
    gOnFetchDone = f;
}

void HttpSetEnabled(bool on) {
    gFetchLock.Lock();
    gHttpEnabled = on;
    gFetchLock.Unlock();
}

bool HttpEnabled() {
    gFetchLock.Lock();
    bool on = gHttpEnabled;
    gFetchLock.Unlock();
    return on;
}

int HttpFetchPending() {
    gFetchLock.Lock();
    int n = gFetchPending;
    gFetchLock.Unlock();
    return n;
}

// What the worker is handed. Its own copy of the URL, because the slot's is
// only stable while the slot is Pending and this outlives that by a hair.
struct FetchJob {
    int slot = 0;
    Str url = {};
};

static void SlotDrop(FetchSlot* s) {
    if (s->body) {
        Free(nullptr, s->body);
        s->body = nullptr;
    }
    s->len = 0;
    if (s->url.s) {
        StrFree(s->url);
        s->url = {};
    }
    s->state = FetchState::None;
}

// Runs on a pool thread. The slot index is still ours: eviction skips
// Pending.
static void FetchWorker(FetchJob* job) {
    HttpRsp r;
    bool ok = HttpGet(job->url, &r);
    bool got = ok && r.status >= 200 && r.status < 300 && r.body.len > 0;

    gFetchLock.Lock();
    FetchSlot* s = &gFetch[job->slot];
    if (got) {
        // The body moves rather than copies: Vec's assignment is a deep copy
        // and this is the one place that would rather not pay for it.
        s->body = r.body.els;
        s->len = r.body.len;
        r.body.els = nullptr;
        r.body.len = 0;
        r.body.cap = 0;
        s->state = FetchState::Done;
    } else {
        s->state = FetchState::Failed;
    }
    gFetchPending--;
    gFetchLock.Unlock();

    HttpRspFree(&r);
    StrFree(job->url);
    Free(nullptr, job);
}

// The lock is held. Null when every slot is either Pending or the table is
// full of them.
static FetchSlot* SlotFree() {
    for (int i = 0; i < kFetchSlots; i++) {
        int ix = (gFetchNext + i) % kFetchSlots;
        FetchSlot* s = &gFetch[ix];
        if (s->state == FetchState::Pending) {
            continue;
        }
        gFetchNext = (ix + 1) % kFetchSlots;
        SlotDrop(s);
        return s;
    }
    return nullptr;
}

FetchState HttpFetch(Str url, const uint8_t** bytes, int* len) {
    if (bytes) {
        *bytes = nullptr;
    }
    if (len) {
        *len = 0;
    }
    if (!HttpUrlIsRemote(url)) {
        return FetchState::Failed;
    }

    gFetchLock.Lock();
    for (int i = 0; i < kFetchSlots; i++) {
        FetchSlot* s = &gFetch[i];
        if (s->state == FetchState::None || !base::StrEq(s->url, url)) {
            continue;
        }
        FetchState st = s->state;
        if (st == FetchState::Done) {
            if (bytes) {
                *bytes = s->body;
            }
            if (len) {
                *len = s->len;
            }
        }
        gFetchLock.Unlock();
        return st;
    }
    // Nothing knows this URL. Start it, unless we are already busy or the
    // application has said not to — either way the caller asks again.
    if (!gHttpEnabled || gFetchPending >= kMaxConcurrent) {
        gFetchLock.Unlock();
        return FetchState::None;
    }
    FetchSlot* s = SlotFree();
    if (!s) {
        gFetchLock.Unlock();
        return FetchState::None;
    }
    FetchJob* job = AllocArray<FetchJob>(1);
    if (!job) {
        gFetchLock.Unlock();
        return FetchState::None;
    }
    s->url = StrDup(url);
    s->state = FetchState::Pending;
    job->slot = (int)(s - gFetch);
    job->url = StrDup(url);
    gFetchPending++;
    gFetchLock.Unlock();

    // The pool, not a thread of its own: a document pointing at four pictures
    // asks for four of these at once, and kMaxConcurrent is what keeps that
    // from being four more threads every time.
    if (!ExecSpawn(MkFunc0(FetchWorker, job), gOnFetchDone)) {
        gFetchLock.Lock();
        gFetchPending--;
        SlotDrop(s);
        gFetchLock.Unlock();
        StrFree(job->url);
        Free(nullptr, job);
        return FetchState::None;
    }
    return FetchState::Pending;
}

void HttpFetchClear() {
    // AppFree calls this, so it must not hold a quit open: a transfer waiting
    // on a name that will not resolve has fifteen seconds of its own to run
    // out, and nobody is closing a window to watch that. Wait a moment for
    // one that is nearly there and then stop.
    //
    // Stopping early costs nothing. A worker writes into its slot, so a slot
    // that is still Pending is not this function's to free — it keeps its
    // body until the process ends, which is the next thing that happens. The
    // slots themselves are static and outlive every thread.
    const int kWaitMs = 500;
    for (int waited = 0; waited < kWaitMs; waited += 10) {
        if (HttpFetchPending() == 0) {
            break;
        }
        PlatSleepMs(10);
    }
    gFetchLock.Lock();
    for (int i = 0; i < kFetchSlots; i++) {
        if (gFetch[i].state != FetchState::Pending) {
            SlotDrop(&gFetch[i]);
        }
    }
    gFetchNext = 0;
    gFetchLock.Unlock();
}

} // namespace gpui
