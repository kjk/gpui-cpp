/* Not a port — `fmt()` is this tree's printf, inherited from SumatraPDF's
   StrFormat rather than from gpui-component.

   The doc block above `Fmt` in base.cpp is what these check: the directives
   it takes, that flags and width and precision reach snprintf unchanged,
   that a length modifier is normalized to a 32- or 64-bit width, and that a
   format which does not hold up answers an empty Str rather than a partial
   one. */

#include "Test.h"

#include <string.h>

static void TheOrdinaryDirectives() {
    utassert(base::StrEq(fmt("%d", 42), StrL("42")));
    utassert(base::StrEq(fmt("%i", -7), StrL("-7")));
    utassert(base::StrEq(fmt("%u", 7), StrL("7")));
    utassert(base::StrEq(fmt("%o", 8), StrL("10")));
    utassert(base::StrEq(fmt("%x", 255), StrL("ff")));
    utassert(base::StrEq(fmt("%X", 255), StrL("FF")));
    utassert(base::StrEq(fmt("%c", 'z'), StrL("z")));
    utassert(base::StrEq(fmt("%f", 1.5), StrL("1.500000")));
    utassert(base::StrEq(fmt("%g", 0.5), StrL("0.5")));
    utassert(base::StrEq(fmt("%s", StrL("hi")), StrL("hi")));
    utassert(base::StrEq(fmt("a%db%dc", 1, 2), StrL("a1b2c")));
    utassert(base::StrEq(fmt("no directives"), StrL("no directives")));

    // %% is the only escape, and a bare '{' is text — which is what lets a
    // registry path or a CSS template through unformatted.
    utassert(base::StrEq(fmt("100%%"), StrL("100%")));
    utassert(base::StrEq(fmt("a{b}c"), StrL("a{b}c")));
    utassert(base::StrEq(fmt("{ %d }", 1), StrL("{ 1 }")));
}

static void FlagsWidthAndPrecision() {
    utassert(base::StrEq(fmt("%5d|", 42), StrL("   42|")));
    utassert(base::StrEq(fmt("%-5d|", 42), StrL("42   |")));
    utassert(base::StrEq(fmt("%05d", 42), StrL("00042")));
    utassert(base::StrEq(fmt("%+d", 42), StrL("+42")));
    utassert(base::StrEq(fmt("%#x", 255), StrL("0xff")));
    utassert(base::StrEq(fmt("%.2f", 1.0), StrL("1.00")));
    utassert(base::StrEq(fmt("%8.3f|", 2.5), StrL("   2.500|")));

    // %s does its own padding and truncation, because a Str need not be
    // NUL-terminated and snprintf would walk past the end looking for one.
    utassert(base::StrEq(fmt("%6s|", StrL("hi")), StrL("    hi|")));
    utassert(base::StrEq(fmt("%-6s|", StrL("hi")), StrL("hi    |")));
    utassert(base::StrEq(fmt("%.2s", StrL("hello")), StrL("he")));
    utassert(base::StrEq(fmt("%s", Str(StrL("hello").s, 2)), StrL("he")));
}

static void TheLengthModifierIsNormalized() {
    int64_t big = 5000000000LL;
    utassert(base::StrEq(fmt("%lld", big), StrL("5000000000")));
    utassert(base::StrEq(fmt("%I64d", big), StrL("5000000000")));
    utassert(base::StrEq(fmt("%jd", big), StrL("5000000000")));
    utassert(base::StrEq(fmt("%zu", (size_t)12), StrL("12")));
    utassert(base::StrEq(fmt("%ld", 12), StrL("12")));
    utassert(base::StrEq(fmt("%hd", 12), StrL("12")));
}

static void TheAnyDirectives() {
    // %v, %{} and %{n} format by the argument's own type.
    utassert(base::StrEq(fmt("%v", 42), StrL("42")));
    utassert(base::StrEq(fmt("%{}", 42), StrL("42")));
    utassert(base::StrEq(fmt("%{0}", 42), StrL("42")));
    utassert(base::StrEq(fmt("%{}", -7), StrL("-7")));
    utassert(base::StrEq(fmt("%{}", 1.5f), StrL("1.5")));
    utassert(base::StrEq(fmt("%{}", 2.25), StrL("2.25")));
    utassert(base::StrEq(fmt("%{}", StrL("hi")), StrL("hi")));
    utassert(base::StrEq(fmt("%{}", 'z'), StrL("z")));

    // Positional, in any order, and the same argument twice.
    utassert(base::StrEq(fmt("%{1}-%{0}", 1, 2), StrL("2-1")));
    utassert(base::StrEq(fmt("%{0}%{0}", 5), StrL("55")));
    utassert(base::StrEq(fmt("a%{0}b%{1}c", 1, 2), StrL("a1b2c")));

    // %{n} does not move the counter a plain directive walks, so the two
    // together are easy to mis-count — which is what the doc block warns of,
    // written down as a fact rather than as advice.
    utassert(base::StrEq(fmt("%d %{0}", 7), StrL("7 7")));
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
    utassert(base::StrEq(fmt("%c", 122), StrL("z")));
    utassert(base::StrEq(fmt("%d", 'a'), StrL("97")));
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
    utassert(base::StrEq(kept, StrL("3 + 4")));
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
