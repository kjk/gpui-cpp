/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "Base.h"

void* AllocZero(int count, int size) {
    return calloc(count, size);
}

// ─── Arena.cpp ───────────────────────────────────────────────────────────────

u64 gArenaDefaultReserveSize = 64ull * 1024ull * 1024ull;
u64 gArenaDefaultCommitSize = 64ull * 1024ull;
ArenaFlags gArenaDefaultFlags = 0;

static u64 ArenaAlignPow2(u64 value, u64 align) {
    if (align <= 1) {
        return value;
    }
    return (value + align - 1) & ~(align - 1);
}

static u64 ArenaMin(u64 a, u64 b) {
    return (a < b) ? a : b;
}

static u64 ArenaMax(u64 a, u64 b) {
    return (a > b) ? a : b;
}

static u64 ArenaClampTop(u64 value, u64 maxValue) {
    return (value < maxValue) ? value : maxValue;
}

static u64 ArenaClampBot(u64 minValue, u64 value) {
    return (value > minValue) ? value : minValue;
}

u64 ArenaPageSize();
u64 ArenaLargePageSize();
bool ArenaCommit(void* base, u64 size, bool largePages);
void* ArenaReserve(u64 size);
void* ArenaReserveAndCommit(u64 size, bool largePages);
void ArenaReleaseMemory(void* base, u64 size);

static void ArenaRelease(Arena* arena) {
    ArenaReleaseMemory(arena, arena->reserved);
}

static void* ArenaPushLocked(Arena* arena, u64 size, u64 align, bool zero) {
    if (!arena) {
        return nullptr;
    }
    if (align == 0) {
        align = 1;
    }

    Arena* current = arena->current;
    u64 posPre = ArenaAlignPow2(current->pos, align);
    u64 posPost = posPre + size;

    u64 sizeToZero = 0;
    if (zero && current->committed > posPre) {
        sizeToZero = ArenaMin(current->committed, posPost) - posPre;
    }

    if (current->reserved < posPost && !(arena->flags & ArenaFlagNoChain)) {
        u64 reserveChunkSize = current->reserveChunkSize;
        u64 commitChunkSize = current->commitChunkSize;
        if (size + kArenaHeaderSize > reserveChunkSize) {
            reserveChunkSize = ArenaAlignPow2(size + kArenaHeaderSize,
                                              ArenaMax(align, ArenaPageSize()));
            commitChunkSize = reserveChunkSize;
        }

        ArenaParams newParams = {};
        newParams.flags = current->flags;
        newParams.reserveSize = reserveChunkSize;
        newParams.commitSize = commitChunkSize;
        newParams.allocationSiteFile = current->allocationSiteFile;
        newParams.allocationSiteLine = current->allocationSiteLine;
        newParams.name = current->name;

        Arena* newBlock = ArenaNew(newParams);
        if (!newBlock) {
            return nullptr;
        }

        newBlock->basePos = current->basePos + current->reserved;
        newBlock->prev = current;
        arena->current = newBlock;
        current = newBlock;
        posPre = ArenaAlignPow2(current->pos, align);
        posPost = posPre + size;
        sizeToZero = 0;
    }

    if (current->committed < posPost) {
        if (current->flags & ArenaFlagLargePages) {
            return nullptr;
        }

        u64 commitEnd = ArenaAlignPow2(posPost, current->commitChunkSize);
        u64 commitClamped = ArenaClampTop(commitEnd, current->reserved);
        u64 commitSize = commitClamped - current->committed;
        void* commitPtr = (char*)current + current->committed;
        if (!ArenaCommit(commitPtr, commitSize, false)) {
            return nullptr;
        }
        current->committed = commitClamped;
    }

    if (current->committed < posPost) {
        return nullptr;
    }

    void* result = (char*)current + posPre;
    current->pos = posPost;

    // update allocation stats on the head arena (stats live on the head, not on
    // chained blocks). peak is the high-water mark of total bytes used.
    arena->nAllocsLifetime++;
    arena->nAllocsSinceReset++;
    u64 used = current->basePos + posPost;
    arena->peakBytesLifetime = std::max(used, arena->peakBytesLifetime);
    arena->peakBytesSinceReset = std::max(used, arena->peakBytesSinceReset);

    if (sizeToZero) {
        memset(result, 0, (size_t)sizeToZero);
    }
    return result;
}

ArenaParams ArenaDefaultParams() {
    ArenaParams params = {};
    params.flags = gArenaDefaultFlags;
    params.reserveSize = gArenaDefaultReserveSize;
    params.commitSize = gArenaDefaultCommitSize;
    return params;
}

Arena* ArenaNew(const ArenaParams& srcParams) {
    ArenaParams params = srcParams;
    if (params.reserveSize == 0) {
        params.reserveSize = gArenaDefaultReserveSize;
    }
    if (params.commitSize == 0) {
        params.commitSize = gArenaDefaultCommitSize;
    }

    bool useLargePages = (params.flags & ArenaFlagLargePages) != 0;
    const u64 pageSize = useLargePages ? ArenaLargePageSize() : ArenaPageSize();
    u64 reserveSize = ArenaAlignPow2(
        ArenaMax(params.reserveSize, kArenaHeaderSize), pageSize);
    u64 commitSize =
        ArenaAlignPow2(ArenaMax(params.commitSize, kArenaHeaderSize), pageSize);
    commitSize = ArenaClampTop(commitSize, reserveSize);

    void* base = params.optionalBackingBuffer;
    bool usesExternalBuffer = (base != nullptr);
    ArenaFlags actualFlags = params.flags;

    if (!usesExternalBuffer) {
        if (useLargePages) {
            base = ArenaReserveAndCommit(reserveSize, true);
            if (base) {
                commitSize = reserveSize;
            } else {
                actualFlags &= ~ArenaFlagLargePages;
                useLargePages = false;
                reserveSize = ArenaAlignPow2(reserveSize, ArenaPageSize());
                commitSize = ArenaAlignPow2(commitSize, ArenaPageSize());
            }
        }

        if (!base) {
            base = ArenaReserve(reserveSize);
            if (base && !ArenaCommit(base, commitSize, false)) {
                ArenaReleaseMemory(base, reserveSize);
                base = nullptr;
            }
        }
    } else {
        commitSize = reserveSize;
    }

    if (!base) {
        return nullptr;
    }

    memset(base, 0, (size_t)std::min<u64>(commitSize, kArenaHeaderSize));
    Arena* arena = (Arena*)base;
    arena->prev = nullptr;
    arena->current = arena;
    arena->flags = actualFlags;
    arena->commitChunkSize = useLargePages ? reserveSize : commitSize;
    arena->reserveChunkSize = reserveSize;
    arena->basePos = 0;
    arena->pos = kArenaHeaderSize;
    arena->committed = commitSize;
    arena->reserved = reserveSize;
    arena->allocationSiteFile = params.allocationSiteFile;
    arena->allocationSiteLine = params.allocationSiteLine;
    arena->name = params.name;
    arena->usesExternalBuffer = usesExternalBuffer;
    arena->nAllocsLifetime = 0;
    arena->peakBytesLifetime = 0;
    arena->nAllocsSinceReset = 0;
    arena->peakBytesSinceReset = 0;
    return arena;
}

void ArenaDelete(Arena* arena) {
    if (!arena) {
        return;
    }

    Arena* node = arena->current;
    while (node) {
        Arena* prev = node->prev;
        if (!node->usesExternalBuffer) {
            ArenaRelease(node);
        }
        node = prev;
    }
}

void* Arena::Push(u64 size, u64 align, bool zero) {
    if (!this) {
        return nullptr;
    }
    lock.Lock();
    void* mem = ArenaPushLocked(this, size, align, zero);
    lock.Unlock();
    return mem;
}

void Arena::PopTo(u64 pos) {
    Arena* arena = this;
    if (!arena) {
        return;
    }

    lock.Lock();

    u64 bigPos = ArenaClampBot(kArenaHeaderSize, pos);
    Arena* current = arena->current;
    while (current && current->basePos >= bigPos) {
        Arena* prev = current->prev;
        if (!current->usesExternalBuffer) {
            ArenaRelease(current);
        } else {
            current->pos = kArenaHeaderSize;
        }
        current = prev;
    }

    if (!current) {
        lock.Unlock();
        return;
    }

    arena->current = current;
    u64 newPos = bigPos - current->basePos;
    current->pos = newPos;
    lock.Unlock();
}

void* Arena::Alloc(int size) {
    if (size <= 0) {
        return nullptr;
    }
    return Push((u64)size, 8, false);
}

void Arena::Reset() {
    PopTo(0);
    nAllocsSinceReset = 0;
    peakBytesSinceReset = 0;
}

// size_t overloads that match the legacy Allocator::* static helper API
// and fall back to malloc/free when arena is nullptr.
void* Alloc(Arena* arena, int size) {
    if (size <= 0) {
        return nullptr;
    }
    if (!arena) {
        return malloc(size);
    }
    return arena->Alloc(size);
}

void Free(Arena* arena, void* mem) {
    // Arena has no free
    if (arena) return;
    free(mem);
}

// size_t overloads that match the legacy Allocator::* static helper API
// and fall back to malloc/free when arena is nullptr.
void* Alloc(Arena* arena, size_t size) {
    if (size == 0) {
        return nullptr;
    }
    if (!arena) {
        return malloc(size);
    }
    return arena->Push((u64)size, 8, false);
}

void* AllocZero(Arena* arena, size_t size) {
    if (size == 0) {
        return nullptr;
    }
    if (!arena) {
        void* mem = malloc(size);
        if (mem) {
            memset(mem, 0, size);
        }
        return mem;
    }
    return arena->Push((u64)size, 8, true);
}

void* Realloc(Arena* arena, void* mem, size_t newSize, size_t copySize) {
    if (!arena) {
        return realloc(mem, newSize);
    }
    // Arena has no realloc: allocate fresh and copy. Old memory is not freed
    // (arena lifetime handles it).
    if (newSize == 0) {
        return nullptr;
    }
    void* newMem = arena->Push((u64)newSize, 8, false);
    if (newMem && mem && copySize > 0) {
        // Arena bump allocations can end up adjacent to (and overlapping) the
        // old block; memmove handles that. copySize is the caller's used bytes.
        size_t n = copySize;
        n = std::min(n, newSize);
        memmove(newMem, mem, n);
    }
    return newMem;
}

void* MemDup(Arena* arena, const void* mem, size_t size, size_t extraBytes) {
    void* newMem = Alloc(arena, size + extraBytes);
    if (!newMem) {
        return nullptr;
    }
    if (mem && size) {
        memcpy(newMem, mem, size);
    }
    // zero the tail so callers using extraBytes to append a null terminator
    // (e.g. StrDup with extraBytes = sizeof(char)) don't read uninitialized
    // memory. When allocated from an arena via Push(..., zero=false) or from
    // malloc() the bytes past `size` aren't otherwise zeroed.
    if (extraBytes > 0) {
        memset((char*)newMem + size, 0, extraBytes);
    }
    return newMem;
}

thread_local Arena* gTempArena = nullptr;

Arena* GetTempArena() {
    if (!gTempArena) {
        gTempArena = ArenaNew();
    }
    return gTempArena;
}

void ResetTempArena() {
    if (gTempArena) {
        gTempArena->Reset();
    }
}

void DestroyTempArena() {
    ArenaDelete(gTempArena);
    gTempArena = nullptr;
}

// allocate null-terminated string
Str AllocStrTemp(int size) {
    if (size == 0) {
        return {};
    }
    Arena* arena = GetTempArena();
    char* res = (char*)arena->Push((u64)size + 1, 1, false);
    res[size] = 0;
    return Str(res, size);
}

// Grow/shrink vec storage to newCap elements, plus one trailing zero-pad
// element (so Vec<char>/Vec<WCHAR> stay C-string compatible).
// Keeps the first min(len, newCap) elements; zeros the rest of the new block.
// Updates *els and *cap. len is not modified (caller owns logical length).
// Grow/shrink vec-like storage to newCap elements (+1 trailing zero pad).
// Updates *els and *cap; keeps min(len, newCap) elements.
GPUI_NO_INLINE bool VecRealloc(Arena* a, void** els, int len, int* cap,
                               int newCap, int elSize) {
    // newCap+1 must fit in int; newElCount * elSize must not overflow.
    if (elSize <= 0 || newCap < 0 || newCap > INT_MAX - 1) {
        return false;
    }
    int newElCount = newCap + 1;
    if (newElCount > INT_MAX / elSize) {
        return false;
    }

    int keep = len;
    keep = std::max(keep, 0);
    keep = std::min(keep, newCap);
    int oldSize = keep * elSize;
    int allocSize = newElCount * elSize;

    // Realloc(a, nullptr, n, 0) is malloc-like; single path for first alloc and
    // grow.
    void* newEls = Realloc(a, *els, (size_t)allocSize, (size_t)oldSize);
    if (!newEls) {
        return false;
    }
    int tail = allocSize - oldSize;
    if (tail > 0) {
        memset((char*)newEls + oldSize, 0, (size_t)tail);
    }
    *els = newEls;
    *cap = newCap;
    return true;
}

// ─── Arena_win.cpp
// ───────────────────────────────────────────────────────────────

u64 ArenaPageSize() {
    static u64 pageSize = 0;
    if (pageSize == 0) {
        SYSTEM_INFO info = {};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
    }
    return pageSize;
}

u64 ArenaLargePageSize() {
    static u64 largePageSize = 0;
    if (largePageSize == 0) {
        SIZE_T size = GetLargePageMinimum();
        largePageSize = size ? (u64)size : ArenaPageSize();
    }
    return largePageSize;
}

bool ArenaCommit(void* base, u64 size, bool largePages) {
    if (size == 0) {
        return true;
    }
    DWORD flags = MEM_COMMIT;
    if (largePages) {
        flags |= MEM_LARGE_PAGES;
    }
    return VirtualAlloc(base, (SIZE_T)size, flags, PAGE_READWRITE) != nullptr;
}

void* ArenaReserve(u64 size) {
    return VirtualAlloc(nullptr, (SIZE_T)size, MEM_RESERVE, PAGE_READWRITE);
}

void* ArenaReserveAndCommit(u64 size, bool largePages) {
    DWORD flags = MEM_RESERVE | MEM_COMMIT;
    if (largePages) {
        flags |= MEM_LARGE_PAGES;
    }
    return VirtualAlloc(nullptr, (SIZE_T)size, flags, PAGE_READWRITE);
}

void ArenaReleaseMemory(void* base, u64 size) {
    (void)size;
    VirtualFree(base, 0, MEM_RELEASE);
}

static Str WrapAllocated(char* s, int cch = -1) {
    if (!s) {
        return {};
    }
    if (cch < 0) {
        return Str(s);
    }
    return Str(s, cch);
}

Str StrDup(Arena* a, Str s) {
    if (StrIsNull(s) || s.len < 0) {
        return {};
    }
    int cch = s.len;
    return WrapAllocated(
        (char*)MemDup(a, s.s, (size_t)cch * sizeof(char), sizeof(char)), cch);
}

Str StrDup(Str s) {
    return StrDup(nullptr, s);
}

void StrFree(Str s) {
    free(s.s);
}

bool StrEq(Str s1, Str s2) {
    if (s1.s == s2.s) {
        return true;
    }
    int len1 = 0;
    while (!StrIsNull(s1) && len1 < s1.len && s1.s[len1]) {
        len1++;
    }
    int len2 = 0;
    while (!StrIsNull(s2) && len2 < s2.len && s2.s[len2]) {
        len2++;
    }
    if (len1 != len2) {
        return false;
    }
    if (len1 == 0) {
        return true;
    }
    if (StrIsNull(s1) || StrIsNull(s2)) {
        return false;
    }
    return 0 == memcmp(s1.s, s2.s, (size_t)len1);
}

// return true if s1 == s2, case insensitive
bool StrEqI(Str s1, Str s2) {
    if (s1.s == s2.s) {
        return true;
    }
    if (s1.len != s2.len) {
        return false;
    }
    if (s1.len == 0) {
        return true;
    }
    if (StrIsNull(s1) || StrIsNull(s2)) {
        return false;
    }
    return 0 == _strnicmp(s1.s, s2.s, (size_t)s1.len);
}

bool StrContainsI(Str s, Str sub) {
    if (!s || !sub || sub.len <= 0) {
        return false;
    }
    for (int off = 0; off + sub.len <= s.len; off++) {
        if (StrEqNI(Str(s.s + off, s.len - off), sub, sub.len)) {
            return true;
        }
    }
    return false;
}

bool StrEqNI(Str s1, Str s2, int n) {
    if (s1.s == s2.s) {
        return true;
    }
    if (!s1 || !s2 || n == 0) {
        return n == 0;
    }
    if (s1.len < n || s2.len < n) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        if (tolower(s1.s[i]) != tolower(s2.s[i])) {
            return false;
        }
    }
    return true;
}

bool IsDigit(char c) {
    return ('0' <= c) && (c <= '9');
}

// for compatibility with C string, the last character is always 0
// kPadding is number of characters needed for terminating character
static constexpr int kPadding = 1;

// using external scratch, or no storage yet (not heap)
static bool IsExternalOrEmpty(const StrBuilder* s) {
    return !s->els || (s->buf.s && s->els == s->buf.s);
}

static char* EnsureCap(StrBuilder* s, int needed) {
    // only use external buf if we haven't moved to the heap yet.
    // RemoveAt() can shrink len enough for needed to fit again and switching
    // back would lose the data and leak the heap allocation.
    if (IsExternalOrEmpty(s) && s->buf.s && needed + kPadding <= s->buf.len) {
        s->els = s->buf.s;
        return s->els;
    }

    int capacityHint = s->cap;
    // tricky: to save space we reuse cap for capacityHint while still on
    // external/empty storage (cap was set from constructor hint)
    if (IsExternalOrEmpty(s)) {
        s->cap = 0;
    }

    if (s->els && s->cap >= needed) {
        return s->els;
    }

    int newCap = s->cap * 2;
    newCap = std::max(needed, newCap);
    newCap = std::max(newCap, capacityHint);

    int newElCount = newCap + kPadding;

    s->nReallocs++;

    int allocSize = newElCount;
    char* newEls;
    if (IsExternalOrEmpty(s)) {
        newEls = (char*)Alloc(s->a, allocSize);
        if (newEls && s->els && s->len > 0) {
            memcpy(newEls, s->els, (size_t)s->len + 1);
        } else if (newEls) {
            newEls[0] = 0;
        }
    } else {
        newEls = (char*)Realloc(s->a, s->els, (size_t)allocSize,
                                (size_t)s->len + kPadding);
    }
    if (!newEls) {
        return nullptr;
    }
    s->els = newEls;
    s->cap = newCap;
    return newEls;
}

static char* MakeSpaceAt(StrBuilder* s, int idx, int count) {
    int newLen = std::max(s->len, idx) + count;
    char* buf = EnsureCap(s, newLen);
    if (!buf) {
        return nullptr;
    }
    buf[newLen] = 0;
    char* res = &(buf[idx]);
    if (s->len > idx) {
        // inserting in the middle of string, have to copy
        char* src = buf + idx;
        char* dst = buf + idx + count;
        memmove(dst, src, (size_t)(s->len - idx));
    }
    s->len = newLen;
    // ZeroMemory(res, count);
    return res;
}

static void StrBuilderReset(StrBuilder* s) {
    s->len = 0;
    // keep an existing heap buffer for re-use; only bind external buf when
    // we have not allocated heap yet
    if (!s->els || (s->buf.s && s->els == s->buf.s)) {
        s->els = s->buf.s; // may be null when no external buf
    }
    if (s->els) {
        s->els[0] = 0;
    }
}

static void StrBuilderFree(StrBuilder* s) {
    if (s->els && !(s->buf.s && s->els == s->buf.s)) {
        Free(s->a, s->els);
    }
    s->len = 0;
    s->cap = 0;
    s->els = s->buf.s;
    if (s->els) {
        s->els[0] = 0;
    }
}

void StrBuilder::Reset(Str s) {
    StrBuilderReset(this);
    Append(s); // no-op if s is empty
}

// arena is not owned by Builder; set .a after construction if needed
// capHint: preferred capacity after first grow
// capHint: preferred capacity after first grow
StrBuilder::StrBuilder(Str externalBuf) {
    this->buf = externalBuf;
    Reset();
}

// capHint: preferred capacity after first grow
// capHint: preferred capacity after first grow
StrBuilder::StrBuilder(int capHint) {
    Reset();
    cap = capHint + kPadding; // + kPadding for terminating 0
}

StrBuilder::~StrBuilder() {
    StrBuilderFree(this);
}

char& StrBuilder::operator[](int idx) const {
    return els[idx];
}

int len(const StrBuilder& b) {
    return b.len;
}

bool StrBuilder::InsertAt(int idx, char el) {
    char* p = MakeSpaceAt(this, idx, 1);
    if (!p) {
        return false;
    }
    p[0] = el;
    return true;
}

bool StrBuilder::AppendChar(char c) {
    return InsertAt(len, c);
}

bool StrBuilder::Append(Str src) {
    if (StrIsNull(src) || 0 == src.len) {
        return true;
    }
    char* dst = MakeSpaceAt(this, len, src.len);
    if (!dst) {
        return false;
    }
    memcpy(dst, src.s, (size_t)src.len);
    return true;
}

// perf hack for using as a buffer: client can get accumulated data
// without duplicate allocation. Note: since Vec over-allocates, this
// is likely to use more memory than strictly necessary, but in most cases
// it doesn't matter
Str StrBuilder::TakeStr() {
    int n = len;
    char* res = els;
    if (!els || n == 0) {
        Reset();
        return Str{};
    }
    if (buf.s && els == buf.s) {
        // data is in the external buffer, so we have to duplicate it
        res = (char*)MemDup(this->a, els, (size_t)n + kPadding);
        els = buf.s;
    } else {
        // we're returning the heap allocation; rebind to external if any
        els = buf.s;
    }

    Reset();
    return Str(res, n);
}

// ─── StrFormatParse.cpp
// ───────────────────────────────────────────────────────────────

/*
Fmt is type-safe printf()-like system. Every directive starts with '%':
the usual %d / %s / %f etc., plus two that take an argument of any type:

  %{}   the next argument, whatever its type (same as %v)
  %{$n} the n-th argument (0-based), whatever its type

%% is the only escape; '{' on its own is ordinary text, so registry paths,
GUIDs, CSS and JS templates pass through untouched.

Type safety is achieved by using strongly typed methods for adding arguments
(i(), c(), s() etc.). We also verify that the type of the argument matches
the type of formatting directive.

Positional directives are useful in translations with more than 1 argument
because in some languages translation is akward if you can't re-arrange
the order of arguments.

Idiomatic usage:
Fmt fmt("%d = %s");
char *s = fmt.i(5).s("5").Get(); // returns "5 = 5"
// s is valid until fmt is valid
// use .GetDup() to get a copy that must be free()d
// you can re-use fmt as:
s = fmt.ParseFormat("%{1} = %{2} + %{0}").i(3).s("3").s(L"

You can mix %-style and %{$n} directives but beware, as the rule for assigning
argument number to a plain % directive is simple (n-th argument position for
n-th % directive) but it's easy to mis-count when adding %{$n} to the mix.

TODO: similar approach could be used for type-safe scanf() replacement.
*/

// formatting instruction
struct Inst {
    FmtArg::Kind t = FmtArg::Kind::None;
    int argNo = 0;  // <0 for strings that come from formatting string
    int rawOff = 0; // offset into format for FmtArg::Kind::RawStr / start of
                    // fwp for % spec
    int sLen = 0;   // length, for FmtArg::Kind::RawStr

    // for a % spec: the conversion char and the flags+width+precision range
    // (everything between '%' and the length-modifier/conversion). We delegate
    // the actual formatting to snprintf, only normalizing the length modifier
    // so 32/64-bit semantics match printf exactly.
    char conv = 0;
    int intBits = 0; // 32 or 64 for integer-family conversions
    int fwpOff = 0;  // offset into format of flags+width+precision
    int fwpLen = 0;
    int width = 0; // parsed width (for manual %s padding)
    int prec = -1; // parsed precision, -1 if none (for manual %s)
    bool leftJust = false;
};

struct Fmt {
    Fmt() = default;
    ~Fmt() = default;

    bool Eval(const FmtArg** args, int nArgs);

    bool isOk =
        true; // true if mismatch between formatting instruction and args

    Str format;
    Inst instructions[32]{}; // 32 should be big enough for everybody
    int nInst = 0;

    int currArgNo = 0;
    int currPercArgNo = 0;
    StrBuilder res;

    char buf[256] = {};
};

static void addRawStr(Fmt& fmt, int off, size_t n) {
    if (n == 0) {
        return;
    }
    auto& i = fmt.instructions[fmt.nInst++];
    i.t = FmtArg::Kind::RawStr;
    i.rawOff = off;
    i.sLen = (int)n;
    i.argNo = -1;
}

// parse: %{} (the next argument) or %{$n} (positional). off points at the '{',
// the '%' has already been consumed. Both take an argument of any type.
static int parseArgDefBrace(Fmt& fmt, int off) {
    off++;
    int n = 0;
    bool positional = false;
    // a '{' with no closing '}' must not walk past the end of the format
    // string. Reachable via a translated format string (fmt(_TRA("...").s,
    // ...)).
    while (off < fmt.format.len && fmt.format.s[off] != '}') {
        if (!IsDigit(fmt.format.s[off])) {
            fmt.isOk = false;
            return off;
        }
        n = (n * 10) + (fmt.format.s[off] - '0');
        positional = true;
        off++;
    }
    if (off >= fmt.format.len) {
        fmt.isOk = false;
        return off;
    }
    if (fmt.nInst >= (int)dimof(fmt.instructions)) {
        fmt.isOk = false;
        return off;
    }
    auto& i = fmt.instructions[fmt.nInst++];
    i.t = FmtArg::Kind::Any;
    // %{} consumes arguments in order, like every other % directive
    i.argNo = positional ? n : fmt.currPercArgNo++;
    return off + 1;
}

static FmtArg::Kind typeFromConv(char c) {
    switch (c) {
        case 'c':
            return FmtArg::Kind::Char;
        case 'd':
        case 'i':
        case 'u':
        case 'o':
        case 'x':
        case 'X':
            return FmtArg::Kind::Int;
        case 'p':
            return FmtArg::Kind::Ptr;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A':
            return FmtArg::Kind::Float;
        case 's':
        case 'S':
            return FmtArg::Kind::Str;
        case 'v':
            return FmtArg::Kind::Any;
    }
    return FmtArg::Kind::None;
}

static bool startsWith(Str s, int off, const char* prefix) {
    int i = 0;
    while (prefix[i]) {
        if (off + i >= s.len || s.s[off + i] != prefix[i]) {
            return false;
        }
        i++;
    }
    return true;
}

// parse: %[flags][width][.prec][length]<conv>
// We capture flags+width+precision verbatim (fed to snprintf) and normalize the
// length modifier into an explicit 32/64-bit width so output matches printf.
static int parseArgDefPerc(Fmt& fmt, int off) {
    Str f = fmt.format;
    off++; // past '%'
    int fwpStart = off;
    bool leftJust = false;
    // flags
    while (off < f.len &&
           (f.s[off] == '-' || f.s[off] == '+' || f.s[off] == ' ' ||
            f.s[off] == '0' || f.s[off] == '#')) {
        if (f.s[off] == '-') {
            leftJust = true;
        }
        off++;
    }
    // width
    int width = 0;
    while (off < f.len && IsDigit(f.s[off])) {
        width = (width * 10) + (f.s[off] - '0');
        off++;
    }
    // precision
    int prec = -1;
    if (off < f.len && f.s[off] == '.') {
        off++;
        prec = 0;
        while (off < f.len && IsDigit(f.s[off])) {
            prec = (prec * 10) + (f.s[off] - '0');
            off++;
        }
    }
    int fwpEnd = off;
    // length modifier; determine integer width (32/64 on LLP64 / win64)
    int bits = 32;
    char lenMod = (off < f.len) ? f.s[off] : 0;
    bool is32BitLenMod =
        lenMod == 'l' || lenMod == 'h' || lenMod == 'L' || lenMod == 'w';
    // size_t / intmax_t / ptrdiff_t / MS size_t
    bool is64BitLenMod =
        lenMod == 'z' || lenMod == 'j' || lenMod == 't' || lenMod == 'I';
    if (startsWith(f, off, "I64")) {
        bits = 64;
        off += 3;
    } else if (startsWith(f, off, "I32")) {
        off += 3;
    } else if (startsWith(f, off, "ll")) {
        bits = 64;
        off += 2;
    } else if (startsWith(f, off, "hh")) {
        off += 2;
    } else if (is32BitLenMod) {
        off++; // long is 32-bit on win64
    } else if (is64BitLenMod) {
        bits = 64;
        off++;
    }
    char conv = (off < f.len) ? f.s[off] : 0;
    off++;

    auto& i = fmt.instructions[fmt.nInst++];
    i.t = typeFromConv(conv);
    i.argNo = fmt.currPercArgNo++;
    i.conv = conv;
    i.intBits = bits;
    i.fwpOff = fwpStart;
    i.fwpLen = fwpEnd - fwpStart;
    i.width = width;
    i.prec = prec;
    i.leftJust = leftJust;
    return off;
}

static bool hasInstructionWithArgNo(Inst* insts, int nInst, int argNo) {
    for (int i = 0; i < nInst; i++) {
        if (insts[i].argNo == argNo) {
            return true;
        }
    }
    return false;
}

static bool isIntLike(FmtArg::Kind t) {
    return t == FmtArg::Kind::Char || t == FmtArg::Kind::Int ||
           t == FmtArg::Kind::Ptr;
}

static bool validArgTypes(FmtArg::Kind instType, FmtArg::Kind argType) {
    if (instType == FmtArg::Kind::Any || instType == FmtArg::Kind::RawStr) {
        return true;
    }
    // integer-family specs (%c %d %u %x %p ...) accept any integer-like arg
    // (char / int / pointer), matching printf's leniency -- e.g. an HWND with
    // %x, or an int with %c.
    if (instType == FmtArg::Kind::Char || instType == FmtArg::Kind::Int ||
        instType == FmtArg::Kind::Ptr) {
        return isIntLike(argType);
    }
    if (instType == FmtArg::Kind::Float) {
        return argType == FmtArg::Kind::Float ||
               argType == FmtArg::Kind::Double;
    }
    if (instType == FmtArg::Kind::Str) {
        return argType == FmtArg::Kind::Str || argType == FmtArg::Kind::WStr;
    }
    return false;
}

static bool ParseFormat(Fmt& o, Str fmtStr) {
    o.format = fmtStr;
    o.nInst = 0;
    o.currPercArgNo = 0;
    o.currArgNo = 0;
    o.res.Reset();

    // parse formatting string, until a %$c, %{} or %{$n}
    // %% is how we escape %; nothing else is special, so a bare '{' is text
    int start = 0;
    int off = 0;
    while (off < fmtStr.len && fmtStr.s[off]) {
        char c = fmtStr.s[off];
        if ('%' == c) {
            // handle %%
            if (off + 1 < fmtStr.len && '%' == fmtStr.s[off + 1]) {
                addRawStr(o, start, off - start);
                start = off + 1;
                off += 2; // skip '%'
                continue;
            }
            addRawStr(o, start, off - start);
            if (off + 1 < fmtStr.len && '{' == fmtStr.s[off + 1]) {
                off = parseArgDefBrace(o, off + 1);
            } else {
                off = parseArgDefPerc(o, off);
            }
            start = off;
            continue;
        }
        off++;
    }
    addRawStr(o, start, off - start);

    int maxArgNo = -1; // -1 so an escape/literal-only format requires no args
    // check that arg numbers in %{$n} makes sense
    for (int i = 0; i < o.nInst; i++) {
        if (o.instructions[i].t == FmtArg::Kind::RawStr) {
            continue;
        }
        maxArgNo = std::max(o.instructions[i].argNo, maxArgNo);
    }

    // instructions[i].argNo can be duplicate
    // (we can have positional arg like {0} multiple times
    // but must cover all space from 0..nArgsExpected
    for (int i = 0; i <= maxArgNo; i++) {
        bool isOk = hasInstructionWithArgNo(o.instructions, o.nInst, i);
        if (!isOk) {
            return false;
        }
    }
    return true;
}

// format a single value into a caller-provided buffer via snprintf,
// NUL-terminating even on truncation. Avoids allocating (assuming vsnprintf
// doesn't allocate).
static void bufFmt(Str buf, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    VsnprintfUtf8(buf, fmt, args);
    va_end(args);
    buf.s[buf.len - 1] = 0;
}

// default formatting for {n} positional and %v: format by the arg's runtime
// type
static void evalDefault(Fmt& fmt, const FmtArg& arg) {
    TempStr s;
    Str buf(fmt.buf, (int)dimof(fmt.buf));
    switch (arg.t) {
        case FmtArg::Kind::Char:
            fmt.res.AppendChar(arg.c);
            break;
        case FmtArg::Kind::Int:
            bufFmt(buf, "%lld", (long long)arg.i);
            fmt.res.Append(fmt.buf);
            break;
        case FmtArg::Kind::Ptr:
            bufFmt(buf, "%p", arg.ptr);
            fmt.res.Append(fmt.buf);
            break;
        case FmtArg::Kind::Float:
            // Note: %G, unlike %f, avoids trailing '0'
            bufFmt(buf, "%G", (double)arg.f);
            fmt.res.Append(fmt.buf);
            break;
        case FmtArg::Kind::Double:
            bufFmt(buf, "%G", arg.d);
            fmt.res.Append(fmt.buf);
            break;
        case FmtArg::Kind::Str:
            fmt.res.Append(arg.str);
            break;
        case FmtArg::Kind::WStr:
            break;
        default:
            break;
    }
}

// extract an integer value from any integer-like arg (char / int / pointer) so
// %d/%x/%c/%p work with any of them, like printf.
static i64 argToI64(const FmtArg& arg) {
    switch (arg.t) {
        case FmtArg::Kind::Char:
            return (i64)arg.c;
        case FmtArg::Kind::Ptr:
            return (i64)(intptr_t)arg.ptr;
        default:
            return arg.i;
    }
}

// format a typed % spec by reconstructing a single-conversion printf format and
// delegating to snprintf (bufFmt), normalizing the length modifier so the
// 32/64-bit value width matches printf. %s padding/truncation is done by hand
// to avoid relying on the Str being NUL-terminated.
static void evalPercInst(Fmt& fmt, const Inst& inst, const FmtArg& arg) {
    char* buf = fmt.buf;
    Str bufS(fmt.buf, (int)dimof(fmt.buf));

    if (inst.conv == 's' || inst.conv == 'S') {
        Str sv = arg.str;
        int slen = sv.len;
        if (inst.prec >= 0 && inst.prec < slen) {
            slen = inst.prec;
        }
        int pad = inst.width - slen;
        pad = std::max(pad, 0);
        if (!inst.leftJust) {
            for (int j = 0; j < pad; j++) {
                fmt.res.AppendChar(' ');
            }
        }
        fmt.res.Append(Str(sv.s, slen));
        if (inst.leftJust) {
            for (int j = 0; j < pad; j++) {
                fmt.res.AppendChar(' ');
            }
        }
        return;
    }

    // build "%" + flags+width+precision into fbuf
    char fbuf[64];
    int k = 0;
    fbuf[k++] = '%';
    for (int j = 0; j < inst.fwpLen && k < (int)dimof(fbuf) - 5; j++) {
        fbuf[k++] = fmt.format.s[inst.fwpOff + j];
    }
    char conv = inst.conv;
    i64 ival = argToI64(arg);
    switch (conv) {
        case 'd':
        case 'i':
            if (inst.intBits == 64) {
                fbuf[k++] = 'l';
                fbuf[k++] = 'l';
                fbuf[k++] = 'd';
                fbuf[k] = 0;
                bufFmt(bufS, fbuf, (long long)ival);
            } else {
                fbuf[k++] = 'd';
                fbuf[k] = 0;
                bufFmt(bufS, fbuf, (int)ival);
            }
            fmt.res.Append(buf);
            break;
        case 'u':
        case 'o':
        case 'x':
        case 'X':
            if (inst.intBits == 64) {
                fbuf[k++] = 'l';
                fbuf[k++] = 'l';
                fbuf[k++] = conv;
                fbuf[k] = 0;
                bufFmt(bufS, fbuf, (unsigned long long)ival);
            } else {
                fbuf[k++] = conv;
                fbuf[k] = 0;
                bufFmt(bufS, fbuf, (unsigned int)(unsigned long long)ival);
            }
            fmt.res.Append(buf);
            break;
        case 'c':
            fbuf[k++] = 'c';
            fbuf[k] = 0;
            bufFmt(bufS, fbuf, (int)ival);
            fmt.res.Append(buf);
            break;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G':
        case 'a':
        case 'A': {
            fbuf[k++] = conv;
            fbuf[k] = 0;
            double dv = (arg.t == FmtArg::Kind::Double) ? arg.d : (double)arg.f;
            bufFmt(bufS, fbuf, dv);
            fmt.res.Append(buf);
        } break;
        case 'p': {
            // flags/width are uncommon (and platform-specific) for %p; emit
            // plain
            const void* pv = (arg.t == FmtArg::Kind::Ptr)
                                 ? arg.ptr
                                 : (const void*)(intptr_t)ival;
            bufFmt(bufS, "%p", pv);
            fmt.res.Append(buf);
        } break;
        default:
            break;
    }
}

bool Fmt::Eval(const FmtArg** args, int nArgs) {
    if (!isOk) {
        // if failed parsing format
        return false;
    }

    for (int n = 0; n < nInst; n++) {
        auto& inst = instructions[n];

        if (inst.t == FmtArg::Kind::RawStr) {
            res.Append(Str(format.s + inst.rawOff, inst.sLen));
            continue;
        }

        int argNo = inst.argNo;
        if (argNo < 0 || argNo >= nArgs) {
            isOk = false;
            return false;
        }

        const FmtArg& arg = *args[argNo];
        isOk = validArgTypes(inst.t, arg.t);
        if (!isOk) {
            return false;
        }

        if (inst.t == FmtArg::Kind::Any) {
            evalDefault(*this, arg);
        } else {
            evalPercInst(*this, inst, arg);
        }
    }
    return true;
}

// Format into an explicit arena; the returned Str lives in `a`. Use this
// instead of fmt()/FormatTemp when the result must outlive the temp
// allocator's scope, or on paths that must not touch the temp allocator / heap
// at all (e.g. the crash handler, which pre-allocates its arena).
// FormatTempArgs() is just this with GetTempArena().
Str FormatArgs(Arena* a, const char* fmt, const FmtArg** args, int nArgs) {
    // trailing arguments could be empty (unused defaults from the variadic
    // call)
    while (nArgs > 0 && args[nArgs - 1]->t == FmtArg::Kind::None) {
        nArgs--;
    }

    if (nArgs == 0) {
        // no args: if the format has no directives, return it verbatim (fast
        // path); otherwise still run it through so %% is unescaped
        bool hasDirective = false;
        for (const char* p = fmt; p && *p; p++) {
            if (*p == '%') {
                hasDirective = true;
                break;
            }
        }
        if (!hasDirective) {
            return StrDup(a, Str(fmt));
        }
    }

    Fmt f;
    // format directly into the caller's arena so there are no temp-allocator /
    // heap allocations at all (matters for the crash handler's pre-allocated
    // arena). TakeStr() then returns that arena buffer without a second copy.
    f.res.a = a;
    bool ok = ParseFormat(f, fmt);
    if (!ok) {
        return {};
    }
    ok = f.Eval(args, nArgs);
    if (!ok) {
        return {};
    }
    return f.res.TakeStr();
}

TempStr FormatTempArgs(const char* fmt, const FmtArg** args, int nArgs) {
    return FormatArgs(GetTempArena(), fmt, args, nArgs);
}

#include <locale.h>

#if defined(_MSC_VER)
static _locale_t GetUtf8FormatLocale() {
    // wrapped in a struct so the locale is freed at exit (keeps leak
    // detectors quiet); after the destructor runs, callers see nullptr
    // and fall back to plain vsnprintf
    struct Locale {
        _locale_t loc = _create_locale(LC_ALL, ".UTF-8");
        ~Locale() {
            if (loc) {
                _free_locale(loc);
                loc = nullptr;
            }
        }
    };
    static Locale l;
    return l.loc;
}
#endif

// The format string is a plain const char* because this is a thin wrapper
// around vsnprintf and is almost always called with a string literal.
int VsnprintfUtf8(Str buf, const char* fmt, va_list args) {
#if defined(_MSC_VER)
    _locale_t loc = GetUtf8FormatLocale();
    if (loc) {
        return _vsnprintf_l(buf.s, (size_t)buf.len, fmt, loc, args);
    }
#endif
    return vsnprintf(buf.s, (size_t)buf.len, fmt, args);
}
