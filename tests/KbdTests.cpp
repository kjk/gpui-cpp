/* Ported from crates/ui/src/kbd.rs.
 *
 * `Kbd::format` is the platform's spelling of a keystroke: the modifiers in
 * the order that platform lists them, joined the way it joins them, then the
 * key — named where it has a name, capitalised where it does not. The pinned
 * Rust test has separate macOS and non-macOS answers; keep both here. */

#include "Test.h"

using namespace gpui::component;

enum ExpectedKbd {
    ExpectedCtrlP,
    ExpectedAllModifiers,
    ExpectedShiftPlatformT,
    ExpectedEscape,
    ExpectedBackspace,
    ExpectedLeft,
    ExpectedEnter,
    ExpectedShortBuffer,
};

#if GPUI_OS_MAC
static const char* const kExpected[] = {
    "⌃P", "⌃⌥⇧⌘P", "⇧⌘T", "⎋", "⌫", "←", "⏎", "⌃",
};
static const int kShortBufferCap = 4;
#else
static const char* const kExpected[] = {
    "Ctrl+P",      "Ctrl+Alt+Shift+Win+P",
    "Shift+Win+T", "Esc",
    "Backspace",   "Left",
    "Enter",       "Ctrl+",
};
static const int kShortBufferCap = 6;
#endif

static Str Fmt(Keystroke k, char* buf, int cap) {
    int n = KbdFormat(k, buf, cap);
    return Str(buf, n);
}

static void TheModifiersComeFirstInPlatformOrder() {
    char buf[64];
    Keystroke k;
    k.key = StrL("p");
    k.ctrl = true;
    utassert(StrSame(Fmt(k, buf, 64), Str(kExpected[ExpectedCtrlP])));

    k = Keystroke{};
    k.key = StrL("p");
    k.ctrl = true;
    k.alt = true;
    k.shift = true;
    k.platform = true;
    // Ctrl, then Alt, then Shift, then the platform key — Rust's order,
    // spelled as symbols with no separators on macOS.
    utassert(StrSame(Fmt(k, buf, 64), Str(kExpected[ExpectedAllModifiers])));

    k = Keystroke{};
    k.key = StrL("t");
    k.shift = true;
    k.platform = true;
    utassert(StrSame(Fmt(k, buf, 64), Str(kExpected[ExpectedShiftPlatformT])));
}

static void ANamedKeyKeepsItsName() {
    char buf[64];
    Keystroke k;
    k.key = StrL("escape");
    utassert(StrSame(Fmt(k, buf, 64), Str(kExpected[ExpectedEscape])));
    k.key = StrL("backspace");
    utassert(StrSame(Fmt(k, buf, 64), Str(kExpected[ExpectedBackspace])));
    k.key = StrL("pagedown");
    utassert(StrSame(Fmt(k, buf, 64), StrL("Page Down")));
    k.key = StrL("pageup");
    utassert(StrSame(Fmt(k, buf, 64), StrL("Page Up")));
    k.key = StrL("left");
    utassert(StrSame(Fmt(k, buf, 64), Str(kExpected[ExpectedLeft])));
    k.key = StrL("enter");
    utassert(StrSame(Fmt(k, buf, 64), Str(kExpected[ExpectedEnter])));
}

static void AnythingElseIsCapitalised() {
    char buf[64];
    Keystroke k;
    // One character is upper-cased...
    k.key = StrL("c");
    utassert(StrSame(Fmt(k, buf, 64), StrL("C")));
    // ...and a symbol is left as it is.
    k.key = StrL("/");
    utassert(StrSame(Fmt(k, buf, 64), StrL("/")));
    k.key = StrL("-");
    utassert(StrSame(Fmt(k, buf, 64), StrL("-")));
    // A longer name keeps its spelling with the first letter raised, which is
    // how the keys with no table entry of their own read.
    k.key = StrL("home");
    utassert(StrSame(Fmt(k, buf, 64), StrL("Home")));
    k.key = StrL("f12");
    utassert(StrSame(Fmt(k, buf, 64), StrL("F12")));
    k.key = StrL("space");
    utassert(StrSame(Fmt(k, buf, 64), StrL("Space")));
}

static void ABufferTooSmallStillEndsTheString() {
    char buf[6];
    Keystroke k;
    k.ctrl = true;
    k.key = StrL("c");
    int n = KbdFormat(k, buf, kShortBufferCap);
    Str expected = Str(kExpected[ExpectedShortBuffer]);
    utassert(n == expected.len);
    utassert(buf[n] == 0);
    utassert(StrSame(Str(buf, n), expected));
}

void TestKbd() {
    TestSuite("kbd");
    TheModifiersComeFirstInPlatformOrder();
    ANamedKeyKeepsItsName();
    AnythingElseIsCapitalised();
    ABufferTooSmallStillEndsTheString();
}
