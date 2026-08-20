/* Ported from crates/ui/src/kbd.rs.
 *
 * `Kbd::format` is the platform's spelling of a keystroke: the modifiers in
 * the order that platform lists them, joined the way it joins them, then the
 * key — named where it has a name, capitalised where it does not. These are
 * the answers on everything but macOS, which is where the tests run. */

#include "Test.h"

using namespace gpui::component;

static Str Fmt(Keystroke k, char* buf, int cap) {
    int n = KbdFormat(k, buf, cap);
    return Str(buf, n);
}

static void TheModifiersComeFirstInPlatformOrder() {
    char buf[64];
    Keystroke k;
    k.key = StrL("p");
    k.ctrl = true;
    utassert(StrSame(Fmt(k, buf, 64), StrL("Ctrl+P")));

    k = Keystroke{};
    k.key = StrL("p");
    k.ctrl = true;
    k.alt = true;
    k.shift = true;
    k.platform = true;
    // Ctrl, then Alt, then Shift, then the platform key — Rust's order.
    utassert(StrSame(Fmt(k, buf, 64), StrL("Ctrl+Alt+Shift+Win+P")));

    k = Keystroke{};
    k.key = StrL("t");
    k.shift = true;
    k.platform = true;
    utassert(StrSame(Fmt(k, buf, 64), StrL("Shift+Win+T")));
}

static void ANamedKeyKeepsItsName() {
    char buf[64];
    Keystroke k;
    k.key = StrL("escape");
    utassert(StrSame(Fmt(k, buf, 64), StrL("Esc")));
    k.key = StrL("backspace");
    utassert(StrSame(Fmt(k, buf, 64), StrL("Backspace")));
    k.key = StrL("pagedown");
    utassert(StrSame(Fmt(k, buf, 64), StrL("Page Down")));
    k.key = StrL("pageup");
    utassert(StrSame(Fmt(k, buf, 64), StrL("Page Up")));
    k.key = StrL("left");
    utassert(StrSame(Fmt(k, buf, 64), StrL("Left")));
    k.key = StrL("enter");
    utassert(StrSame(Fmt(k, buf, 64), StrL("Enter")));
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
    int n = KbdFormat(k, buf, (int)sizeof(buf));
    utassert(n == 5);
    utassert(buf[n] == 0);
    utassert(StrSame(Str(buf, n), StrL("Ctrl+")));
}

void TestKbd() {
    TestSuite("kbd");
    TheModifiersComeFirstInPlatformOrder();
    ANamedKeyKeepsItsName();
    AnythingElseIsCapitalised();
    ABufferTooSmallStillEndsTheString();
}
