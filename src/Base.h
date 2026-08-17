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

#if OS_WIN
using AtomicBool = volatile LONG;
using AtomicInt = volatile LONG;
using AtomicRefCount = volatile LONG;
using AtomicPtr = void* volatile;
#else
using AtomicBool = volatile int;
using AtomicInt = volatile int;
using AtomicRefCount = volatile int;
using AtomicPtr = void* volatile;
#endif

bool AtomicBoolGet(AtomicBool* p);
void AtomicBoolSet(AtomicBool* p, bool v);
int AtomicIntGet(AtomicInt* p);
void AtomicIntSet(AtomicInt* p, int v);
int AtomicIntAdd(AtomicInt* p, int v);
int AtomicIntInc(AtomicInt* p);
int AtomicIntDec(AtomicInt* p);
int AtomicRefCountAdd(AtomicRefCount* v);
int AtomicRefCountDec(AtomicRefCount* v);
void* AtomicPtrGet(AtomicPtr* p);
void AtomicPtrSet(AtomicPtr* p, void* v);
void* AtomicPtrExchange(AtomicPtr* p, void* v);

#if !OS_WIN
u64 GetTickCount64();
#endif

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

// exists just to mark the intent, needed by both Str.h and TempAllocator.h
using TempStr = Str;

// Create Str from string literal with compile-time length
#define StrL(lit) Str((char*)(lit), (int)(sizeof(lit) - 1))

// Compile-time length (as int) of a string literal (or char[]/WCHAR[] array);
// faster than len() which does a runtime strlen. Works for both narrow
// and wide literals since dimof counts elements. Not for decayed pointers.
#define LenL(lit) (dimofi(lit) - 1)

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
    explicit WStr(wchar_t* s_, int len_) : s(s_), len(len_) {}

    explicit operator bool() const { return len > 0 && s; }
};

// exists just to mark the intent, needed by both Str.h and TempAllocator.h
using TempWStr = WStr;

// Create WStr from wide string literal with compile-time length
#define WStrL(lit) WStr((wchar_t*)(lit), (int)((sizeof(lit) / sizeof(wchar_t)) - 1))

// length of a Str / WStr as int. Also accepts a C string (char* / wchar_t*) via
// Str/WStr's implicit ctor, like the former str::Leni / wstr::Leni it replaces.
inline int len(Str s) {
    return s.len;
}
inline int len(WStr s) {
    return s.len;
}

struct VecStr {
    int len;
    int cap;
    Str* els;
};

#if COMPILER_MSVC
#define NO_INLINE __declspec(noinline)
#define FORCEINLINE __forceinline
#else
// assuming gcc or similar
#define NO_INLINE __attribute__((noinline))
#define FORCEINLINE inline __attribute__((always_inline))
#endif

#define NoOp() ((void)0)

template <typename T, size_t N>
char (&DimofSizeHelper(T (&array)[N]))[N];
#define dimof(array) (sizeof(DimofSizeHelper(array)))
#define dimofi(array) (int)(sizeof(DimofSizeHelper(array)))
#define sizeofi(x) ((int)sizeof(x))

#if COMPILER_MSVC
// https://msdn.microsoft.com/en-us/library/4dt9kyhy.aspx
// enable msvc equivalent of -Wundef gcc option, warns when doing "#if FOO" and FOO is not defined
// can't be turned on globally because windows headers have those
#pragma warning(default : 4668)
#endif

// TODO: is there a better way?
#if COMPILER_MSVC
#define IS_UNUSED
#else
#define IS_UNUSED __attribute__((unused))
#endif

// __analysis_assume is defined by msvc for prefast analysis
#if !defined(__analysis_assume)
#define __analysis_assume(x)
#endif

#if COMPILER_MSVC
#pragma warning(push)
#pragma warning(disable : 6011) // silence /analyze: de-referencing a nullptr pointer
#endif
// Note: it's inlined to make it easier on crash reports analyzer (if wasn't inlined
// CrashMe() would show up as the cause of several different crash sites)
//
// Note: I tried doing this via RaiseException(0x40000015, EXCEPTION_NONCONTINUABLE, 0, 0);
// but it seemed to confuse callstack walking
inline void CrashMe() {
    char* p = nullptr;
    // cppcheck-suppress nullPointer
    *p = 0; // NOLINT
}
#if COMPILER_MSVC
#pragma warning(pop)
#endif

// ReportIf() is like assert() except it sends crash report in pre-release and debug
// builds.
// The idea is that assert() indicates "can't possibly happen" situation and if
// it does happen, we would like to fix the underlying cause.
// In practice in our testing we rarely get notified when an assert() is triggered
// and they are disabled in builds running on user's computers.
//
// ReportAlwaysIf() sends a report even in release builds. This is to catch the most
// thorny scenarios.
// Enabling it in pre-release builds but not in release builds is trade-off between
// shipping small executables (each ReportIf() adds few bytes of code) and having
// more testing on user's machines and not only in our personal testing.
// To crash uncoditionally use ReportIf(). It should only be used in
// rare cases where we really want to know a given condition happens. Before
// each release we should audit the uses of ReportAlwaysIf()

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
#define ReportIfFast(cond) ReportIfCond(cond, #cond, FILE_LINE, false, false)
#if defined(DEBUG)
#define ReportDebugIf(cond) ReportIfCond(cond, #cond, FILE_LINE, false, true)
#else
// In release the check is gone, but the condition must still be *read*, or a
// variable whose only consumer is a ReportDebugIf looks unused: the compiler
// warns and clang-analyzer-deadcode.DeadStores reports a dead store, tempting
// someone to "clean up" the variable and delete the debug assert with it.
// Passing it to an empty inline function is a real read that costs nothing --
// the call and the (side-effect-free) condition both optimize away.
inline void ReportDebugIfNoOp(bool) {}
#define ReportDebugIf(cond) ReportDebugIfNoOp(!!(cond))
#endif

// ugly hack: logf() is our logging macro so we must provide a way to call logf() from math library
inline float math_logf(float f) {
    return logf(f);
}

/* Logging macros are defined here but must be implemented by the app because different apps have different logging
 * needs. */
void log(Str s);
void loga(Str s); // log always

#define logf(...)                     \
    do {                              \
        Str s__ = ::fmt(__VA_ARGS__); \
        ::log(s__);                   \
    } while (0)
#define logfa(...)                    \
    do {                              \
        Str s__ = ::fmt(__VA_ARGS__); \
        ::loga(s__);                  \
    } while (0)

void* AllocZero(int count, int size);

template <typename T>
FORCEINLINE T* AllocArray(int n) {
    return (T*)AllocZero(n, sizeofi(T));
}

template <typename T>
FORCEINLINE T* AllocStruct() {
    return (T*)AllocZero(1, sizeofi(T));
}

template <typename T>
inline void ZeroStruct(T* s) {
    ZeroMemory((void*)s, sizeof(T));
}

template <typename T>
inline void ZeroArray(T& a) {
    size_t size = sizeof(a);
    ZeroMemory((void*)&a, size);
}

int limitValue(int val, int min, int max);
DWORD limitValue(DWORD val, DWORD min, DWORD max);
float limitValue(float val, float min, float max);

// return true if adding n to val overflows. Only valid for n > 0
template <typename T>
inline bool addOverflows(T val, T n) {
    if (n == 0 || val == 0) {
        return true;
    }
    ReportIf(n < 0);
    ReportIf(val < 0);
    T res = val + n;
    return val > res;
}

// return false if adding n to val overflows. Only valid for n > 0
template <typename T>
inline bool addSafe(T* valInOut, T n) {
    if (n == 0 || *valInOut == 0) {
        valInOut = 0;
        return true;
    }
    ReportIf(n < 0);
    ReportIf(*valInOut < 0);
    T res = *valInOut + n;
    if (res < *valInOut) {
        return false;
    }
    *valInOut = res;
    return true;
}

// return false if multiplying val by n overflows. Only valid for n > 0
template <typename T>
inline bool mulSafe(T* valInOut, T n) {
    if (n == 0 || *valInOut == 0) {
        *valInOut = 0;
        return true;
    }
    ReportIf(n < 0);
    ReportIf(*valInOut < 0);
    T res = *valInOut * n;
    if (res < *valInOut || res < n) {
        // multiplication overflowed
        return false;
    }
    *valInOut = res;
    return true;
}

void* memdup(const void* data, int n, int extraBytes = 0);
bool memeq(const void* s1, const void* s2, int n);

int RoundToPowerOf2(int size);
u32 MurmurHash2(const void* key, int n);
u32 MurmurHash2(Str s);
u32 MurmurHash2(WStr s);
u32 MurmurHashWStrI(WStr str);
u32 MurmurHashStrI(Str s);

int RoundUp(int n, int rounding);
void* RoundUp(void* d, int rounding);

template <typename T>
void ListDelete(T* root) {
    T* next;
    T* curr = root;
    while (curr) {
        next = curr->next;
        delete curr;
        curr = next;
    }
}

template <typename T>
void ListInsertFront(T** root, T* el) {
    el->next = *root;
    *root = el;
}

template <typename T>
void ListInsertEnd(T** root, T* el) {
    el->next = nullptr;
    if (!*root) {
        *root = el;
        return;
    }
    T** prevPtr = root;
    T** currPtr = root;
    T* curr;
    while (*currPtr) {
        prevPtr = currPtr;
        curr = *currPtr;
        currPtr = &(curr->next);
    }
    T* prev = *prevPtr;
    prev->next = el;
}

template <typename T>
void ListReverse(T** root) {
    T* newRoot = nullptr;
    T* next;
    T* el = *root;
    while (el) {
        next = el->next;
        el->next = newRoot;
        newRoot = el;
        el = next;
    }
    *root = newRoot;
}

template <typename T>
bool ListRemove(T** root, T* el) {
    T** currPtr = root;
    T* curr;
    for (;;) {
        curr = *currPtr;
        if (!curr) {
            return false;
        }
        if (curr == el) {
            break;
        }
        currPtr = &(curr->next);
    }
    *currPtr = el->next;
    return true;
}

template <typename T>
int ListLen(T* root) {
    int n = 0;
    T* curr = root;
    while (curr) {
        n++;
        curr = curr->next;
    }
    return n;
}

/*
Poor-man's manual dynamic typing.
Identity of an object is an address of a unique, global string.
String is good for debugging

For classes / structs that we want to query for type at runtime, we add:

// in foo.h
struct Foo {
    Kind kind;
};

or:

struct Foo : KindBase {
};

extern Kind kindFoo;

// in foo.cpp
Kind kindFoo = "foo";
*/

using Kind = const char*;

struct KindBase {
    Kind kind = nullptr;

    Kind GetKind() const { return kind; }
};

inline bool isOfKindHelper(Kind k1, Kind k2) {
    return k1 == k2;
}

#define IsOfKind(o, wantedKind) ((o) && isOfKindHelper((o)->kind, (wantedKind)))

extern Kind kindNone; // unknown kind

// from https://pastebin.com/3YvWQa5c
// In my testing, in debug build defer { } creates somewhat bloated code
// but in release it seems to be optimized to optimally small code
#define CONCAT_INTERNAL(x, y) x##y
#define CONCAT(x, y) CONCAT_INTERNAL(x, y)

template <typename T>
struct ExitScope {
    T lambda;
    ExitScope(T lambda) : lambda(lambda) { // NOLINT
    }
    ~ExitScope() { lambda(); }
    ExitScope(const ExitScope&);

  private:
    ExitScope& operator=(const ExitScope&);
};

class ExitScopeHelp {
  public:
    template <typename T>
    ExitScope<T> operator+(T t) {
        return t;
    }
};

using func0Ptr = void (*)(void*);
using funcVoidPtr = void (*)();

// Func1 keeps a flag in userData's lowest bit, so every value stored there
// has to be even - including this sentinel, which is why it is ~1 and not -1
#define kFuncNoArg ((void*)~(uintptr_t)1)

// the simplest possible function that ties a function and a single argument to it
// we get type safety and convenience with mkFunc()
struct Func0 {
    void* fn = nullptr;
    void* userData = nullptr;

    Func0() = default;
    // copy constructor
    Func0(const Func0& that) {
        this->fn = that.fn;
        this->userData = that.userData;
    }
    // copy assignment operator
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
Func0 MkFunc0Void(funcVoidPtr fn);

template <typename T>
Func0 MkFunc0(void (*fn)(T*), T* d) {
    auto res = Func0{};
    res.fn = (void*)fn;
    res.userData = (void*)d;
    return res;
}

template <typename T, void (T::*Method)()>
static void MethodTrampoline(void* obj) {
    (static_cast<T*>(obj)->*Method)();
}

template <typename T, void (T::*Method)()>
Func0 MkMethod0(T* obj) {
    auto res = Func0{};
    res.fn = (void*)&MethodTrampoline<T, Method>;
    res.userData = (void*)obj;
    return res;
}

template <typename T>
struct Func1 {
    // bit 0 of userData says fn takes no T, so Call() drops the argument -
    // that's how a Func0 can stand in for a Func1. Everything we store is at
    // least 2-byte aligned (and kFuncNoArg is even), so the bit is free and the
    // struct stays two words
    static constexpr uintptr_t kDropsArgBit = 1;

    void (*fn)(void*, T) = nullptr;
    uintptr_t userData = 0;

    Func1() = default;
    // a Func0 is a Func1 that doesn't look at its argument
    Func1(const Func0& that) {
        this->fn = (void (*)(void*, T))that.fn;
        this->SetData(that.userData, true);
    }
    // copy constructor
    Func1(const Func1& that) {
        this->fn = that.fn;
        this->userData = that.userData;
    }
    // copy assignment operator
    Func1& operator=(const Func1& that) {
        if (this != &that) {
            this->fn = that.fn;
            this->userData = that.userData;
        }
        return *this;
    }
    ~Func1() = default;

    void SetData(void* d, bool dropsArg) {
        // an odd pointer would collide with the flag. Nothing we take the
        // address of is 1-byte aligned, so this means the caller handed us
        // something that isn't a real pointer
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

template <typename T, typename TArg, void (T::*Method)(TArg)>
static void MethodTrampoline1(void* obj, TArg arg) {
    (static_cast<T*>(obj)->*Method)(arg);
}

template <typename T, typename TArg, void (T::*Method)(TArg)>
Func1<TArg> MkMethod1(T* obj) {
    auto res = Func1<TArg>{};
    using fptr = void (*)(void*, TArg);
    res.fn = (fptr)&MethodTrampoline1<T, TArg, Method>;
    res.SetData((void*)obj, false);
    return res;
}

template <typename T1, typename T2>
Func1<T2> MkFunc1(void (*fn)(T1*, T2), T1* d) {
    auto res = Func1<T2>{};
    using fptr = void (*)(void*, T2);
    res.fn = (fptr)fn;
    res.SetData((void*)d, false);
    return res;
}

template <typename T2>
Func1<T2> MkFunc1Void(void (*fn)(T2)) {
    auto res = Func1<T2>{};
    using fptr = void (*)(void*, T2);
    res.fn = (fptr)fn;
    res.SetData(kFuncNoArg, false);
    return res;
}

template <typename T1, typename T2>
Func1<T2>* NewFunc1(void (*fn)(T1*, T2), T1* d) {
    auto res = new Func1<T2>{};
    using fptr = void (*)(void*, T2);
    res->fn = (fptr)fn;
    res->SetData((void*)d, false);
    return res;
}

int setMinMax(int& v, int minVal, int maxVal);

/* Usage: defer { instance->Release(); }; */
#define defer const auto& CONCAT(defer__, __LINE__) = ExitScopeHelp() + [&]()

extern AtomicInt gAllowAllocFailure;

// ─── Thread.h ───────────────────────────────────────────────────────────────

#if OS_WIN
using ThreadId = DWORD;
using ThreadHandle = HANDLE;

struct Mutex {
    SRWLOCK lock = SRWLOCK_INIT;

    Mutex() = default;
    ~Mutex() = default;

    void Lock() { AcquireSRWLockExclusive(&lock); }
    void Unlock() { ReleaseSRWLockExclusive(&lock); }
    bool TryLock() { return TryAcquireSRWLockExclusive(&lock); }
};

struct ConditionVariable {
    CONDITION_VARIABLE cond = CONDITION_VARIABLE_INIT;

    ConditionVariable() = default;
    ~ConditionVariable() = default;

    void Wait(Mutex* mutex) { SleepConditionVariableSRW(&cond, &mutex->lock, INFINITE, 0); }
    void Wake() { WakeConditionVariable(&cond); }
    void WakeAll() { WakeAllConditionVariable(&cond); }
};

struct RecursiveMutex {
    CRITICAL_SECTION lock;

    RecursiveMutex() { InitializeCriticalSection(&lock); }
    ~RecursiveMutex() { DeleteCriticalSection(&lock); }

    void Lock() { EnterCriticalSection(&lock); }
    void Unlock() { LeaveCriticalSection(&lock); }
    bool TryLock() { return TryEnterCriticalSection(&lock); }
};
#else
using ThreadId = u64;

struct ThreadHandlePosix;
using ThreadHandle = ThreadHandlePosix*;

struct Mutex {
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

    Mutex() = default;
    ~Mutex() = default;

    void Lock() { pthread_mutex_lock(&lock); }
    void Unlock() { pthread_mutex_unlock(&lock); }
    bool TryLock() { return pthread_mutex_trylock(&lock) == 0; }
};

struct ConditionVariable {
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

    ConditionVariable() = default;
    ~ConditionVariable() { pthread_cond_destroy(&cond); }

    void Wait(Mutex* mutex) { pthread_cond_wait(&cond, &mutex->lock); }
    void Wake() { pthread_cond_signal(&cond); }
    void WakeAll() { pthread_cond_broadcast(&cond); }
};

struct RecursiveMutex {
    pthread_mutex_t lock;

    RecursiveMutex() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&lock, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    ~RecursiveMutex() { pthread_mutex_destroy(&lock); }

    void Lock() { pthread_mutex_lock(&lock); }
    void Unlock() { pthread_mutex_unlock(&lock); }
    bool TryLock() { return pthread_mutex_trylock(&lock) == 0; }
};

ThreadId GetCurrentThreadId();
#endif

struct ScopedMutex {
    Mutex* mutex;

    explicit ScopedMutex(Mutex* mutex) : mutex(mutex) { mutex->Lock(); }
    ~ScopedMutex() { mutex->Unlock(); }
};

struct ScopedRecursiveMutex {
    RecursiveMutex* mutex;

    explicit ScopedRecursiveMutex(RecursiveMutex* mutex) : mutex(mutex) { mutex->Lock(); }
    ~ScopedRecursiveMutex() { mutex->Unlock(); }
};

void SetThreadName(Str threadName, ThreadId threadId = 0);
void SleepInMs(int ms);

void RunAsync(const Func0&, Str threadName = {});
ThreadHandle StartThread(const Func0&, Str threadName = {});
#if OS_WIN
inline bool SafeCloseThreadHandle(ThreadHandle* hPtr) {
    ThreadHandle h = *hPtr;
    if (!h || h == INVALID_HANDLE_VALUE) {
        *hPtr = nullptr;
        return false;
    }
    BOOL ok = CloseHandle(h);
    *hPtr = nullptr;
    return !!ok;
}
#else
bool SafeCloseThreadHandle(ThreadHandle*);
#endif

extern AtomicInt gDangerousThreadCount;
bool AreDangerousThreadsPending();

// ─── Arena.h ───────────────────────────────────────────────────────────────

// Reserve/commit arena allocator (implemented in Arena.cpp).
// Not self-sufficient: include after the part of utils/Base.h that defines
// u64 and pulls in <windows.h> / <utility>. Base.h includes this header.

// Standalone reserve/commit arena
// 256 (not 128) to leave room in the header for the allocation stats below
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

struct Arena;

struct ArenaSavepoint {
    Arena* arena;
    u64 pos;
};

struct Arena {
    Arena* prev;    // Previous arena in chain
    Arena* current; // Current arena in chain
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

    // allocation statistics, updated after every successful allocation
    // (see ArenaPushLocked). "peak bytes" is the high-water mark of total
    // bytes used by the arena (its position, including the header).
    u64 nAllocsLifetime;     // total allocations over the arena's whole life
    u64 peakBytesLifetime;   // largest total size the arena ever reached
    u64 nAllocsSinceReset;   // allocations since the last Reset()
    u64 peakBytesSinceReset; // largest total size reached since the last Reset()

    void* Alloc(int size);
    void Free(void* ptr);
    void Reset();
    void* Push(u64 size, u64 align = 8, bool zero = true);
    u64 Pos();
    void PopTo(u64 pos);
    void Pop(u64 amt);
    void* GetAvailableSpace(int* bufSizeOut);
    void* CommitReserved(void* mem, int size);

    Arena() = delete;  // use ArenaNew()
    ~Arena() = delete; // use ArenaDelete()
};

static_assert(sizeof(Arena) <= kArenaHeaderSize, "Arena header must fit in reserved header bytes");

extern u64 gArenaDefaultReserveSize;
extern u64 gArenaDefaultCommitSize;
extern ArenaFlags gArenaDefaultFlags;

ArenaParams ArenaDefaultParams();
Arena* ArenaNew(const ArenaParams& params = ArenaDefaultParams());
void ArenaDelete(Arena* arena);

ArenaSavepoint GetArenaSavepoint(Arena* arena);
void RestoreArenaSavepoint(ArenaSavepoint temp);

u32 ArenaPtrCompress(Arena* arena, void* ptr);
void* ArenaPtrUncompress(Arena* arena, u32 compressed);

template <typename T>
inline T* ArenaPtrUncompress(Arena* arena, u32 compressed) {
    return (T*)ArenaPtrUncompress(arena, compressed);
}

// Thread-local temporary arena, reset after each message loop iteration
extern thread_local Arena* gTempArena;
Arena* GetTempArena();
void ResetTempArena();
void DestroyTempArena();

// RAII scratch scope for an arena (the temp arena unless told otherwise):
// rewinds it to the entry position on scope exit, so code that allocates
// scratch in a loop or on a hot path doesn't grow the arena unbounded.
struct AutoArenaSavepoint {
    ArenaSavepoint sp;
    AutoArenaSavepoint(Arena* a = GetTempArena()) { // NOLINT
        sp = GetArenaSavepoint(a);
    }
    AutoArenaSavepoint(AutoArenaSavepoint& other) = delete;
    AutoArenaSavepoint(AutoArenaSavepoint&& other) = delete;
    AutoArenaSavepoint(const AutoArenaSavepoint& other) = delete;
    AutoArenaSavepoint(const AutoArenaSavepoint&& other) = delete;
    ~AutoArenaSavepoint() { RestoreArenaSavepoint(sp); }
};

// Arena for allocations that live for the whole lifetime of the program (i.e.
// never freed until exit). Allocating them here avoids per-allocation frees and
// lets us track how much such memory we use (logged on exit). Never Reset().
extern Arena* gPermArena;
Arena* GetPermArena();
void DestroyPermArena();

template <typename T>
inline T* PushArrayNoZeroAligned(Arena* arena, u64 count, u64 align) {
    return (T*)arena->Push(sizeof(T) * count, align, false);
}

template <typename T>
inline T* PushArrayAligned(Arena* arena, u64 count, u64 align) {
    return (T*)arena->Push(sizeof(T) * count, align, true);
}

template <typename T>
inline T* PushArrayNoZero(Arena* arena, u64 count) {
    return PushArrayNoZeroAligned<T>(arena, count, (alignof(T) > 8) ? alignof(T) : 8);
}

template <typename T>
inline T* PushArray(Arena* arena, u64 count) {
    return PushArrayAligned<T>(arena, count, (alignof(T) > 8) ? alignof(T) : 8);
}

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

// like AllocArray but in the thread-local temp arena (reset each message loop)
template <typename T>
inline T* AllocArrayTemp(int n = 1) {
    return AllocArray<T>(GetTempArena(), n);
}

void* AllocTemp(int size, u64 align = 8);

bool VecRealloc(struct Arena* a, void** els, int len, int* cap, int newCap, int elSize);

// Allocate and construct object using placement new (supports constructor args)
template <typename T, typename... Args>
T* New(Arena* arena, Args&&... args) {
    void* mem = Alloc(arena, sizeofi(T));
    return new (mem) T(std::forward<Args>(args)...);
}
void LogArenaStats(Str what, Arena* a);

// ─── Vec.h ───────────────────────────────────────────────────────────────

/* Simple vector/array class that can store pointer types or POD types
(http://stackoverflow.com/questions/146452/what-are-pod-types-in-c).

Storage is heap (or arena) only; starts empty with no allocation.
*/
template <typename T>
struct Vec;

// Ensure capacity is at least wantedSize for a vec-like {els,len,cap}.
// Growth: max(cap*2, wantedSize). arena may be null (heap).
template <typename T>
bool VecReserve(Arena* arena, T& v, int wantedSize);

// Heap Vec: same growth; returns v.els (nullptr on failure).
template <typename T>
inline T* VecReserve(Vec<T>& v, int capNeeded);

// Open a hole of `count` elements at `idx`; updates len. Returns &v.els[idx].
template <typename T>
T* VecInsertSpace(Vec<T>& v, int idx, int count);

template <typename T>
struct Vec {
    int len = 0;
    int cap = 0;
    T* els = nullptr;

    // We always pad the elements with a single 0 value. This makes
    // Vec<char> and Vec<WCHAR> a C-compatible string. Although it's
    // not useful for other types, the code is simpler if we always do it
    // (rather than have it an optional behavior).

    void FreeEls() {
        if (els) {
            Free(nullptr, (void*)els);
            els = nullptr;
        }
    }

    // resets to initial state, freeing memory
    void Reset() {
        FreeEls();
        len = 0;
        cap = 0;
    }

    // use to empty but don't free els
    // for efficient reuse
    void Clear() {
        len = 0;
        if (els && cap > 0) {
            memset((void*)els, 0, (size_t)cap * sizeof(T));
        }
    }

    explicit Vec() = default;

    // ensure that a Vec never shares its els buffer with another after a clone/copy
    // note: we don't inherit allocator as it's not needed for our use cases
    Vec(const Vec& other) {
        VecReserve(*this, other.len);
        len = other.len;
        // using memcpy, as Vec only supports POD types
        if (other.len > 0 && other.els && els) {
            memcpy((void*)els, (const void*)other.els, sizeof(T) * (size_t)other.len);
        }
    }

    // TODO: write Vec(const Vec&& other)

    Vec& operator=(const Vec& other) {
        if (this == &other) {
            return *this;
        }

        Reset();
        VecReserve(*this, other.len);
        // using memcpy, as Vec only supports POD types
        len = other.len;
        if (other.len > 0) {
            memcpy((void*)els, (const void*)other.els, sizeof(T) * (size_t)len);
            memset((void*)(els + len), 0, sizeof(T) * (size_t)(cap - len));
        }
        return *this;
    }

    ~Vec() { FreeEls(); }

    // this frees all elements and clears the array.
    // only applicable where T is a pointer. Otherwise will fail to compile
    void FreeMembers() {
        for (int i = 0; i < len; i++) {
            auto s = els[i];
            free(s);
        }
        Reset();
    }

    T& operator[](int idx) const {
        ReportIf(idx < 0);
        ReportIf(idx >= len);
        return els[idx];
    }

    bool isValidIndex(int idx) const { return (idx >= 0) && (idx < len); }

    bool InsertAt(int idx, const T& el) {
        T* p = VecInsertSpace(*this, idx, 1);
        if (!p) {
            return false;
        }
        p[0] = el;
        return true;
    }

    bool Append(const T& el) { return InsertAt(len, el); }

    bool Append(const T* src, int count) {
        if (0 == count) {
            return true;
        }
        T* dst = VecInsertSpace(*this, len, count);
        if (!dst) {
            return false;
        }
        memcpy((void*)dst, (const void*)src, (size_t)count * sizeof(T));
        return true;
    }

    bool Append(const Vec& other) {
        int n = other.len;
        const T* data = other.LendData();
        return this->Append(data, n);
    }

    // appends count blank (i.e. zeroed-out) elements at the end
    T* AppendBlanks(int count) { return VecInsertSpace(*this, len, count); }

    void RemoveAt(int idx, int count = 1) {
        if (len > idx + count) {
            T* dst = els + idx;
            T* src = els + idx + count;
            memmove((void*)dst, (const void*)src, (size_t)(len - idx - count) * sizeof(T));
        }
        len -= count;
        memset((void*)(els + len), 0, (size_t)count * sizeof(T));
    }

    void RemoveLast() {
        if (len == 0) {
            return;
        }
        RemoveAt(len - 1);
    }

    // This is a fast version of RemoveAt() which replaces the element we're
    // removing with the last element, copying less memory.
    // It can only be used if order of elements doesn't matter and elements
    // can be copied via memcpy()
    // TODO: could be extend to take number of elements to remove
    void RemoveAtFast(int idx) {
        ReportIf(idx >= len);
        if (idx >= len) {
            return;
        }
        T* toRemove = els + idx;
        T* last = els + len - 1;
        if (toRemove != last) {
            memcpy((void*)toRemove, (const void*)last, sizeof(T));
        }
        memset((void*)last, 0, sizeof(T));
        --len;
    }

    T Pop() {
        ReportIf(0 == len);
        T el = els[len - 1];
        RemoveAtFast(len - 1);
        return el;
    }

    T PopAt(int idx) {
        ReportIf(idx >= len);
        T el = els[idx];
        RemoveAt(idx);
        return el;
    }

    T& Last() const {
        ReportIf(0 == len);
        return els[len - 1];
    }

    // perf hack for using as a buffer: client can get accumulated data
    // without duplicate allocation. Note: since Vec over-allocates, this
    // is likely to use more memory than strictly necessary, but in most cases
    // it doesn't matter
    T* Take() {
        T* res = els;
        els = nullptr;
        len = 0;
        cap = 0;
        return res;
    }

    T* LendData() const { return els; }

    int Find(const T& el, int startAt = 0) const {
        for (int i = startAt; i < len; i++) {
            if (els[i] == el) {
                return i;
            }
        }
        return -1;
    }

    bool Contains(const T& el) const { return -1 != Find(el); }

    // returns position of removed element or -1 if not removed
    int Remove(const T& el) {
        int i = Find(el);
        if (i >= 0) {
            RemoveAt(i);
        }
        return i;
    }

    // returns position of removed element or -1 if not removed
    int RemoveFast(const T& el) {
        int i = Find(el);
        if (i >= 0) {
            RemoveAtFast(i);
        }
        return i;
    }

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() { return els; }
    const_iterator begin() const { return els; }
    iterator end() { return els ? els + len : nullptr; }
    const_iterator end() const { return els ? els + len : nullptr; }
};

// number of elements, as int (matches len() for Str / WStr)
template <typename T>
inline int len(const Vec<T>& v) {
    return v.len;
}

// cmp is non-deduced (via nested type) so lambdas convert after T is known from v.
template <typename T>
struct VecSortCmp {
    using Fn = int (*)(const T* a, const T* b);
};

template <typename T>
void VecSort(Vec<T>& v, typename VecSortCmp<T>::Fn cmpFunc) {
    if (v.len > 0) {
        auto cmp = (int (*)(const void* a, const void* b))cmpFunc;
        qsort((void*)v.els, v.len, sizeof(T), cmp);
    }
}

template <typename T>
void VecReverse(Vec<T>& v) {
    for (int i = 0; i < v.len / 2; i++) {
        std::swap(v.els[i], v.els[v.len - i - 1]);
    }
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

// Set logical length to newSize (std::vector::resize). Grows capacity if needed;
// zeros unused capacity beyond the new length.
template <typename T>
bool VecResize(Vec<T>& v, int newSize) {
    if (newSize < 0) {
        return false;
    }
    if (newSize > v.cap) {
        if (!VecReserve(v, newSize)) {
            return false;
        }
    }
    v.len = newSize;
    if (v.els && v.cap > v.len) {
        memset((void*)(v.els + v.len), 0, (size_t)(v.cap - v.len) * sizeof(T));
    }
    return true;
}

// only suitable for T that are pointers to C++ objects
template <typename T>
inline void DeleteVecMembers(Vec<T>& v) {
    for (T& el : v) {
        delete el;
    }
    v.Clear();
}

template <typename T, typename E>
bool VecPush(Arena* arena, T& v, E el) {
    bool ok = VecReserve(arena, v, v.len + 1);
    if (!ok) {
        return false;
    }
    v.els[v.len] = el;
    v.len++;
    return true;
}

// Iterator wrapper for range-based for loops over Vec types (structs with len/els)
template <typename Vec>
class VecIterator {
    Vec* vec;

  public:
    VecIterator(Vec* v) : vec(v) {}
    auto begin() { return vec ? vec->els : nullptr; }
    auto end() { return vec && vec->els ? vec->els + vec->len : nullptr; }
};

// Helper functions for type deduction (works with both Vec& and Vec*)
template <typename Vec>
VecIterator<Vec> VecIter(Vec& v) {
    return VecIterator<Vec>(&v);
}
template <typename Vec>
VecIterator<Vec> VecIter(Vec* v) {
    return VecIterator<Vec>(v);
}

// ─── Str.h ───────────────────────────────────────────────────────────────

#define UTF8_BOM "\xEF\xBB\xBF"
#define UTF16_BOM "\xFF\xFE"
#define UTF16BE_BOM "\xFE\xFF"

using StrArena = u32;
StrArena StrArenaAlloc(Arena* a, int size);
StrArena StrArenaDupStr(Arena* a, Str s);
Str StrArenaToStr(Arena* a, StrArena sa);

// Singly-linked string node; AllocStrNode places the string bytes immediately
// after the node in one allocation (s.s points into that block).
struct StrNode {
    StrNode* next = nullptr;
    Str s;
};
StrNode* AllocStrNode(Arena* a, Str s);
StrNode* FindStrNode(StrNode* root, Str s);
void FreeStrNode(Arena* a, StrNode* head);

// Head/tail list of StrNodes (first→…→last via next). Does not own/free nodes.
struct StrNodeList {
    StrNode* head = nullptr;
    StrNode* tail = nullptr;
};
void StrNodeListPush(StrNodeList* list, StrNode* n);
void StrNodeListPop(StrNodeList* list);

namespace str {

enum class TrimOpt {
    Left,
    Right,
    Both
};

void Free(Str s);
// catch passing a raw char* (e.g. the .s member): pass the Str directly instead
// -- going through the pointer builds an unnecessary temp Str (with a strlen).
// To free a raw owned pointer use ::free().
void Free(const char*) = delete;
void FreePtr(Str* s);

Str Dup(Arena*, Str str);
Str Dup(Str s);
TempStr DupTemp(Str s);
TempWStr DupTemp(WStr s);

void ReplacePtr(Str* s, Str snew);

void ReplaceWithCopy(Str* s, Str snew);

Str Join(Arena*, Str, Str, Str);
Str Join(Arena*, Str, Str, Str, Str, Str);
Str Join(Str s1, Str s2, Str s3 = {});
FORCEINLINE Str Join(Arena* a, Str s1, Str s2, Str s3, Str s4) {
    return Join(a, s1, s2, s3, s4, Str{});
}
FORCEINLINE Str Join(Str s1, Str s2, Str s3, Str s4) {
    return Join(nullptr, s1, s2, s3, s4);
}
TempStr JoinTemp(Str s1, Str s2, Str s3 = {});
TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4);
TempStr JoinTemp(Str s1, Str s2, Str s3, Str s4, Str s5);
TempWStr JoinTemp(WStr s1, WStr s2, WStr s3 = {});

bool Eq(Str s1, Str s2);
bool EqI(Str s1, Str s2);
bool EqIS(Str s1, Str s2);
bool EqN(Str s1, Str s2, int n);
bool EqNI(Str s1, Str s2, int n);
// inline so callers (and the static analyzer) can see the guard
inline bool IsNull(const Str& s) {
    return !s.s;
}
bool StartsWith(Str str, Str prefix);
bool TrimPrefix(Str& s, Str prefix);

bool StartsWithI(Str str, Str prefix);
bool EndsWith(Str txt, Str end);
bool EndsWithI(Str txt, Str end);
bool EqNIx(Str s, int n, Str s2);

Str ToLowerInPlace(Str s);

Str ToLower(Str s);

Str ToUpperInPlace(Str s);

bool IsDigit(char c);
bool IsWs(char c);
bool IsAlNum(char c);

Str SliceFromChar(Str str, char c);
Str SliceFromCharLast(Str str, char c);
int IndexOfChar(Str s, char c);
int IndexOf(Str buf, Str toFind);
int IndexOfI(Str s, Str toFind);
int IndexOfAfter(Str s, Str needle);
bool Cut(Str s, Str sep, Str* before, Str* after);
bool CutChar(Str s, char c, Str* before, Str* after);
bool CutCharLast(Str s, char c, Str* before, Str* after);
bool NextLine(Str s, Str& line, Str& rest);

bool Contains(Str s, Str sub);
bool ContainsI(Str s, Str sub);
bool ContainsChar(Str s, char c);
bool ContainsCharAny(Str s, Str chars);

Str TrimSuffix(Str s, Str suffix);
int LastIndexOfChar(Str s, char c);
Str TrimSuffixWhitespace(Str s); // trims trailing whitespace in place

TempStr ReplaceTemp(Str s, Str toReplace, Str replaceWith);
TempStr ReplaceNoCaseTemp(Str s, Str toReplace, Str replaceWith);

int TrimWSInPlace(Str& s, TrimOpt opt);

void TransCharsInPlace(Str& str, Str oldChars, Str newChars);

int NormalizeWSInPlace(Str str);
TempStr NormalizeWSTemp(Str s);
int NormalizeNewlinesInPlace(Str s, Str endExclusive);
int NormalizeNewlinesInPlace(Str s);
int RemoveCharsInPlace(Str str, Str toRemove);

int BufSet(Str dst, Str src);
int BufAppend(Str dst, Str s);

TempStr MemToHexTemp(Str buf);
bool HexToMem(Str s, Str buf);

int CmpNatural(Str a, Str b);
int Cmp(Str a, Str b);
int CmpI(Str a, Str b);

bool IsEmptyOrWhiteSpace(Str s);
bool SkipChar(Str& s, char toSkip);

int BufSet(WCHAR* dst, int dstCchSize, Str src);

WStr CastStrToWStr(Str s);
} // namespace str

void SplitStrByWhitespace(Arena* arena, const Str& s, VecStr& vecOut);

namespace wstr {

void Free(WStr s);
// catch passing a raw wchar_t* (e.g. the .s member): pass the WStr directly
// instead. To free a raw owned pointer use ::free().
void Free(const wchar_t*) = delete;
void FreePtr(WStr* s);

WStr Dup(Arena*, WStr str);
WStr Dup(WStr s);
WStr Join(WStr, WStr, WStr s3 = {});
WStr Join(Arena*, WStr, WStr, WStr s3);
bool Eq(WStr s1, WStr s2);
bool EqI(WStr s1, WStr s2);
bool EqN(WStr s1, WStr s2, int n);
bool EqNI(WStr s1, WStr s2, int n);
int Cmp(WStr a, WStr b);
int CmpI(WStr a, WStr b);
// inline so callers (and the static analyzer) can see the guard
inline bool IsNull(const WStr& s) {
    return !s.s;
}
bool StartsWith(WStr str, WStr prefix);
bool StartsWithI(WStr str, WStr prefix);
bool EndsWith(WStr txt, WStr end);
bool EndsWithI(WStr txt, WStr end);
WStr ToLower(WStr s);
WStr ToLowerInPlace(WStr s);
int BufSet(WStr dst, WStr src);
int NormalizeWSInPlace(WStr str);
int RemoveCharsInPlace(WStr str, WStr toRemove);
int IndexOfChar(WStr s, WCHAR c);
bool ContainsChar(WStr s, WCHAR c);
WStr SliceFromChar(WStr str, WCHAR c);
WStr FindFrom(WStr str, WStr find);
bool IsWs(WCHAR c);
bool IsDigit(WCHAR c);
bool IsNonCharacter(WCHAR c);
void TransCharsInPlace(WStr& str, WStr oldChars, WStr newChars);
WStr Replace(WStr s, WStr toReplace, WStr replaceWith);

} // namespace wstr

namespace url {

TempStr DecodeTemp(Str url);
bool IsAbsolute(Str url);
TempStr GetFullPathTemp(Str url);
TempStr GetFileNameTemp(Str url);

} // namespace url

using SeqStrings = const char*;

TempStr SeqStrAt(SeqStrings strs, int off);
bool SeqStrAdvance(SeqStrings strs, int& off, int* idxInOut = nullptr);
int SeqStrIndex(SeqStrings strs, Str toFind);
int SeqStrIndexIS(SeqStrings strs, Str toFind);
TempStr SeqStrByIndex(SeqStrings strs, int idx);

// look up the mime type for a file extension (e.g. ".png" -> "image/png");
// returns {} for unknown extensions. If the matched type is an image and
// imgExt (the extension detected from the file's data) is given, it wins.
TempStr MimeTypeFromExtTemp(Str ext, Str imgExt = {});

// SeqStrNum: like SeqStrings but each entry is <string>\0<varint i64>, sequence ends with \0.
// Varint is unsigned LEB128 of zigzag-encoded i64 (small for non-negative values).
// Use when mapping strings to arbitrary numbers (not just sequential indices).
// In use: ShortcutParse.cpp: gVirtKeysNum (generated by cmd/gen-code.ts).
// Index-is-the-number (SeqStrings suffices today): displayModeNames, gArgNames, gToolNames,
//   permNames, gScrollbarModeNames, gFileActionNames, gAnnotationTextIcons, kPdfFilterStateStrs,
//   gLangCodes.
using SeqStrNum = const char*;

TempStr SeqStrNumAt(SeqStrNum strs, int off);
bool SeqStrNumAdvance(SeqStrNum strs, int& off, int* idxInOut = nullptr);
int SeqStrNumIndex(SeqStrNum strs, Str toFind, i64* numOut);
int SeqStrNumIndexIS(SeqStrNum strs, Str toFind, i64* numOut);
TempStr SeqStrNumByIndex(SeqStrNum strs, int idx, i64* numOut);
TempStr SeqStrNumStrByNumber(SeqStrNum strs, i64 num);

namespace str {
struct Builder {
    // arena is not owned by Builder and must outlive it
    Arena* a = nullptr;
    char* els = nullptr;
    int len = 0;
    int cap = 0;
    // Optional external scratch (not owned; default empty = allocate on first use).
    // When set, used while needed+NUL fits in buf.len; growth copies to heap/arena.
    Str buf;

    int nReallocs = 0;

    // externalBuf: optional scratch (not owned), e.g. stack or temp-arena memory.
    // Set .a after construction if heap allocs should use an arena.
    explicit Builder(Str externalBuf = {});
    explicit Builder(int capHint);
    // the implicit memberwise copy would alias els and double-free it
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    ~Builder();

    // true while storage is the external buf (or no storage yet)
    bool UsesExternalBuf() const { return !els || (buf.s && els == buf.s); }

    void Reset(Str s = {});
    char& operator[](int idx) const;
    bool InsertAt(int idx, char el);
    bool AppendChar(char c);
    bool Append(Str src);
    char RemoveAt(int idx, int count = 1);
    char RemoveLast();
    char& Last() const;
    Str TakeStr();
    bool IsEmpty() const;
    char LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = char*;

    iterator begin() const { return els ? &(els[0]) : nullptr; }
    iterator end() const { return els ? &(els[len]) : nullptr; }
};

bool Contains(const Builder& b, Str sub);
} // namespace str

void SeqStrNumAppend(str::Builder* b, Str s, i64 num);
void SeqStrNumFinish(str::Builder* b);

namespace wstr {
struct Builder {
    // arena is not owned by Builder and must outlive it
    Arena* a = nullptr;
    WCHAR* els = nullptr;
    u32 len = 0;
    u32 cap = 0;
    // Optional external scratch (not owned; default empty = allocate on first use).
    // When set, used while needed+NUL fits in buf.len; growth copies to heap/arena.
    WStr buf;

    int nReallocs = 0;

    static constexpr int kElSize = sizeofi(WCHAR);

    // externalBuf: optional scratch (not owned), e.g. stack or temp-arena memory.
    // Set .a after construction if heap allocs should use an arena.
    explicit Builder(WStr externalBuf = {});
    explicit Builder(int capHint);
    // the implicit memberwise copy would alias els and double-free it
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;

    ~Builder();

    // true while storage is the external buf (or no storage yet)
    bool UsesExternalBuf() const { return !els || (buf.s && els == buf.s); }

    void Reset(WStr s = {});
    WCHAR& operator[](int idx) const;
    bool InsertAt(int idx, const WCHAR& el);
    bool AppendChar(WCHAR);
    bool Append(WStr src);
    WCHAR RemoveAt(int idx, int count = 1);
    WCHAR RemoveLast();
    WStr TakeWStr();
    bool IsEmpty() const;
    WCHAR LastChar() const;

    // http://www.cprogramming.com/c++11/c++11-ranged-for-loop.html
    // https://stackoverflow.com/questions/16504062/how-to-make-the-for-each-loop-function-in-c-work-with-a-custom-class
    using iterator = WCHAR*;

    iterator begin() const { return els ? &(els[0]) : nullptr; }
    iterator end() const { return els ? &(els[len]) : nullptr; }
};
} // namespace wstr

namespace wstr {

bool Replace(Builder& s, WStr toReplace, WStr replaceWith);
bool ContainsChar(const Builder& b, WCHAR el);

} // namespace wstr

int ParseInt(Str s);
i64 ParseInt64(Str s);
bool IsValidProgramVersion(Str ver);
int CompareProgramVersion(Str ver1, Str ver2);
bool IsTextRtl(WStr s);
bool IsTextRtl(Str s);

char* CStrTemp(Str s);
WCHAR* CWStrTemp(WStr s);

WCHAR* CWStrTemp(WStr s, int& cch);

Str ToStr(const str::Builder&);
char* ToCStr(const str::Builder&);
WStr ToWStr(const wstr::Builder&);
WCHAR* ToWCStr(const wstr::Builder&);

TempStr ToStrTemp(const str::Builder&);

int len(const str::Builder&);
int len(const wstr::Builder&);

wchar_t ToLowerW(wchar_t c);
int WStrFindSubstr(WStr str, WStr substr);
int WStrCmpNoCase(WStr a, WStr b);

Str FormatFileSize(Arena* arena, u64 size);
void FormatFileSizeToWstrBuf(u64 size, WStr buf);
int FormatSizeHumanIntoBuf(u64 size, Str buf);
void FormatSizeHumanIntoWBuf(u64 size, WStr wbuf);

// ─── StrUtf8.h ───────────────────────────────────────────────────────────────

bool isLegalUTF8Sequence(const u8* source, const u8* sourceEnd);
bool isLegalUTF8String(const u8** source, const u8* sourceEnd);
int utf8StrLen(const u8* s);
int utf8RuneLen(const u8* s);

namespace str {
void Utf8Encode(char* buf, int& off, int c);
int VsnprintfUtf8(Str buf, const char* fmt, va_list args);
} // namespace str

bool Utf8IsContinuationByte(char c);
int Utf8CodepointCount(Str s);
int Utf8CodepointAtByte(Str s, int byteIdx, int* bytesOut = nullptr);
// byteIdx may point into the middle of a sequence, unlike in the functions above
int Utf8CodepointStartByte(Str s, int byteIdx);
int Utf8CodepointContaining(Str s, int byteIdx);
int Utf8CodepointNext(Str s, int& byteIdx);
int Utf8CodepointPrev(Str s, int& byteIdx);
int Utf8CodepointToByteIndex(Str s, int codepointIdx);
int Utf8AdvanceCodepoints(Str s, int byteIdx, int nCodepoints);
Str Utf8SliceByCodepoints(Str s, int startCodepoint, int nCodepoints);

TempStr ShortenStringUtf8Temp(Str s, int maxRunes);
TempStr ShortenStringUtf8InTheMiddleTemp(Str s, int maxRunes);

WStr ToWStrTemp(Str s);
Str ToUtf8(Arena* arena, WStr wide);
Str ToUtf8Temp(WStr wide);
WCHAR* CWStrTemp(Str s);
WCHAR* CWStrTemp(Str s, int& cch);

// ─── StrFormatParse.h ───────────────────────────────────────────────────────────────

namespace str {

// argument to a formatting instruction
// at the front are arguments given with i(), s() etc.
// at the end are FormatStr arguments from format string
struct FmtArg {
    enum class Kind {
        // concrete types for FmtArg
        Char,
        Int,
        Ptr,
        Float,
        Double,
        Str,
        WStr,

        // for Inst.t
        RawStr, // copy part of format string
        Any,

        None,
    };

    Kind t{Kind::None};
    // A FmtArg only ever holds one value, selected by t, so the value fields
    // share storage. Str/WStr are trivially copyable (only their default ctor is
    // non-trivial), so they are valid union members; the union has no implicit
    // default ctor, hence FmtArg() initializes a trivial member explicitly.
    union {
        Str str;
        WStr wstr;
        char c;
        i64 i;
        float f;
        double d;
        const void* ptr;
    };

    FmtArg() : i{0} {} // t stays None; init the union via a trivial member

    // All single-argument constructors are explicit so values never implicitly
    // convert to FmtArg (e.g. a stray pointer or bool). FormatTemp() is a
    // variadic template that constructs each FmtArg explicitly, so call sites
    // stay terse.

    explicit FmtArg(char c_) : t{Kind::Char}, c{c_} {}

    // integer family: a constructor per fundamental type so e.g. unsigned int
    // (UINT/DWORD), unsigned long, size_t etc. are not ambiguous between
    // FmtArg(int)/FmtArg(i64)/FmtArg(size_t). The conversion char (%d vs %u vs
    // %x) decides the rendering; we just carry the value as i64.
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

    explicit FmtArg(const void* p) : t{Kind::Ptr}, ptr{p} { // for %p
    }

    // raw C strings are not allowed: pass Str / WStr explicitly so we never
    // depend on a NUL-terminated char*/wchar_t*
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

Str FormatArgs(Arena* a, const char* fmt, const FmtArg** args, int nArgs);

inline Str Format(Arena* a, const char* fmt) {
    return FormatArgs(a, fmt, nullptr, 0);
}

template <typename... TArgs>
Str Format(Arena* a, const char* fmt, const TArgs&... args) {
    const FmtArg argv[] = {FmtArg(args)...};
    const FmtArg* argp[sizeof...(TArgs)];
    int n = (int)sizeof...(TArgs);
    for (int i = 0; i < n; i++) {
        argp[i] = &argv[i];
    }
    return FormatArgs(a, fmt, argp, n);
}

// Type-safe scanf-style parsing (analogous to str::Format). Each output arg
// is a pointer whose type is captured by ParseArg's explicit constructors, so a
// format/arg mismatch (e.g. %d into a WORD*) is a compile error, not silent UB.
// %s / %S write a TempStr (or TempWStr) into the passed-in pointer -- the result
// lives in the temp arena, so the caller doesn't free it; the numeric/char specs
// write via the matching pointer type.
struct ParseArg {
    enum class Kind : u8 {
        None,
        Int,
        UInt,
        Float,
        Char,
        StrOut,
        WStrOut
    };
    Kind kind = Kind::None;
    void* ptr = nullptr;

    ParseArg() = default;
    explicit ParseArg(int* p) : kind(Kind::Int), ptr(p) {}
    explicit ParseArg(unsigned int* p) : kind(Kind::UInt), ptr(p) {}
    explicit ParseArg(float* p) : kind(Kind::Float), ptr(p) {}
    explicit ParseArg(char* p) : kind(Kind::Char), ptr(p) {}
    explicit ParseArg(Str* p) : kind(Kind::StrOut), ptr(p) {}
    explicit ParseArg(WStr* p) : kind(Kind::WStrOut), ptr(p) {}
};

Str ParseArgs(Str str, const char* fmt, const ParseArg* args, int nArgs);

inline Str Parse(Str str, const char* fmt) {
    return ParseArgs(str, fmt, nullptr, 0);
}

template <typename... TArgs>
Str Parse(Str str, const char* fmt, TArgs*... args) {
    const ParseArg argv[] = {ParseArg(args)...};
    return ParseArgs(str, fmt, argv, (int)sizeof...(TArgs));
}

TempStr FormatFloatWithThousandSepTemp(double number, LCID locale = LOCALE_USER_DEFAULT, bool stripTrailingZero = true);
TempStr FormatNumWithThousandSepTemp(i64 num, LCID locale = LOCALE_USER_DEFAULT);
TempStr FormatSizeShortTemp(i64 size);
TempStr FormatSizeShortTemp(i64 size, Str const* sizeUnits);
TempStr FormatFileSizeTemp(i64);
TempStr FormatRomanNumeralTemp(int number);

} // namespace str

// fmt() is the type-safe positional/printf-style formatter from StrFormat.h.
// A function-like macro (not a function) so only fmt(...) call syntax is
// rewritten -- identifiers named `fmt` (params, locals like `Format fmt`) are
// untouched.
#define fmt(...) str::FormatTemp(__VA_ARGS__)

// ─── Strconv.h ───────────────────────────────────────────────────────────────

namespace strconv {

WStr Utf8ToWStr(Str s, Arena* a = nullptr);
Str WStrToUtf8(WStr s, Arena* a = nullptr);

Str WStrToCodePage(uint codePage, WStr s, Arena* a = nullptr);
TempStr ToMultiByteTemp(Str src, uint codePageSrc, uint codePageDest);
WStr StrCPToWStr(Str src, uint codePage);
TempWStr StrCPToWStrTemp(Str src, uint codePage);
TempStr StrToUtf8Temp(Str src, uint codePage);

TempStr UnknownToUtf8Temp(Str s);

Str WStrToAnsi(WStr src);
Str Utf8ToAnsi(Str s);

TempWStr AnsiToWStrTemp(Str src);
Str AnsiToUtf8(Str src);
TempStr AnsiToUtf8Temp(Str src);
} // namespace strconv

Str ToUtf8(WStr s, Arena* a = nullptr);
WStr ToWStr(Str s, Arena* a = nullptr);
// ─── Scoped.h ───────────────────────────────────────────────────────────────

// auto-free memory for arbitrary malloc()ed memory of type T*
template <typename T>
class ScopedMem {
  public:
    T* ptr = nullptr;

    ScopedMem() = default;
    explicit ScopedMem(T* ptr) : ptr(ptr) {}
    ~ScopedMem() { free(ptr); }
    void Set(T* newPtr) {
        free(ptr);
        ptr = newPtr;
    }
    T* Get() const { return ptr; }
    T* Take() {
        T* tmp = ptr;
        ptr = nullptr;
        return tmp;
    }
    operator T*() const { // NOLINT
        return ptr;
    }
};

// deletes an object at the end of the scope
template <typename T>
struct AutoDelete {
    T* o = nullptr;
    AutoDelete() = default;
    AutoDelete(T* p) { // NOLINT
        o = p;
    }
    ~AutoDelete() { delete o; }

    AutoDelete& operator=(AutoDelete& other) = delete;
    AutoDelete& operator=(AutoDelete&& other) = delete;
    AutoDelete& operator=(const AutoDelete& other) = delete;
    AutoDelete& operator=(const AutoDelete&& other) = delete;
    operator T*() const { // NOLINT
        return o;
    }
    T* operator->() const { // NOLINT
        return o;
    }
};

template <typename Fn>
struct AutoCall;

template <typename Result>
struct AutoCall<Result (*)()> {
    using Fn = Result (*)();
    Fn fn = nullptr;
    AutoCall() = default;
    AutoCall(Fn fn) { this->fn = fn; } // NOLINT
    AutoCall(AutoCall& other) = delete;
    AutoCall(AutoCall&& other) = delete;
    AutoCall(const AutoCall& other) = delete;
    AutoCall(const AutoCall&& other) = delete;
    ~AutoCall() {
        if (fn) {
            fn();
        }
    }

    AutoCall& operator=(AutoCall& other) = delete;
    AutoCall& operator=(AutoCall&& other) = delete;
    AutoCall& operator=(const AutoCall& other) = delete;
    AutoCall& operator=(const AutoCall&& other) = delete;
};

template <typename Result, typename Arg>
struct AutoCall<Result (*)(Arg)> {
    using Fn = Result (*)(Arg);
    Fn fn = nullptr;
    Arg arg{};
    AutoCall() = default;
    AutoCall(Fn fn, Arg arg) { // NOLINT
        this->fn = fn;
        this->arg = arg;
    }
    AutoCall(AutoCall& other) = delete;
    AutoCall(AutoCall&& other) = delete;
    AutoCall(const AutoCall& other) = delete;
    AutoCall(const AutoCall&& other) = delete;
    ~AutoCall() {
        if (fn) {
            fn(arg);
        }
    }

    AutoCall& operator=(AutoCall& other) = delete;
    AutoCall& operator=(AutoCall&& other) = delete;
    AutoCall& operator=(const AutoCall& other) = delete;
    AutoCall& operator=(const AutoCall&& other) = delete;
};

template <typename Result, typename Arg1, typename Arg2>
struct AutoCall<Result (*)(Arg1, Arg2)> {
    using Fn = Result (*)(Arg1, Arg2);
    Fn fn = nullptr;
    Arg1 arg1{};
    Arg2 arg2{};
    AutoCall() = default;
    AutoCall(Fn fn, Arg1 arg1, Arg2 arg2) { // NOLINT
        this->fn = fn;
        this->arg1 = arg1;
        this->arg2 = arg2;
    }
    AutoCall(AutoCall& other) = delete;
    AutoCall(AutoCall&& other) = delete;
    AutoCall(const AutoCall& other) = delete;
    AutoCall(const AutoCall&& other) = delete;
    ~AutoCall() {
        if (fn) {
            fn(arg1, arg2);
        }
    }

    AutoCall& operator=(AutoCall& other) = delete;
    AutoCall& operator=(AutoCall&& other) = delete;
    AutoCall& operator=(const AutoCall& other) = delete;
    AutoCall& operator=(const AutoCall&& other) = delete;
};

template <typename Result>
AutoCall(Result (*)()) -> AutoCall<Result (*)()>;
template <typename Result, typename Arg>
AutoCall(Result (*)(Arg), Arg) -> AutoCall<Result (*)(Arg)>;
template <typename Result, typename Arg1, typename Arg2>
AutoCall(Result (*)(Arg1, Arg2), Arg1, Arg2) -> AutoCall<Result (*)(Arg1, Arg2)>;


// Windows/MSVC string APIs: use str::/wstr:: BufSet, EqI, CmpI instead.
#ifdef lstrcpy
#undef lstrcpy
#define lstrcpy dont_use_lstrcpy
#endif
#ifdef lstrcpyn
#undef lstrcpyn
#define lstrcpyn dont_use_lstrcpyn
#endif
#ifdef lstrcpynW
#undef lstrcpynW
#define lstrcpynW dont_use_lstrcpynW
#endif
#ifdef lstrcmpiA
#undef lstrcmpiA
#define lstrcmpiA dont_use_lstrcmpiA
#endif
#ifdef lstrcmpiW
#undef lstrcmpiW
#define lstrcmpiW dont_use_lstrcmpiW
#endif
