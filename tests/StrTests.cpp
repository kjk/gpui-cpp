/* The slice equality contract in base.h. The length rejection is inline so
   callers avoid entering the case-insensitive comparison for the common
   mismatch, including two slices that happen to share their first byte. */

#include "Test.h"

static void CaseInsensitiveEqualityRejectsLengthFirst() {
    char text[] = "Alpha";
    Str whole(text, 5);
    Str prefix(text, 4);

    utassert(base::StrEqI(whole, "aLPHA"));
    utassert(!base::StrEqI(prefix, whole));
    utassert(!base::StrEqI(whole, "alpha!"));
}

static void CaseInsensitiveEqualityKeepsEmptySliceSemantics() {
    char empty[] = "";
    utassert(base::StrEqI(Str{}, Str{}));
    utassert(base::StrEqI(Str(empty), ""));
    utassert(base::StrEqI(Str{}, ""));
    utassert(!base::StrEqI(Str{}, "x"));
}

static void CaseInsensitivePrefixUsesBothOverloads() {
    Str text = StrL("Alpha");
    utassert(base::StrStartsWithI(text, "aL"));
    utassert(base::StrStartsWithI(text, StrL("ALP")));
    utassert(!base::StrStartsWithI(text, "Alphas"));
    utassert(base::StrStartsWithI(Str{}, ""));
    utassert(!base::StrStartsWithI(Str{}, "a"));
}

static void ReplaceAllReplacesNonOverlappingMatches() {
    utassert(base::StrEq(base::StrReplaceAll(StrL("aaaa"), StrL("aa"),
                                             StrL("b")),
                         "bb"));
    utassert(base::StrEq(base::StrReplaceAll(StrL("one two one"),
                                             StrL("one"), StrL("three")),
                         "three two three"));
}

static void ReplaceAllHandlesEmptyAndMissingMatches() {
    Str value = StrL("hello");
    Str unchanged = base::StrReplaceAll(value, StrL(""), StrL("x"));
    utassert(unchanged.s == value.s && unchanged.len == value.len);
    unchanged = base::StrReplaceAll(value, StrL("z"), StrL("x"));
    utassert(unchanged.s == value.s && unchanged.len == value.len);
    utassert(base::StrEq(base::StrReplaceAll(value, StrL("l"), StrL("")),
                         "heo"));
}

static void PrefixSuffixAndFindHelpersHandleBoundaries() {
    Str text = StrL("Alpha beta");
    utassert(base::StrStartsWith(text, "Alpha"));
    utassert(!base::StrStartsWith(text, "alpha"));
    utassert(base::StrStartsWith(text, ""));
    utassert(base::StrEndsWith(text, "beta"));
    utassert(!base::StrEndsWith(text, "Beta"));
    utassert(base::StrEndsWithI(text, "BETA"));
    utassert(base::StrEndsWith(text, ""));
    utassert(base::StrFind(text, StrL("beta")) == 6);
    utassert(base::StrFindI(text, StrL("BETA")) == 6);
    utassert(base::StrFind(text, StrL("gamma")) == -1);
    utassert(base::StrFind(text, StrL("")) == -1);
    utassert(base::StrContains(text, StrL("Alpha")));
    utassert(base::StrContainsI(text, StrL("BETA")));
    utassert(!base::StrContains(text, StrL("alpha")));
}

static void TrimAsciiReturnsASlice() {
    char text[] = "\f \tHello world\r\n";
    Str trimmed = base::StrTrimAscii(Str(text, (int)sizeof(text) - 1));
    utassert(base::StrEq(trimmed, "Hello world"));
    utassert(trimmed.s == text + 3);
    utassert(base::StrEq(base::StrTrimAscii(StrL(" \t\r\n")), ""));
}

static void BuilderBorrowsThenGrowsLikeAVec() {
    char scratch[5] = {};
    StrBuilder b;
    StrBuilderUseExternalBuffer(b, Str(scratch, (int)sizeof(scratch)));
    utassert(b.cap == -4); // the fifth byte is held back for the NUL
    utassert(b.Append(StrL("four")));
    utassert(b.els == scratch && scratch[4] == 0);

    // Taking borrowed storage copies the result and keeps the scratch bound.
    Str four = b.TakeStr();
    utassert(base::StrEq(four, "four"));
    utassert(four.s != scratch && b.els == scratch && b.len == 0);
    StrFree(four);

    // The next append past the lent capacity allocates and copies. The caller's
    // buffer remains untouched, and the heap block can be handed over.
    utassert(b.Append(StrL("abcde")));
    utassert(b.els != scratch && b.cap > 0);
    utassert(scratch[0] == 0);
    Str five = b.TakeStr();
    utassert(base::StrEq(five, "abcde"));
    utassert(b.els == nullptr && b.cap == 0 && b.len == 0);
    StrFree(five);
}

static void BuilderArenaStorageStaysWithTheArena() {
    Arena* a = ArenaNew();
    StrBuilder b;
    utassert(StrBuilderReserve(a, b, 4));
    char* first = b.els;
    utassert(first && b.cap < 0);
    utassert(StrBuilderAppend(a, b, StrL("a string longer than reserve")));
    utassert(b.cap < 0 && b.els != first);
    char* storage = b.els;

    Str result = StrBuilderTakeStr(a, b);
    utassert(base::StrEq(result, "a string longer than reserve"));
    utassert(result.s != storage);
    utassert(b.els == storage && b.cap < 0 && b.len == 0);
    // Destroying b must not try to free either arena allocation.
    ArenaDelete(a);
}

static void BuilderRemovalKeepsTheTerminator() {
    StrBuilder b;
    utassert(b.Append(StrL("abcd")));
    utassert(b.LastChar() == 'd');
    utassert(b.RemoveAt(1, 2) == 'b');
    utassert(base::StrEq(Str(b.els, b.len), "ad"));
    utassert(b.els[b.len] == 0);
    utassert(b.RemoveLast() == 'd');
    utassert(b.LastChar() == 'a' && b.els[b.len] == 0);
    utassert(b.RemoveLast() == 'a');
    utassert(b.RemoveLast() == 0 && b.LastChar() == 0);
}

void TestStr() {
    TestSuite("str");
    CaseInsensitiveEqualityRejectsLengthFirst();
    CaseInsensitiveEqualityKeepsEmptySliceSemantics();
    CaseInsensitivePrefixUsesBothOverloads();
    ReplaceAllReplacesNonOverlappingMatches();
    ReplaceAllHandlesEmptyAndMissingMatches();
    PrefixSuffixAndFindHelpersHandleBoundaries();
    TrimAsciiReturnsASlice();
    BuilderBorrowsThenGrowsLikeAVec();
    BuilderArenaStorageStaysWithTheArena();
    BuilderRemovalKeepsTheTerminator();
}
