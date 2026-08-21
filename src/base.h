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
template <typename T>
struct ArenaVec {
    T* els = nullptr;
    int len = 0;
    int cap = 0;

    T& operator[](int idx) const { return els[idx]; }

    bool Append(Arena* a, const T& el) {
        if (len >= cap) {
            int newCap = cap > 0 ? cap * 2 : 8;
            if (!VecRealloc(a, (void**)&els, len, &cap, newCap,
                            (int)sizeof(T))) {
                return false;
            }
        }
        els[len++] = el;
        return true;
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
