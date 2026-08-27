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
    utassert(base::StrEq(Fmt(k, buf, 64), kExpected[ExpectedCtrlP]));

    k = Keystroke{};
    k.key = StrL("p");
    k.ctrl = true;
    k.alt = true;
    k.shift = true;
    k.platform = true;
    // Ctrl, then Alt, then Shift, then the platform key — Rust's order,
    // spelled as symbols with no separators on macOS.
    utassert(base::StrEq(Fmt(k, buf, 64), kExpected[ExpectedAllModifiers]));

    k = Keystroke{};
    k.key = StrL("t");
    k.shift = true;
    k.platform = true;
    utassert(base::StrEq(Fmt(k, buf, 64), kExpected[ExpectedShiftPlatformT]));
}

static void ANamedKeyKeepsItsName() {
    char buf[64];
    Keystroke k;
    k.key = StrL("escape");
    utassert(base::StrEq(Fmt(k, buf, 64), kExpected[ExpectedEscape]));
    k.key = StrL("backspace");
    utassert(base::StrEq(Fmt(k, buf, 64), kExpected[ExpectedBackspace]));
    k.key = StrL("pagedown");
    utassert(base::StrEq(Fmt(k, buf, 64), StrL("Page Down")));
    k.key = StrL("pageup");
    utassert(base::StrEq(Fmt(k, buf, 64), StrL("Page Up")));
    k.key = StrL("left");
    utassert(base::StrEq(Fmt(k, buf, 64), kExpected[ExpectedLeft]));
    k.key = StrL("enter");
    utassert(base::StrEq(Fmt(k, buf, 64), kExpected[ExpectedEnter]));
}

static void AnythingElseIsCapitalised() {
    char buf[64];
    Keystroke k;
    // One character is upper-cased...
    k.key = StrL("c");
    utassert(base::StrEq(Fmt(k, buf, 64), StrL("C")));
    // ...and a symbol is left as it is.
    k.key = StrL("/");
    utassert(base::StrEq(Fmt(k, buf, 64), StrL("/")));
    k.key = StrL("-");
    utassert(base::StrEq(Fmt(k, buf, 64), StrL("-")));
    // A longer name keeps its spelling with the first letter raised, which is
    // how the keys with no table entry of their own read.
    k.key = StrL("home");
    utassert(base::StrEq(Fmt(k, buf, 64), StrL("Home")));
    k.key = StrL("f12");
    utassert(base::StrEq(Fmt(k, buf, 64), StrL("F12")));
    k.key = StrL("space");
    utassert(base::StrEq(Fmt(k, buf, 64), StrL("Space")));
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
    utassert(base::StrEq(Str(buf, n), expected));
}

// Kbd::binding_for_action_in: the shortcut a row shows is looked up in the
// keymap rather than typed by the caller, so it cannot drift from what is
// actually bound. The input's chords are the ones a menu shows most.
static void AShortcutComesFromTheBinding() {
    InputInitKeys();
    Keystroke k;
    // The field's actions live in its own key context, and asking without one
    // finds nothing — which is the same rule the matcher applies.
    utassert(!KeystrokeForAction(input::Copy(), nullptr, &k));
    utassert(KeystrokeForAction(input::Copy(), "Input", &k));
    utassert(base::StrEq(k.key, StrL("c")));
#if GPUI_OS_MAC
    utassert(k.platform && !k.ctrl);
#else
    utassert(k.ctrl && !k.platform);
#endif

    // And it spells out the way this platform spells it.
    char buf[32];
    int n = KbdFormat(k, buf, 32);
    utassert(n > 0);
#if GPUI_OS_MAC
    utassert(base::StrEq(Str(buf, n), StrL("\u2318C")));
#else
    utassert(base::StrEq(Str(buf, n), StrL("Ctrl+C")));
#endif

    // An action nothing binds has no shortcut to show, which is a row with
    // nothing on its right rather than a blank box.
    utassert(!KeystrokeForAction(ActionOf(StrL("t::NoSuchThing")), "Input",
                                 &k));
}

void TestKbd() {
    TestSuite("kbd");
    AShortcutComesFromTheBinding();
    TheModifiersComeFirstInPlatformOrder();
    ANamedKeyKeepsItsName();
    AnythingElseIsCapitalised();
    ABufferTooSmallStillEndsTheString();
}
