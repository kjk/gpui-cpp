/* Not a port — `fmt()` is this tree's printf, inherited from SumatraPDF's
   StrFormat rather than from gpui-component.

   The doc block above `Fmt` in base.cpp is what these check: the directives
   it takes, that flags and width and precision reach snprintf unchanged,
   that a length modifier is normalized to a 32- or 64-bit width, and that a
   format which does not hold up answers an empty Str rather than a partial
   one. */

#include "Test.h"

#include <string.h>

static bool Is(Str got, const char* want) {
    int n = (int)strlen(want);
    return got.len == n && memcmp(got.s, want, (size_t)n) == 0;
}

static void TheOrdinaryDirectives() {
    utassert(Is(fmt("%d", 42), "42"));
    utassert(Is(fmt("%i", -7), "-7"));
    utassert(Is(fmt("%u", 7), "7"));
    utassert(Is(fmt("%o", 8), "10"));
    utassert(Is(fmt("%x", 255), "ff"));
    utassert(Is(fmt("%X", 255), "FF"));
    utassert(Is(fmt("%c", 'z'), "z"));
    utassert(Is(fmt("%f", 1.5), "1.500000"));
    utassert(Is(fmt("%g", 0.5), "0.5"));
    utassert(Is(fmt("%s", StrL("hi")), "hi"));
    utassert(Is(fmt("a%db%dc", 1, 2), "a1b2c"));
    utassert(Is(fmt("no directives"), "no directives"));

    // %% is the only escape, and a bare '{' is text — which is what lets a
    // registry path or a CSS template through unformatted.
    utassert(Is(fmt("100%%"), "100%"));
    utassert(Is(fmt("a{b}c"), "a{b}c"));
    utassert(Is(fmt("{ %d }", 1), "{ 1 }"));
}

static void FlagsWidthAndPrecision() {
    utassert(Is(fmt("%5d|", 42), "   42|"));
    utassert(Is(fmt("%-5d|", 42), "42   |"));
    utassert(Is(fmt("%05d", 42), "00042"));
    utassert(Is(fmt("%+d", 42), "+42"));
    utassert(Is(fmt("%#x", 255), "0xff"));
    utassert(Is(fmt("%.2f", 1.0), "1.00"));
    utassert(Is(fmt("%8.3f|", 2.5), "   2.500|"));

    // %s does its own padding and truncation, because a Str need not be
    // NUL-terminated and snprintf would walk past the end looking for one.
    utassert(Is(fmt("%6s|", StrL("hi")), "    hi|"));
    utassert(Is(fmt("%-6s|", StrL("hi")), "hi    |"));
    utassert(Is(fmt("%.2s", StrL("hello")), "he"));
    utassert(Is(fmt("%s", Str(StrL("hello").s, 2)), "he"));
}

static void TheLengthModifierIsNormalized() {
    int64_t big = 5000000000LL;
    utassert(Is(fmt("%lld", big), "5000000000"));
    utassert(Is(fmt("%I64d", big), "5000000000"));
    utassert(Is(fmt("%jd", big), "5000000000"));
    utassert(Is(fmt("%zu", (size_t)12), "12"));
    utassert(Is(fmt("%ld", 12), "12"));
    utassert(Is(fmt("%hd", 12), "12"));
}

static void TheAnyDirectives() {
    // %v, %{} and %{n} format by the argument's own type.
    utassert(Is(fmt("%v", 42), "42"));
    utassert(Is(fmt("%{}", 42), "42"));
    utassert(Is(fmt("%{0}", 42), "42"));
    utassert(Is(fmt("%{}", -7), "-7"));
    utassert(Is(fmt("%{}", 1.5f), "1.5"));
    utassert(Is(fmt("%{}", 2.25), "2.25"));
    utassert(Is(fmt("%{}", StrL("hi")), "hi"));
    utassert(Is(fmt("%{}", 'z'), "z"));

    // Positional, in any order, and the same argument twice.
    utassert(Is(fmt("%{1}-%{0}", 1, 2), "2-1"));
    utassert(Is(fmt("%{0}%{0}", 5), "55"));
    utassert(Is(fmt("a%{0}b%{1}c", 1, 2), "a1b2c"));

    // %{n} does not move the counter a plain directive walks, so the two
    // together are easy to mis-count — which is what the doc block warns of,
    // written down as a fact rather than as advice.
    utassert(Is(fmt("%d %{0}", 7), "7 7"));
}

static void AFormatThatDoesNotHoldUpAnswersNothing() {
    // A type that does not match its directive.
    utassert(fmt("%d", StrL("no")).len == 0);
    utassert(fmt("%s", 1).len == 0);
    utassert(fmt("%f", 1).len == 0);

    // An argument that was not passed.
    utassert(fmt("%{1}", 1).len == 0);
    utassert(fmt("%d %d", 1).len == 0);

    // A positional format with a hole in it: %{1} was never named, so the
    // argument it would have checked cannot be checked at all.
    utassert(fmt("%{0} %{2}", 1, 2, 3).len == 0);

    // A '{' that never closes, and the '$' spelling that reads like it
    // should work and does not.
    utassert(fmt("%{0", 1).len == 0);
    utassert(fmt("%{$0}", 1).len == 0);

    // An integer directive takes anything integer-like, which is printf's
    // own leniency rather than an accident.
    utassert(Is(fmt("%c", 122), "z"));
    utassert(Is(fmt("%d", 'a'), "97"));
}

static void OutputLongerThanTheScratchBuffer() {
    // Every conversion but %s goes through a 256-byte buffer in Fmt. A
    // conversion that overruns it truncates rather than overruns, and what
    // is appended is what landed — 255 characters and the terminator.
    Str s = fmt("%500d", 1);
    utassert(s.len == 255);
    utassert(s.s[0] == ' ' && s.s[254] == ' ');

    // The truncation is of one conversion, not of the format: what follows
    // it is still appended.
    Str after = fmt("%500d|", 1);
    utassert(after.len == 256 && after.s[255] == '|');

    // And what outlives the frame is a copy in an arena of the caller's:
    // fmt() answers temp-arena memory and nothing else.
    Arena* a = ArenaNew();
    Str kept = StrDup(a, fmt("%d + %d", 3, 4));
    utassert(Is(kept, "3 + 4"));
    ArenaDelete(a);
}

void TestFmt() {
    TestSuite("fmt");
    TheOrdinaryDirectives();
    FlagsWidthAndPrecision();
    TheLengthModifierIsNormalized();
    TheAnyDirectives();
    AFormatThatDoesNotHoldUpAnswersNothing();
    OutputLongerThanTheScratchBuffer();
}
