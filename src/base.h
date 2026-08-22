/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

// C/C++ standard headers we use often
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>       // for placement new
#include <algorithm> // for std::min, std::max
#include <utility>   // for std::forward

// ─── os ──────────────────────────────────────────────────────────────────
//
// Exactly one of these is 1 on every build. Prefer a portable function
// implemented in <name>_win.cpp / <name>_linux.cpp / <name>_mac.cpp over an
// #if in shared code: these are for the handful of places where a single
// expression differs.

#if defined(_WIN32)
#define GPUI_OS_WINDOWS 1
#define GPUI_OS_LINUX 0
#define GPUI_OS_MAC 0
#elif defined(__APPLE__)
#define GPUI_OS_WINDOWS 0
#define GPUI_OS_LINUX 0
#define GPUI_OS_MAC 1
#elif defined(__linux__)
#define GPUI_OS_WINDOWS 0
#define GPUI_OS_LINUX 1
#define GPUI_OS_MAC 0
#else
#error "unsupported platform: gpui builds on Windows, Linux and macOS"
#endif

// Everything that is not Windows is a POSIX host here, which is what the
// _posix.cpp half of the platform layer is written against.
#define GPUI_OS_POSIX (!GPUI_OS_WINDOWS)

#if GPUI_OS_WINDOWS
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#else
#include <pthread.h>
#include <limits.h>
#endif

// The SumatraPDF base this tree is built on — Str, Vec, Arena, Func0/Func1,
// fmt/logf, the Plat* platform shims — in a namespace of its own.
//
// It is `base` rather than `gpui` so that a module which is a port of a crate
// that has never heard of gpui can be written against this and nothing else.
// `src/taffy` and `src/markdown` are those modules: they include this header,
// they name `base::Str` and `base::Arena`, and they reach for no other part
// of the tree. gpui itself pulls the whole namespace in with a using-directive
// (see the top of gpui.h), so its own code writes `Str` unqualified the way it
// always has, and `gpui::Str` still names this type for a caller outside.
namespace base {

// The longest path we put on the stack. Windows spells it MAX_PATH; Linux
// PATH_MAX, which is 4096 and too big for the fixed arrays here.
enum {
    kMaxPath = 1024
};

struct Arena;

struct Str {
    char* s;
    int len;

    Str() : s(nullptr), len(0) {}
    Str(const char* s_) : s((char*)s_), len(0) {
        len = s_ ? (int)strlen(s_) : 0;
    }
    explicit Str(const char* s_, int len_) : s((char*)s_), len(len_) {}
    explicit Str(char* s_) : s(s_), len(0) { len = s ? (int)strlen(s) : 0; }
    explicit Str(char* s_, int len_) : s(s_), len(len_) {}

    explicit operator bool() const { return len > 0 && s; }
};

void log(Str s);

using TempStr = Str;

#define StrL(lit) ::base::Str((char*)(lit), (int)(sizeof(lit) - 1))

Str AllocStrTemp(int size);

#if GPUI_OS_WINDOWS
// UTF-8 Str -> null-terminated UTF-16 for the OS calls that need it.
// Temp-arena backed: do not free, and do not keep it past the next
// ResetTempArena().
WCHAR* ToCWstrTemp(Str s);
#endif

// ─── platform layer ──────────────────────────────────────────────────────
//
// Implemented in Base_win.cpp / Base_linux.cpp.

uint64_t PlatPageSize();
uint64_t PlatLargePageSize();
// Reserve address space without backing it. Returns null on failure.
void* PlatMemReserve(uint64_t size);
bool PlatMemCommit(void* base, uint64_t size, bool largePages);
void* PlatMemReserveCommit(uint64_t size, bool largePages);
void PlatMemRelease(void* base, uint64_t size);

// Case-insensitive ASCII compares and a bounded copy: the CRT spells these
// _stricmp / _strnicmp / strncpy_s on Windows and strcasecmp / strncasecmp /
// strlcpy elsewhere.
int StrCmpI(const char* a, const char* b);
int StrCmpNI(const char* a, const char* b, int n);
// Copies at most cap-1 bytes and always null-terminates.
void StrCopyZ(char* dst, int cap, const char* src);

// The filesystem bits the asset loader needs; reading a file is plain stdio.
bool PlatDirExists(const char* path);
void PlatGetCwd(char* out, int cap);
// The directory the running binary sits in.
void PlatGetExeDir(char* out, int cap);

// One entry of a directory listing.
struct DirEntry {
    char name[260] = {};
    bool isDir = false;
};

// Lists `dir`, skipping "." and "..". Returns how many entries were written,
// 0 if the directory could not be read.
int PlatListDir(const char* dir, DirEntry* out, int max);

// Logical cores, at least 1.
int PlatCoreCount();
// This process' own CPU time in 100 ns units and its resident set, for the
// FPS HUD. False if the OS would not say.
bool PlatSelfUsage(uint64_t* cpu100ns, uint64_t* memBytes);

void* AllocZero(int count, int size);

template <typename T>
inline T* AllocArray(int n) {
    return (T*)AllocZero(n, (int)sizeof(T));
}

template <typename T>
inline void ZeroStruct(T* s) {
    memset((void*)s, 0, sizeof(T));
}

struct Func0 {
    void* fn = nullptr;
    uintptr_t userData = 0;

    Func0() = default;

    bool IsValid() const { return fn != nullptr; }
    void Call() const {
        if (!fn) {
            return;
        }
        auto func = (void (*)(uintptr_t))fn;
        func(userData);
    }
};

template <typename T>
Func0 MkFunc0(void (*fn)(T*), T* d) {
    auto res = Func0{};
    res.fn = (void*)fn;
    res.userData = (uintptr_t)d;
    return res;
}

template <typename T>
struct Func1 {
    void (*fn)(uintptr_t, T) = nullptr;
    uintptr_t userData = 0;

    Func1() = default;

    bool IsValid() const { return fn != nullptr; }
    void Call(T arg) const {
        if (!fn) {
            return;
        }
        fn(userData, arg);
    }
};

template <typename T1, typename T2>
Func1<T2> MkFunc1(void (*fn)(T1*, T2), T1* d) {
    auto res = Func1<T2>{};
    using fptr = void (*)(uintptr_t, T2);
    res.fn = (fptr)fn;
    res.userData = (uintptr_t)d;
    return res;
}

// A plain non-recursive lock. Only the arena uses it.
struct Mutex {
#if GPUI_OS_WINDOWS
    SRWLOCK lock = SRWLOCK_INIT;
    void Lock() { AcquireSRWLockExclusive(&lock); }
    void Unlock() { ReleaseSRWLockExclusive(&lock); }
#else
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    void Lock() { pthread_mutex_lock(&lock); }
    void Unlock() { pthread_mutex_unlock(&lock); }
#endif
    Mutex() = default;
    ~Mutex() = default;
};

static const uint64_t kArenaHeaderSize = 256;

struct Arena {
    Arena* prev;
    Arena* current;
    uint64_t flags;
    uint64_t commitChunkSize;
    uint64_t reserveChunkSize;
    uint64_t basePos;
    uint64_t pos;
    uint64_t committed;
    uint64_t reserved;
    const char* allocationSiteFile;
    int allocationSiteLine;
    const char* name;
    bool usesExternalBuffer;
    Mutex lock;
    uint64_t nAllocsLifetime;
    uint64_t peakBytesLifetime;
    uint64_t nAllocsSinceReset;
    uint64_t peakBytesSinceReset;

    void* Alloc(int size);
    void Reset();
    void* Push(uint64_t size, uint64_t align = 8, bool zero = true);
    void PopTo(uint64_t pos);

    Arena() = delete;
    ~Arena() = delete;
};

Arena* ArenaNew();
void ArenaDelete(Arena* arena);

// How many bytes the arena has handed out, across every block in its chain.
// The chain shares one position space — a block's `basePos` is where it
// starts in it — so this is the number a caller means by "how big did it
// get".
uint64_t ArenaUsed(Arena* arena);

// ─── a string that costs eight bytes ──────────────────────────────────────
//
// A `Str` is a pointer and a length: sixteen bytes once the compiler has
// padded it. A structure that holds several of them and is allocated by the
// thousand — an mdast `Node` holds eight — spends most of its size on
// pointers into an arena it already knows the name of.
//
// An ArenaStr is that pair packed into one 64-bit word: the length in the
// upper half, the offset into the arena's position space in the lower. Half
// the size, and no relocation, at the cost of needing the arena to read it
// back.
//
// The offset is the same position space `PopTo` takes, so it stays valid as
// the arena chains onto a new block. Nothing may be freed out from under it:
// an ArenaStr into an arena that has been reset or popped past it is a
// dangling pointer spelled differently.
using ArenaStr = uint64_t;

constexpr ArenaStr kArenaStrNone = 0;

constexpr uint32_t ArenaStrLen(ArenaStr s) {
    return (uint32_t)(s >> 32);
}

// Whether it names anything. A zero-length string that was allocated is still
// a string — `IsSet` asks whether one was stored at all, which is what a
// `Str` with a null `s` means.
constexpr bool ArenaStrIsSet(ArenaStr s) {
    return s != kArenaStrNone;
}

// Copies `src` into `a` and answers where it went. The bytes are
// NUL-terminated the way StrDup's are, so a caller that needs a C string has
// one. An empty or null `src` answers kArenaStrNone without allocating.
ArenaStr ArenaStrDup(Arena* a, Str src);

// The same, for a string already inside `a` — a slice of something the arena
// holds. Answers where it is rather than copying it again, which is what a
// parser that has already put the text in the arena wants. `s` must point
// into `a`, and this is a walk of the chain, so it is for the handful of
// places that can avoid a copy rather than the general case.
ArenaStr ArenaStrRef(Arena* a, Str s);

// Reading one back. The Str points into the arena and lives exactly as long
// as the arena's contents do.
Str ArenaStrGet(Arena* a, ArenaStr s);

// The per-frame scratch arena that fmt() / AllocStrTemp() carve out of.
Arena* GetTempArena();
void ResetTempArena();
void DestroyTempArena();

void* Alloc(struct Arena* arena, int size);
void Free(struct Arena* arena, void* mem);

template <typename T, typename... Args>
T* ArenaNew(Arena* arena, Args&&... args) {
    void* mem = Alloc(arena, (int)sizeof(T));
    return new (mem) T(std::forward<Args>(args)...);
}

// Kept out of line so the Vec<T> templates stay small at every use site.
#if GPUI_OS_WINDOWS
#define GPUI_NOINLINE __declspec(noinline)
#else
#define GPUI_NOINLINE __attribute__((noinline))
#endif

// One arena block for an ArenaVec segment: `hdrSize` bytes of header
// followed by `count` elements. Out of line so the template does not
// carry the overflow checks into every instantiation.
void* ArenaVecAlloc(struct Arena* a, int count, int elSize, int align,
                    int hdrSize = 0);

GPUI_NOINLINE bool VecRealloc(struct Arena* a, void** els, int len, int* cap,
                              int newCap, int elSize);

template <typename T>
struct Vec;

template <typename T>
bool VecReserve(Arena* arena, T& v, int wantedSize);

template <typename T>
inline T* VecReserve(Vec<T>& v, int capNeeded);

template <typename T>
T* VecInsertSpace(Vec<T>& v, int idx, int count);

template <typename T>
struct Vec {
    int len = 0;
    int cap = 0;
    T* els = nullptr;

    void FreeEls() {
        if (els) {
            Free(nullptr, (void*)els);
            els = nullptr;
        }
    }

    void Reset() {
        FreeEls();
        len = 0;
        cap = 0;
    }

    void Clear() {
        len = 0;
        if (els && cap > 0) {
            memset((void*)els, 0, (size_t)cap * sizeof(T));
        }
    }

    explicit Vec() = default;

    Vec(const Vec& other) {
        VecReserve(*this, other.len);
        len = other.len;
        if (other.len > 0 && other.els && els) {
            memcpy((void*)els, (const void*)other.els,
                   sizeof(T) * (size_t)other.len);
        }
    }

    Vec& operator=(const Vec& other) {
        if (this == &other) {
            return *this;
        }
        Reset();
        VecReserve(*this, other.len);
        len = other.len;
        if (other.len > 0) {
            memcpy((void*)els, (const void*)other.els, sizeof(T) * (size_t)len);
            memset((void*)(els + len), 0, sizeof(T) * (size_t)(cap - len));
        }
        return *this;
    }

    ~Vec() { FreeEls(); }

    T& operator[](int idx) const { return els[idx]; }

    bool InsertAt(int idx, const T& el) {
        T* p = VecInsertSpace(*this, idx, 1);
        if (!p) {
            return false;
        }
        p[0] = el;
        return true;
    }

    bool Append(const T& el) { return InsertAt(len, el); }

    T* AppendBlanks(int count) { return VecInsertSpace(*this, len, count); }
};

template <typename T>
bool VecReserve(Arena* arena, T& v, int wantedSize) {
    if (wantedSize <= v.cap) {
        return true;
    }
    int newCap = std::max(v.cap * 2, wantedSize);
    return VecRealloc(arena, (void**)&v.els, v.len, &v.cap, newCap,
                      (int)sizeof(*v.els));
}

template <typename T>
inline T* VecReserve(Vec<T>& v, int capNeeded) {
    if (!VecReserve(nullptr, v, capNeeded)) {
        return nullptr;
    }
    return v.els;
}

template <typename T>
T* VecInsertSpace(Vec<T>& v, int idx, int count) {
    int newLen = std::max(v.len, idx) + count;
    T* ok = VecReserve(v, newLen);
    if (!ok) {
        return nullptr;
    }
    T* res = &(v.els[idx]);
    if (v.len > idx) {
        T* src = v.els + idx;
        T* dst = v.els + idx + count;
        memmove((void*)dst, (const void*)src,
                (size_t)(v.len - idx) * sizeof(T));
    }
    v.len = newLen;
    return res;
}

// An array that grows into an arena. `Vec<T>` frees its storage in its
// destructor, which is wrong for anything built on a frame arena: an element
// tree and the builders that make one are allocated from the frame's arena and
// thrown away with it, never destructed. This has no destructor and never
// frees — the arena is what owns the memory — so a builder can take as many
// items as the caller has instead of declaring a cap for them.
//
// It is a list of segments rather than one block that reallocates. An arena
// hands back only the allocation on top of it, so a `Vec`-style grow of
// anything that is not the newest thing in the arena copies the elements and
// abandons the old block: the elements are copied over and over and the
// abandoned blocks are never reused. Segments never move, so a build is one
// store per element and the only waste is the tail of the last segment.
// Parsing 64 KB of markdown took 3257 KB of scratch arena the old way and
// takes 1538 KB this way, in fewer allocations.
//
// Two things follow from segments never moving. A `T*` or a `T&` taken from
// the vec stays good across an append, which the flat version could not
// promise. And the elements are not contiguous, so there is no one array to
// hand to something that wants one — `Flatten` is that, and it costs nothing
// for a vec that never left its first segment, which is most of them.
template <typename T>
struct ArenaVecSegment {
    ArenaVecSegment<T>* next;
    // Index of this segment's first element in the vec: the sum of the
    // lengths before it. What `operator[]` walks on, and all a truncated
    // segment needs to be handed its elements back.
    int base;
    int len;
    // Only the last segment ever takes an append, so a capacity per segment
    // looks redundant — but `Truncate` makes an earlier segment the active
    // one again and has to know how much room it has.
    int cap;

    // The elements sit right behind the header, in the same arena block, so
    // where they are is an add on a pointer the caller already has rather
    // than a field to store and load. The offset is a constant per T.
    // A function and not a constant: inside its own definition the class is
    // only complete in a member function body, and `sizeof` needs it.
    static constexpr int HeaderSize() {
        return ((int)sizeof(ArenaVecSegment<T>) + (int)alignof(T) - 1) &
               ~((int)alignof(T) - 1);
    }

    T* Els() const { return (T*)((char*)(void*)this + HeaderSize()); }
};

// The first three segment sizes, then doubling. The progression started at
// 16/64/256 and these are what the markdown benchmark asked for: most of
// these vecs are an edit map entry holding two events, or a node holding
// three children, and a first segment they never fill is arena spent for
// nothing. `bun cmd/bench.ts markdown` is the test — 16/64/256 costs 2.4x
// the scratch arena and about 10% on `gfm tables`. Revisit them the same
// way, against a benchmark and not against taste.
constexpr int kArenaVecCap0 = 4;
constexpr int kArenaVecCap1 = 16;
constexpr int kArenaVecCap2 = 64;

template <typename T>
struct ArenaVec {
    using Segment = ArenaVecSegment<T>;

    // The segments holding the elements, oldest first. `last` is the one an
    // append goes into, so appending does not walk; segments past it are ones
    // a `Truncate` let go of, kept linked and empty so a pop and a push at a
    // segment boundary do not allocate a segment per pass.
    Segment* first = nullptr;
    Segment* last = nullptr;
    int len = 0;

    // A walk over the segments, not the elements: one compare for a vec that
    // never left its first, one more per segment after that. Reading the
    // elements in order this way is what `Iter` is for — `v[i]` in a loop
    // starts the walk over every time.
    T& operator[](int idx) const {
        Segment* seg = first;
        // Most vecs never leave their first segment, and `first == last`
        // says so from two fields of the handle itself.
        if (seg == last) {
            return seg->Els()[idx];
        }
        while (idx >= seg->base + seg->len) {
            seg = seg->next;
        }
        return seg->Els()[idx - seg->base];
    }

    // Reading the elements in order. The whole state is the segment and the
    // index inside it, so stepping is `++idx` and a compare, and only the
    // step that leaves a segment touches a pointer:
    //
    //     for (const Event& e : entry.add) { ... }
    //
    // A `Truncate` can leave empty segments linked after the last one, so
    // both ends normalize past anything empty and `end()` is a null segment.
    struct Iter {
        Segment* seg;
        int idx;

        void Normalize() {
            while (seg && idx >= seg->len) {
                seg = seg->next;
                idx = 0;
            }
        }
        T& operator*() const { return seg->Els()[idx]; }
        T* operator->() const { return &seg->Els()[idx]; }
        Iter& operator++() {
            idx++;
            if (idx >= seg->len) {
                seg = seg->next;
                idx = 0;
                Normalize();
            }
            return *this;
        }
        bool operator!=(const Iter& o) const {
            return seg != o.seg || idx != o.idx;
        }
    };

    Iter begin() const {
        Iter it = {first, 0};
        it.Normalize();
        return it;
    }
    Iter end() const { return Iter{nullptr, 0}; }

    bool Append(Arena* a, const T& el) {
        Segment* seg = last;
        if (!seg || seg->len >= seg->cap) {
            seg = NextSegment(a, 1);
            if (!seg) {
                return false;
            }
        }
        seg->Els()[seg->len++] = el;
        len++;
        return true;
    }

    bool AppendMany(Arena* a, const T* src, int n) {
        while (n > 0) {
            Segment* seg = last;
            if (!seg || seg->len >= seg->cap) {
                seg = NextSegment(a, n);
                if (!seg) {
                    return false;
                }
            }
            int room = seg->cap - seg->len;
            int take = n < room ? n : room;
            for (int i = 0; i < take; i++) {
                seg->Els()[seg->len + i] = src[i];
            }
            seg->len += take;
            len += take;
            src += take;
            n -= take;
        }
        return true;
    }

    // Room for `n` more appends without allocating. A caller who knows the
    // count skips the 4/16/64 climb, which is all the segment sizes cost.
    bool Reserve(Arena* a, int n) {
        if (last && last->cap - last->len >= n) {
            return true;
        }
        return NextSegment(a, n) != nullptr;
    }

    // Drop everything from `newLen` on. The segments that held it stay linked
    // and empty, for the next appends to fill again.
    void Truncate(int newLen) {
        if (newLen < 0) {
            newLen = 0;
        }
        if (newLen >= len) {
            return;
        }
        Segment* seg = first;
        while (seg && seg->base + seg->len <= newLen) {
            seg = seg->next;
        }
        if (seg) {
            seg->len = newLen - seg->base;
            last = seg;
            for (Segment* s = seg->next; s; s = s->next) {
                s->len = 0;
            }
        }
        len = newLen;
    }

    void Pop() { Truncate(len - 1); }

    // The elements as one array, for a callee that takes a `const T*`. Free
    // for a vec that never grew past its first segment; a copy into `a`
    // otherwise.
    T* Flatten(Arena* a) const {
        if (len == 0) {
            return nullptr;
        }
        if (first == last) {
            return first->Els();
        }
        T* out = (T*)ArenaVecAlloc(a, len, (int)sizeof(T), (int)alignof(T));
        if (!out) {
            return nullptr;
        }
        int at = 0;
        for (const T& el : *this) {
            out[at++] = el;
        }
        return out;
    }

    static int NextCap(int prevCap) {
        if (prevCap < kArenaVecCap0) {
            return kArenaVecCap0;
        }
        if (prevCap < kArenaVecCap1) {
            return kArenaVecCap1;
        }
        if (prevCap < kArenaVecCap2) {
            return kArenaVecCap2;
        }
        return prevCap * 2;
    }

    // The segment the next append goes into, with room for at least `want`.
    // Off the hot path: once per segment, not once per element.
    GPUI_NOINLINE Segment* NextSegment(Arena* a, int want) {
        Segment* reuse = last ? last->next : nullptr;
        if (reuse && reuse->cap >= want) {
            reuse->len = 0;
            reuse->base = len;
            last = reuse;
            return reuse;
        }
        int cap = NextCap(last ? last->cap : 0);
        if (cap < want) {
            cap = want;
        }
        int align = (int)alignof(T) > 8 ? (int)alignof(T) : 8;
        void* mem =
            ArenaVecAlloc(a, cap, (int)sizeof(T), align, Segment::HeaderSize());
        if (!mem) {
            return nullptr;
        }
        Segment* seg = (Segment*)mem;
        seg->next = nullptr;
        seg->base = len;
        seg->len = 0;
        seg->cap = cap;
        if (last) {
            last->next = seg;
        } else {
            first = seg;
        }
        last = seg;
        return seg;
    }
};

// ─── float geometry ──────────────────────────────────────────────────────
//
// One point, one size and one edge-set, shared by gpui and by the taffy port.
// They were two sets before: taffy's `SizeF`/`PointF`/`RectF` (its `Size<f32>`,
// `Point<f32>` and `Rect<f32>`) and gpui's `Size`/`Point`/`Edges`, identical
// in shape and converted at the seam between them. They are the same types
// now, and gpui keeps its own names for them as aliases, since `Size` and
// `Edges` read better in a widget than `SizeF` and `RectF` do.
//
// Only what both sides use is a method here. Everything taffy does with a
// flex direction or a writing-mode axis — `Main`, `Cross`, `SetMain`, the
// `Rect` edge pickers — is a free function in `namespace taffy`, because the
// axis enums are taffy's and have no business in the base.

// Rust's `Point<f32>`. With a rect, the top-left corner.
struct PointF {
    float x = 0.0f;
    float y = 0.0f;

    static constexpr PointF Zero() { return {0.0f, 0.0f}; }
};

constexpr bool operator==(PointF a, PointF b) {
    return a.x == b.x && a.y == b.y;
}

constexpr bool operator!=(PointF a, PointF b) {
    return !(a == b);
}

constexpr PointF operator+(PointF a, PointF b) {
    return {a.x + b.x, a.y + b.y};
}

// Rust's `Size<f32>`. Rust spells the fields `width` and `height`; they are
// `w` and `h` here, which is what gpui has always called them and what the
// taffy port's own `SizeDim` and `SizeAvail` — which are not this type — still
// spell out in full. Taffy's `SizeFOpt` *is* this type: an `Option<f32>` there
// is a NaN-tagged float, so `Size<Option<f32>>` is `Size<f32>`.
struct SizeF {
    float w = 0.0f;
    float h = 0.0f;

    static constexpr SizeF Zero() { return {0.0f, 0.0f}; }
};

constexpr bool operator==(SizeF a, SizeF b) {
    return a.w == b.w && a.h == b.h;
}

constexpr bool operator!=(SizeF a, SizeF b) {
    return !(a == b);
}

constexpr SizeF operator+(SizeF a, SizeF b) {
    return {a.w + b.w, a.h + b.h};
}

constexpr SizeF operator-(SizeF a, SizeF b) {
    return {a.w - b.w, a.h - b.h};
}

// Rust's `Rect<f32>`: an axis-aligned rectangle, or — which is all either
// caller uses it for — a set of four edge values, so gpui spells it `Edges`.
// The axis sums are the *edge totals*, not a width or a height.
struct RectF {
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;

    static constexpr RectF Zero() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    static constexpr RectF New(float l, float r, float t, float b) {
        return {l, r, t, b};
    }

    constexpr float HorizontalAxisSum() const { return left + right; }
    constexpr float VerticalAxisSum() const { return top + bottom; }
    constexpr SizeF SumAxes() const { return {left + right, top + bottom}; }
};

constexpr bool operator==(RectF a, RectF b) {
    return a.left == b.left && a.right == b.right && a.top == b.top &&
           a.bottom == b.bottom;
}

constexpr bool operator!=(RectF a, RectF b) {
    return !(a == b);
}

constexpr RectF operator+(RectF a, RectF b) {
    return {a.left + b.left, a.right + b.right, a.top + b.top,
            a.bottom + b.bottom};
}

// A calendar date, which is all the date widgets need out of the clock.
struct LocalDate {
    int year = 0;
    int month = 0; // 1..12
    int day = 0;   // 1..31
};

// Today, in local time.
LocalDate DateToday();
// `base` shifted by whole days, normalized. GPUI's date widgets get this from
// chrono's checked_add_days.
LocalDate DateAddDays(LocalDate base, int days);

void StrFree(Str s);
void StrFree(const char*) = delete;

Str StrDup(Arena*, Str str);
Str StrDup(Str s);

bool StrEqI(Str s1, Str s2);
bool StrContainsI(Str s, Str sub);

// ─── sequential strings ───────────────────────────────────────────────────
//
// A run of NUL-terminated strings laid end to end, the run itself ended by an
// empty one:
//
//     "red green blue "
//
// which as a C literal already carries the final NUL. It is smaller than an
// array of `const char*` — no pointer per string and no relocation per
// pointer — and reading it is a linear scan, which the L1 cache is good at.
// The trade is that reaching the nth string means walking the ones before it,
// so this is for tables looked up rarely: a name to an index, an index back
// to a name. Ported from SumatraPDF's `src/base/Str.h`.
using SeqStrings = const char*;

// The string at a byte offset into the run, or {} at its end.
Str SeqStrAt(SeqStrings strs, int off);
// Step `off` past the string it names, and `idx` with it when one is given.
// False at the end of the run, and `off` is left at -1.
bool SeqStrAdvance(SeqStrings strs, int& off, int* idxInOut = nullptr);
// Which string in the run this is, or -1. `IS` ignores case.
int SeqStrIndex(SeqStrings strs, Str toFind);
int SeqStrIndexIS(SeqStrings strs, Str toFind);
// The nth string, or {} past the end of the run.
Str SeqStrByIndex(SeqStrings strs, int idx);
// How many strings the run holds. Not one of Sumatra's — it is here because
// a run that parallels a table has to be as long as the table, and something
// has to be able to say so.
int SeqStrCount(SeqStrings strs);
// Lowercase A-Z in place. ASCII only, which is what the filters using it
// compare.
void StrLowerAscii(char* s);

struct StrBuilder {
    Arena* a = nullptr;
    char* els = nullptr;
    int len = 0;
    int cap = 0;
    Str buf;

    explicit StrBuilder(Str externalBuf = {});
    StrBuilder(const StrBuilder&) = delete;
    StrBuilder& operator=(const StrBuilder&) = delete;
    ~StrBuilder();

    void Reset(Str s = {});
    bool InsertAt(int idx, char el);
    bool AppendChar(char c);
    bool Append(Str src);
    Str TakeStr();
};

struct FmtArg {
    enum class Kind {
        Char,
        Int,
        Ptr,
        Float,
        Double,
        Str,
        RawStr,
        Any,
        None,
    };

    Kind t{Kind::None};
    union {
        Str str;
        char c;
        int64_t i;
        float f;
        double d;
        const void* ptr;
    };

    FmtArg() : i{0} {}
    explicit FmtArg(char c_) : t{Kind::Char}, c{c_} {}
    explicit FmtArg(int arg) : t{Kind::Int}, i{(int64_t)arg} {}
    explicit FmtArg(unsigned int arg) : t{Kind::Int}, i{(int64_t)arg} {}
    explicit FmtArg(long arg) : t{Kind::Int}, i{(int64_t)arg} {}
    explicit FmtArg(unsigned long arg) : t{Kind::Int}, i{(int64_t)arg} {}
    explicit FmtArg(long long arg) : t{Kind::Int}, i{(int64_t)arg} {}
    explicit FmtArg(unsigned long long arg) : t{Kind::Int}, i{(int64_t)arg} {}
    explicit FmtArg(float f_) : t{Kind::Float}, f{f_} {}
    explicit FmtArg(double d_) : t{Kind::Double}, d{d_} {}
    explicit FmtArg(Str arg) : t{Kind::Str}, str{arg} {}
    explicit FmtArg(const void* p) : t{Kind::Ptr}, ptr{p} {}
    FmtArg(char*) = delete;
    FmtArg(const char*) = delete;
    FmtArg(wchar_t*) = delete;
    FmtArg(const wchar_t*) = delete;
};

TempStr FormatTempArgs(const char* fmt, const FmtArg** args, int nArgs);

inline TempStr FormatTemp(const char* fmt) {
    return FormatTempArgs(fmt, nullptr, 0);
}

template <typename... TArgs>
TempStr FormatTemp(const char* fmt, const TArgs&... args) {
    const FmtArg argv[] = {FmtArg(args)...};
    const FmtArg* argp[sizeof...(TArgs)];
    int n = (int)sizeof...(TArgs);
    for (int i = 0; i < n; i++) {
        argp[i] = &argv[i];
    }
    return FormatTempArgs(fmt, argp, n);
}

template <typename... TArgs>
inline TempStr fmt(const char* format, const TArgs&... args) {
    return FormatTemp(format, args...);
}

template <typename... TArgs>
inline void logf(const char* format, const TArgs&... args) {
    log(FormatTemp(format, args...));
}
} // namespace base
