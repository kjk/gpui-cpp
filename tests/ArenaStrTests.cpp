/* Not a port — `ArenaStr` is this tree's, not markdown-rs's.

   A string packed into one 64-bit word: the length in the upper half, the
   offset into the arena's position space in the lower. Half what a `Str`
   costs, which is what an mdast Node holding eight of them is for. */

#include "Test.h"

static void WhatGoesInComesOut() {
    Arena* a = ArenaNew();
    ArenaStr s = ArenaStrDup(a, StrL("hello"));
    utassert(ArenaStrIsSet(s));
    utassert(ArenaStrLen(s) == 5);
    utassert(StrSame(ArenaStrGet(a, s), StrL("hello")));

    // NUL-terminated the way StrDup's is, so a caller that needs a C string
    // has one without copying again.
    Str got = ArenaStrGet(a, s);
    utassert(got.s[got.len] == 0);

    // Several, none of them disturbing the others.
    ArenaStr one = ArenaStrDup(a, StrL("one"));
    ArenaStr two = ArenaStrDup(a, StrL("two"));
    ArenaStr three = ArenaStrDup(a, StrL(""));
    utassert(StrSame(ArenaStrGet(a, one), StrL("one")));
    utassert(StrSame(ArenaStrGet(a, two), StrL("two")));
    // Empty is nothing stored at all, which is what a Str with a null s means.
    utassert(!ArenaStrIsSet(three));
    utassert(ArenaStrGet(a, three).s == nullptr);
    utassert(!ArenaStrIsSet(ArenaStrDup(a, Str{})));
    ArenaDelete(a);
}

// The offset is into the whole chain's position space, not into one block, so
// a string keeps its identity after the arena has grown onto another block.
static void ItSurvivesTheArenaChainingOn() {
    Arena* a = ArenaNew();
    ArenaStr first = ArenaStrDup(a, StrL("first"));
    uint64_t before = ArenaUsed(a);

    // Past the default reserve, which forces a new block.
    for (int i = 0; i < 4096; i++) {
        Alloc(a, 4096);
    }
    utassert(ArenaUsed(a) > before);

    ArenaStr last = ArenaStrDup(a, StrL("last"));
    // The early one is still readable, and so is the late one — which is the
    // whole point of the position space over a per-block offset.
    utassert(StrSame(ArenaStrGet(a, first), StrL("first")));
    utassert(StrSame(ArenaStrGet(a, last), StrL("last")));
    ArenaDelete(a);
}

// A slice of something the arena already holds does not need copying again.
static void ARefNamesWhatIsAlreadyThere() {
    Arena* a = ArenaNew();
    ArenaStr full = ArenaStrDup(a, StrL("hello world"));
    Str got = ArenaStrGet(a, full);

    ArenaStr world = ArenaStrRef(a, Str(got.s + 6, 5));
    utassert(ArenaStrIsSet(world));
    utassert(StrSame(ArenaStrGet(a, world), StrL("world")));
    // No copy: it names the same bytes.
    utassert(ArenaStrGet(a, world).s == got.s + 6);

    // A string that is not in this arena has no offset to name, and saying so
    // beats answering an offset into somebody else's bytes.
    utassert(!ArenaStrIsSet(ArenaStrRef(a, StrL("elsewhere"))));
    ArenaDelete(a);
}

static void TheWordIsLengthOverOffset() {
    Arena* a = ArenaNew();
    ArenaStr s = ArenaStrDup(a, StrL("abcd"));
    // Upper half the length, lower half the offset — which is what makes the
    // whole thing eight bytes.
    utassert((uint32_t)(s >> 32) == 4);
    utassert((uint32_t)s > 0);
    // The size is the whole point, so it is checked at compile time.
    static_assert(sizeof(ArenaStr) == 8, "ArenaStr is one word");
    static_assert(sizeof(Str) < 2 * sizeof(ArenaStr) + 1, "Str is two");
    ArenaDelete(a);
}

// Growing the newest string in an arena costs only the bytes appended. The
// check is that it costs that and not more, since the whole point is the
// copies it does not make.
static void AppendingToTheNewestCostsOnlyTheBytes() {
    Arena* a = ArenaNew();
    ArenaStr s = ArenaStrDup(a, StrL("one"));
    uint64_t after = ArenaUsed(a);

    s = ArenaStrAppend(a, s, StrL(" two"));
    utassert(StrSame(ArenaStrGet(a, s), StrL("one two")));
    // Four characters more, and nothing else: no second copy of "one".
    utassert(ArenaUsed(a) == after + 4);

    s = ArenaStrAppend(a, s, StrL(" three"));
    utassert(StrSame(ArenaStrGet(a, s), StrL("one two three")));
    utassert(ArenaUsed(a) == after + 4 + 6);
    utassert(ArenaStrLen(s) == 13);

    // Still NUL-terminated, which the in-place path has to keep true or the
    // next append would find the wrong end.
    Str got = ArenaStrGet(a, s);
    utassert(got.s[got.len] == 0);

    // Appending nothing is not an allocation.
    uint64_t before = ArenaUsed(a);
    s = ArenaStrAppend(a, s, Str{});
    s = ArenaStrAppend(a, s, StrL(""));
    utassert(ArenaUsed(a) == before);
    utassert(ArenaStrLen(s) == 13);

    // Appending to nothing is just storing it.
    ArenaStr fresh = ArenaStrAppend(a, kArenaStrNone, StrL("first"));
    utassert(StrSame(ArenaStrGet(a, fresh), StrL("first")));
    ArenaDelete(a);
}

// And when it is not the newest, it copies — which is what concatenating
// always did, so the answer is right either way.
static void AppendingToAnOlderStringCopies() {
    Arena* a = ArenaNew();
    ArenaStr first = ArenaStrDup(a, StrL("aa"));
    ArenaStr second = ArenaStrDup(a, StrL("bb"));

    // `first` is no longer at the end, so this cannot grow in place.
    first = ArenaStrAppend(a, first, StrL("cc"));
    utassert(StrSame(ArenaStrGet(a, first), StrL("aacc")));
    // The one behind it is untouched, which is the thing a wrong in-place
    // append would break.
    utassert(StrSame(ArenaStrGet(a, second), StrL("bb")));

    // And the copy is itself the newest now, so the next append is in place.
    uint64_t after = ArenaUsed(a);
    first = ArenaStrAppend(a, first, StrL("dd"));
    utassert(StrSame(ArenaStrGet(a, first), StrL("aaccdd")));
    utassert(ArenaUsed(a) == after + 2);
    ArenaDelete(a);
}

void TestArenaStr() {
    TestSuite("arena_str");
    AppendingToTheNewestCostsOnlyTheBytes();
    AppendingToAnOlderStringCopies();
    WhatGoesInComesOut();
    ItSurvivesTheArenaChainingOn();
    ARefNamesWhatIsAlreadyThere();
    TheWordIsLengthOverOffset();
}
