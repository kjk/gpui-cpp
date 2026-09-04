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

static void ComparisonUsesBytesThenLength() {
    utassert(base::StrCmp(StrL("Alpha"), StrL("Alpha")) == 0);
    utassert(base::StrCmp(StrL("Alpha"), StrL("Beta")) < 0);
    utassert(base::StrCmp(StrL("Beta"), StrL("Alpha")) > 0);
    utassert(base::StrCmp(StrL("Alpha"), StrL("Alphabet")) < 0);
    utassert(base::StrCmp(StrL("Alphabet"), StrL("Alpha")) > 0);
    utassert(base::StrCmp(Str{}, Str{}) == 0);
}

static void SequentialStringLookupsAvoidLengthPrepass() {
    static const char values[] = "Alpha\0beta\0longer value\0";
    utassert(base::SeqStrIndex(values, StrL("Alpha")) == 0);
    utassert(base::SeqStrIndex(values, StrL("beta")) == 1);
    utassert(base::SeqStrIndex(values, StrL("BETA")) == -1);
    utassert(base::SeqStrIndexIS(values, StrL("BETA")) == 1);
    utassert(base::SeqStrIndex(values, StrL("longer value")) == 2);
    utassert(base::SeqStrIndexIS(values, StrL("missing")) == -1);
    utassert(base::SeqStrContainsI(values, StrL("alpha")));
    utassert(base::SeqStrContainsI(values, StrL("BETA")));
    utassert(base::SeqStrContainsI(values, StrL("longer value")));
    utassert(!base::SeqStrContainsI(values, StrL("longer")));
    utassert(!base::SeqStrContainsI(values, StrL("missing")));
    utassert(!base::SeqStrContainsI(values, Str{}));
    utassert(!base::SeqStrContainsI(nullptr, StrL("alpha")));
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
    utassert(base::StrEq(
        base::StrReplaceAll(StrL("aaaa"), StrL("aa"), StrL("b")), StrL("bb")));
    utassert(base::StrEq(
        base::StrReplaceAll(StrL("one two one"), StrL("one"), StrL("three")),
        StrL("three two three")));
}

static void ReplaceAllHandlesEmptyAndMissingMatches() {
    Str value = StrL("hello");
    Str unchanged = base::StrReplaceAll(value, StrL(""), StrL("x"));
    utassert(unchanged.s == value.s && unchanged.len == value.len);
    unchanged = base::StrReplaceAll(value, StrL("z"), StrL("x"));
    utassert(unchanged.s == value.s && unchanged.len == value.len);
    utassert(base::StrEq(base::StrReplaceAll(value, StrL("l"), StrL("")),
                         StrL("heo")));
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
    utassert(base::StrEq(trimmed, StrL("Hello world")));
    utassert(trimmed.s == text + 3);
    utassert(base::StrEq(base::StrTrimAscii(StrL(" \t\r\n")), StrL("")));
}

static void BuilderBorrowsThenGrowsLikeAVec() {
    TempStr scratch = AllocStrTemp(4);
    StrBuilder b;
    StrBuilderUseExternalBuffer(b, Str(scratch.s, scratch.len + 1));
    utassert(b.cap == -4); // the fifth byte is held back for the NUL
    utassert(b.Append(StrL("four")));
    utassert(b.els == scratch.s && scratch.s[4] == 0);

    // Taking borrowed storage copies the result and keeps the scratch bound.
    Str four = b.TakeStr();
    utassert(base::StrEq(four, StrL("four")));
    utassert(four.s != scratch.s && b.els == scratch.s && b.len == 0);
    StrFree(four);

    // The next append past the lent capacity allocates and copies. The caller's
    // buffer remains untouched, and the heap block can be handed over.
    utassert(b.Append(StrL("abcde")));
    utassert(b.els != scratch.s && b.cap > 0);
    utassert(scratch.s[0] == 0);
    Str five = b.TakeStr();
    utassert(base::StrEq(five, StrL("abcde")));
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
    utassert(base::StrEq(result, StrL("a string longer than reserve")));
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
    utassert(base::StrEq(Str(b.els, b.len), StrL("ad")));
    utassert(b.els[b.len] == 0);
    utassert(b.RemoveLast() == 'd');
    utassert(b.LastChar() == 'a' && b.els[b.len] == 0);
    utassert(b.RemoveLast() == 'a');
    utassert(b.RemoveLast() == 0 && b.LastChar() == 0);
}

static void Dup2PutsBothStringsInOneBlock() {
    Str a, b;
    StrDup2(StrL("id"), StrL("label"), a, b);
    utassert(base::StrEq(a, StrL("id")));
    utassert(base::StrEq(b, StrL("label")));
    utassert(a.s && b.s == a.s + a.len + 1);
    utassert(a.s[a.len] == 0 && b.s[b.len] == 0);
    StrFree2(a);
}

static void Dup2TreatsNullAsEmptyInsideTheSameBlock() {
    Str a, b;
    StrDup2(Str{}, StrL("x"), a, b);
    utassert(a.len == 0 && a.s);
    utassert(base::StrEq(b, StrL("x")));
    utassert(b.s == a.s + 1);
    StrFree2(a);

    StrDup2(StrL("y"), Str{}, a, b);
    utassert(base::StrEq(a, StrL("y")));
    utassert(b.len == 0 && b.s == a.s + a.len + 1);
    StrFree2(a);
}

static void StartsWithAnyChecksFirstCharInSet() {
    Str s = StrL("+123");
    utassert(StrStartsWithAny(s, "+-"));
    utassert(StrStartsWithAny(s, "+"));
    utassert(!StrStartsWithAny(s, "-"));
    utassert(!StrStartsWithAny(s, "123"));

    Str minus = StrL("-456");
    utassert(StrStartsWithAny(minus, "+-"));
    utassert(!StrStartsWithAny(minus, "+"));
    utassert(StrStartsWithAny(minus, "-"));

    utassert(!StrStartsWithAny(Str{}, "+-"));
    utassert(!StrStartsWithAny(StrL(""), "+-"));
    utassert(!StrStartsWithAny(s, ""));
    utassert(!StrStartsWithAny(s, nullptr));
}

void TestStr() {
    TestSuite("str");
    CaseInsensitiveEqualityRejectsLengthFirst();
    CaseInsensitiveEqualityKeepsEmptySliceSemantics();
    ComparisonUsesBytesThenLength();
    SequentialStringLookupsAvoidLengthPrepass();
    CaseInsensitivePrefixUsesBothOverloads();
    StartsWithAnyChecksFirstCharInSet();
    ReplaceAllReplacesNonOverlappingMatches();
    ReplaceAllHandlesEmptyAndMissingMatches();
    PrefixSuffixAndFindHelpersHandleBoundaries();
    TrimAsciiReturnsASlice();
    BuilderBorrowsThenGrowsLikeAVec();
    BuilderArenaStorageStaysWithTheArena();
    BuilderRemovalKeepsTheTerminator();
    Dup2PutsBothStringsInOneBlock();
    Dup2TreatsNullAsEmptyInsideTheSameBlock();
}
