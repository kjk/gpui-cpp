/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#pragma once

/* OS_DARWIN - Any Darwin-based OS, including Mac OS X and iPhone OS */
#ifdef __APPLE__
#define OS_DARWIN 1
#else
#define OS_DARWIN 0
#endif

/* OS_LINUX - Linux */
#ifdef __linux__
#define OS_LINUX 1
#else
#define OS_LINUX 0
#endif

#if defined(_WIN32)
#define OS_WIN 1
#else
#define OS_WIN 0
#endif

// https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros
#if defined(_M_IX86) || defined(__i386__)
#define IS_INTEL_32 1
#define IS_INTEL_64 0
#define IS_ARM_64 0
#elif defined(_M_X64) || defined(__x86_64__)
#define IS_INTEL_64 1
#define IS_INTEL_32 0
#define IS_ARM_64 0
#elif defined(_M_ARM64) || defined(__aarch64__) || defined(__arm64__)
#define IS_INTEL_64 0
#define IS_INTEL_32 0
#define IS_ARM_64 1
#else
#error "unsupported arch"
#endif

/* OS_POSIX - Any POSIX-like system */
#if OS_DARWIN || OS_LINUX || defined(unix) || defined(__unix) || defined(__unix__)
#define OS_POSIX 1
#else
#define OS_POSIX 0
#endif

#if defined(_MSC_VER)
#define COMPILER_MSVC 1
#else
#define COMPILER_MSVC 0
#endif

#if defined(__GNUC__)
#define COMPILER_GCC 1
#else
#define COMPILER_GCC 0
#endif

#if defined(__clang__)
#define COMPILER_CLANG 1
#else
#define COMPILER_CLANG 0
#endif

#if defined(__MINGW32__)
#define COMPILER_MINGW 1
#else
#define COMPILER_MINGW 0
#endif

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

// C/C++ standard headers  we use often
#include <cctype>
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>
#include <new>       // for placement new
#include <algorithm> // for std::min, std::max
#include <utility>   // for std::forward
#if OS_POSIX
// pthread.h first: glibc mutex structs have a field named __unused
#include <pthread.h>
#include <strings.h>
#endif

// after system headers so we don't rewrite pthread's __unused field
#define __unused [[maybe_unused]]

#define _USE_MATH_DEFINES
#include <math.h>

#if OS_WIN
#define NOMINMAX
#include <winsock2.h> // must include before <windows.h>
#include <windows.h>
#include <ws2def.h>
#include <unknwn.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <windowsx.h>
#include <winsafer.h>
#include <wininet.h>
#include <versionhelpers.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <ole2.h>
#include <uxtheme.h>

// nasty but necessary
#if defined(min) || defined(max)
#error "min or max defined"
#endif
// mingw's gdiplus.h includes <math.h> which in C++ pulls in <cmath>/<limits>
// that use min/max as identifiers; pre-include them before defining macros
#ifdef __GNUC__
#include <cmath>
#endif
#define min(x, y) ((x) < (y) ? (x) : (y))
#define max(x, y) ((x) > (y) ? (x) : (y))
// /analyze flags a bogus C6385 (invalid read) inside GdiplusFontCollection.h;
// it's a false positive in the SDK header, so silence it at the include site.
#pragma warning(push)
#pragma warning(disable : 6385)
#include <gdiplus.h>
#pragma warning(pop)
#undef NOMINMAX
#undef min
#undef max

#else
using BYTE = uint8_t;
using WORD = uint16_t;
using DWORD = uint32_t;
using DWORD64 = uint64_t;
using UINT = unsigned int;
using UINT_PTR = uintptr_t;
using LONG = int32_t;
using BOOL = int;
using WCHAR = wchar_t;
using WPARAM = uintptr_t;
using LPARAM = intptr_t;
using LRESULT = intptr_t;
using LCID = uint32_t;

struct HWND__;
using HWND = HWND__*;
struct HDC__;
using HDC = HDC__*;
struct HFONT__;
using HFONT = HFONT__*;
struct HIMAGELIST__;
using HIMAGELIST = HIMAGELIST__*;
struct HTREEITEM__;
using HTREEITEM = HTREEITEM__*;
struct HBITMAP__;
using HBITMAP = HBITMAP__*;
struct HBRUSH__;
using HBRUSH = HBRUSH__*;
using LPWSTR = WCHAR*;

struct EXCEPTION_POINTERS;
struct MINIDUMP_EXCEPTION_INFORMATION;

struct FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
};

#define CP_ACP 0
#define CP_UTF8 65001
#define LOCALE_USER_DEFAULT 0
#define LOCALE_INVARIANT 0
#define __TEXT(s) L##s
#define TEXT(s) __TEXT(s)
constexpr int MAX_PATH = 4096;
constexpr int URLZONE_INVALID = -1;
constexpr int URLZONE_INTERNET = 3;

#define ZeroMemory(Destination, Length) memset((Destination), 0, (Length))
#endif

using i8 = int8_t;
using u8 = uint8_t;
using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;
using u32 = uint32_t;
using i64 = int64_t;
using u64 = uint64_t;
using uint = unsigned int;

struct Arena;

struct Str {
    char* s;
    int len;

    Str() : s(nullptr), len(0) {}
    Str(const char* s_) : s((char*)s_), len(0) { len = s_ ? (int)strlen(s_) : 0; }
    explicit Str(const char* s_, int len_) : s((char*)s_), len(len_) {}
    explicit Str(char* s_) : s(s_), len(0) { len = s ? (int)strlen(s) : 0; }
    explicit Str(char* s_, int len_) : s(s_), len(len_) {}

    explicit operator bool() const { return len > 0 && s; }
};

using TempStr = Str;

#define StrL(lit) Str((char*)(lit), (int)(sizeof(lit) - 1))

Str AllocStrTemp(int size);

struct WStr {
    wchar_t* s;
    int len;

    WStr() : s(nullptr), len(0) {}
    WStr(const wchar_t* s_) : s((wchar_t*)s_), len(0) {
        while (s_ && s_[len]) len++;
    }
    explicit WStr(const wchar_t* s_, int len_) : s((wchar_t*)s_), len(len_) {}
    explicit WStr(wchar_t* s_) : s(s_), len(0) {
        while (s && s[len]) len++;
    }
    explicit WStr(wchar_t* s_, int len_) : s((wchar_t*)s_), len(len_) {}

    explicit operator bool() const { return len > 0 && s; }
};

inline int len(Str s) {
    return s.len;
}
inline int len(WStr s) {
    return s.len;
}

#if COMPILER_MSVC
#define NO_INLINE __declspec(noinline)
#define FORCEINLINE __forceinline
#else
#define NO_INLINE __attribute__((noinline))
#define FORCEINLINE inline __attribute__((always_inline))
#endif

template <typename T, size_t N>
char (&DimofSizeHelper(T (&array)[N]))[N];
#define dimof(array) (sizeof(DimofSizeHelper(array)))
#define dimofi(array) (int)(sizeof(DimofSizeHelper(array)))
#define sizeofi(x) ((int)sizeof(x))

#if !defined(__analysis_assume)
#define __analysis_assume(x)
#endif

extern void _uploadDebugReport(Str, Str, bool, bool);

#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)
#define FILE_LINE __FILE__ ":" STRINGIZE(__LINE__)

#define ReportIfCond(cond, condStr, fileLine, isCrash, captureCallstack)      \
    __analysis_assume(!(cond));                                               \
    do {                                                                      \
        if (cond) {                                                           \
            _uploadDebugReport(condStr, fileLine, isCrash, captureCallstack); \
        }                                                                     \
    } while (0)

#define ReportIf(cond) ReportIfCond(cond, #cond, FILE_LINE, false, true)

void log(Str s);
void loga(Str s);

#define logf(...)                     \
    do {                              \
        Str s__ = ::fmt(__VA_ARGS__); \
        ::log(s__);                   \
    } while (0)

void* AllocZero(int count, int size);

template <typename T>
FORCEINLINE T* AllocArray(int n) {
    return (T*)AllocZero(n, sizeofi(T));
}

template <typename T>
inline void ZeroStruct(T* s) {
    ZeroMemory((void*)s, sizeof(T));
}

bool memeq(const void* s1, const void* s2, int n);
u32 MurmurHash2(const void* key, int n);
u32 MurmurHash2(Str s);

using func0Ptr = void (*)(void*);
using funcVoidPtr = void (*)();

#define kFuncNoArg ((void*)~(uintptr_t)1)

struct Func0 {
    void* fn = nullptr;
    void* userData = nullptr;

    Func0() = default;
    Func0(const Func0& that) {
        this->fn = that.fn;
        this->userData = that.userData;
    }
    Func0& operator=(const Func0& that) {
        if (this != &that) {
            this->fn = that.fn;
            this->userData = that.userData;
        }
        return *this;
    }
    ~Func0() = default;

    bool IsEmpty() const { return fn == nullptr; }
    bool IsValid() const { return fn != nullptr; }
    void Call() const {
        if (!fn) {
            return;
        }
        if (userData == kFuncNoArg) {
            auto func = (funcVoidPtr)fn;
            func();
            return;
        }
        auto func = (func0Ptr)fn;
        func(userData);
    }
};

template <typename T>
Func0 MkFunc0(void (*fn)(T*), T* d) {
    auto res = Func0{};
    res.fn = (void*)fn;
    res.userData = (void*)d;
    return res;
}

template <typename T>
struct Func1 {
    static constexpr uintptr_t kDropsArgBit = 1;

    void (*fn)(void*, T) = nullptr;
    uintptr_t userData = 0;

    Func1() = default;
    Func1(const Func0& that) {
        this->fn = (void (*)(void*, T))that.fn;
        this->SetData(that.userData, true);
    }
    Func1(const Func1& that) {
        this->fn = that.fn;
        this->userData = that.userData;
    }
    Func1& operator=(const Func1& that) {
        if (this != &that) {
            this->fn = that.fn;
            this->userData = that.userData;
        }
        return *this;
    }
    ~Func1() = default;

    void SetData(void* d, bool dropsArg) {
        ReportIf(((uintptr_t)d & kDropsArgBit) != 0);
        userData = (uintptr_t)d | (dropsArg ? kDropsArgBit : 0);
    }
    bool IsValid() const { return fn != nullptr; }
    void Call(T arg) const {
        if (!fn) {
            return;
        }
        void* d = (void*)(userData & ~kDropsArgBit);
        if (userData & kDropsArgBit) {
            if (d == kFuncNoArg) {
                auto func = (funcVoidPtr)fn;
                func();
            } else {
                auto func = (func0Ptr)fn;
                func(d);
            }
            return;
        }
        if (d == kFuncNoArg) {
            using fptr = void (*)(T);
            auto func = (fptr)fn;
            func(arg);
            return;
        }
        fn(d, arg);
    }
};

template <typename T1, typename T2>
Func1<T2> MkFunc1(void (*fn)(T1*, T2), T1* d) {
    auto res = Func1<T2>{};
    using fptr = void (*)(void*, T2);
    res.fn = (fptr)fn;
    res.SetData((void*)d, false);
    return res;
}

#if OS_WIN
struct Mutex {
    SRWLOCK lock = SRWLOCK_INIT;
    Mutex() = default;
    ~Mutex() = default;
    void Lock() { AcquireSRWLockExclusive(&lock); }
    void Unlock() { ReleaseSRWLockExclusive(&lock); }
};
#else
struct Mutex {
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    Mutex() = default;
    ~Mutex() = default;
    void Lock() { pthread_mutex_lock(&lock); }
    void Unlock() { pthread_mutex_unlock(&lock); }
};
#endif

struct ScopedMutex {
    Mutex* mutex;
    explicit ScopedMutex(Mutex* mutex) : mutex(mutex) { mutex->Lock(); }
    ~ScopedMutex() { mutex->Unlock(); }
};

static const u64 kArenaHeaderSize = 256;

typedef u64 ArenaFlags;
enum : ArenaFlags {
    ArenaFlagNoChain = 1ull << 0,
    ArenaFlagLargePages = 1ull << 1,
};

struct ArenaParams {
    ArenaFlags flags = 0;
    u64 reserveSize = 0;
    u64 commitSize = 0;
    void* optionalBackingBuffer = nullptr;
    const char* allocationSiteFile = nullptr;
    int allocationSiteLine = 0;
    const char* name = nullptr;
};

struct Arena {
    Arena* prev;
    Arena* current;
    ArenaFlags flags;
    u64 commitChunkSize;
    u64 reserveChunkSize;
    u64 basePos;
    u64 pos;
    u64 committed;
    u64 reserved;
    const char* allocationSiteFile;
    int allocationSiteLine;
    const char* name;
    bool usesExternalBuffer;
    Mutex lock;
    u64 nAllocsLifetime;
    u64 peakBytesLifetime;
    u64 nAllocsSinceReset;
    u64 peakBytesSinceReset;

    void* Alloc(int size);
    void Reset();
    void* Push(u64 size, u64 align = 8, bool zero = true);
    void PopTo(u64 pos);

    Arena() = delete;
    ~Arena() = delete;
};

static_assert(sizeof(Arena) <= kArenaHeaderSize, "Arena header must fit in reserved header bytes");

extern u64 gArenaDefaultReserveSize;
extern u64 gArenaDefaultCommitSize;
extern ArenaFlags gArenaDefaultFlags;

ArenaParams ArenaDefaultParams();
Arena* ArenaNew(const ArenaParams& params = ArenaDefaultParams());
void ArenaDelete(Arena* arena);

extern thread_local Arena* gTempArena;
Arena* GetTempArena();
void ResetTempArena();
void DestroyTempArena();

void* Alloc(struct Arena* arena, int size);
void Free(struct Arena* arena, void* mem);
void* Alloc(struct Arena* arena, size_t size);
void* AllocZero(struct Arena* arena, size_t size);
void* Realloc(struct Arena* arena, void* mem, size_t newSize, size_t copySize);
void* MemDup(struct Arena* arena, const void* mem, size_t size, size_t extraBytes = 0);

template <typename T>
inline T* AllocArray(struct Arena* arena, int n = 1) {
    return (T*)AllocZero(arena, (size_t)n * sizeof(T));
}

template <typename T, typename... Args>
T* New(Arena* arena, Args&&... args) {
    void* mem = Alloc(arena, sizeofi(T));
    return new (mem) T(std::forward<Args>(args)...);
}

bool VecRealloc(struct Arena* a, void** els, int len, int* cap, int newCap, int elSize);

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
            memcpy((void*)els, (const void*)other.els, sizeof(T) * (size_t)other.len);
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

    T& operator[](int idx) const {
        ReportIf(idx < 0);
        ReportIf(idx >= len);
        return els[idx];
    }

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

    using iterator = T*;
    using const_iterator = const T*;
    iterator begin() { return els; }
    const_iterator begin() const { return els; }
    iterator end() { return els ? els + len : nullptr; }
    const_iterator end() const { return els ? els + len : nullptr; }
};

template <typename T>
inline int len(const Vec<T>& v) {
    return v.len;
}

template <typename T>
bool VecReserve(Arena* arena, T& v, int wantedSize) {
    if (wantedSize <= v.cap) {
        return true;
    }
    int newCap = std::max(v.cap * 2, wantedSize);
    return VecRealloc(arena, (void**)&v.els, v.len, &v.cap, newCap, (int)sizeof(*v.els));
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
        memmove((void*)dst, (const void*)src, (size_t)(v.len - idx) * sizeof(T));
    }
    v.len = newLen;
    return res;
}

namespace str {

void Free(Str s);
void Free(const char*) = delete;

Str Dup(Arena*, Str str);
Str Dup(Str s);

bool Eq(Str s1, Str s2);
bool EqI(Str s1, Str s2);
bool EqNI(Str s1, Str s2, int n);
bool ContainsI(Str s, Str sub);
bool IsDigit(char c);

inline bool IsNull(const Str& s) {
    return !s.s;
}

struct Builder {
    Arena* a = nullptr;
    char* els = nullptr;
    int len = 0;
    int cap = 0;
    Str buf;
    int nReallocs = 0;

    explicit Builder(Str externalBuf = {});
    explicit Builder(int capHint);
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
    ~Builder();

    void Reset(Str s = {});
    char& operator[](int idx) const;
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
        WStr,
        RawStr,
        Any,
        None,
    };

    Kind t{Kind::None};
    union {
        Str str;
        WStr wstr;
        char c;
        i64 i;
        float f;
        double d;
        const void* ptr;
    };

    FmtArg() : i{0} {}
    explicit FmtArg(char c_) : t{Kind::Char}, c{c_} {}
    explicit FmtArg(int arg) : t{Kind::Int}, i{(i64)arg} {}
    explicit FmtArg(unsigned int arg) : t{Kind::Int}, i{(i64)arg} {}
    explicit FmtArg(long arg) : t{Kind::Int}, i{(i64)arg} {}
    explicit FmtArg(unsigned long arg) : t{Kind::Int}, i{(i64)arg} {}
    explicit FmtArg(long long arg) : t{Kind::Int}, i{(i64)arg} {}
    explicit FmtArg(unsigned long long arg) : t{Kind::Int}, i{(i64)arg} {}
    explicit FmtArg(float f_) : t{Kind::Float}, f{f_} {}
    explicit FmtArg(double d_) : t{Kind::Double}, d{d_} {}
    explicit FmtArg(Str arg) : t{Kind::Str}, str{arg} {}
    explicit FmtArg(WStr arg) : t{Kind::WStr}, wstr{arg} {}
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

int VsnprintfUtf8(Str buf, const char* fmt, va_list args);
} // namespace str

#define fmt(...) str::FormatTemp(__VA_ARGS__)
