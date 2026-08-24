/* Ported from crates/ui/src/title_bar.rs, mod tests.
 *
 * `default_title_bar_background` mixes the two colours' channels by hand
 * rather than through `Colorize::mix`, so what it asks for is RgbaMix — the
 * plain channel blend, not the HSL arc RgbaMixHsl walks (ColorTests has that
 * one). This is the assertion upstream makes about it. */

#include "Test.h"

static void DefaultTitleBarBackground() {
    // Black title bar over a white background, 55% of the way to the title
    // bar, is the 0.45 grey Rust asserts: 0.45 * 255 = 114.75.
    Rgba mixed = RgbaMix(Rgb(0, 0, 0), Rgb(255, 255, 255), 0.55f);
    utassert(mixed.r == 115);
    utassert(mixed.g == 115);
    utassert(mixed.b == 115);
    utassert(mixed.a == 255);

    // The ends of the blend are the inputs themselves.
    Rgba all = RgbaMix(Rgb(0x0a, 0x0a, 0x0a), Rgb(0xfa, 0xfa, 0xfa), 1.f);
    utassert(all.r == 0x0a && all.g == 0x0a && all.b == 0x0a);
    Rgba none = RgbaMix(Rgb(0x0a, 0x0a, 0x0a), Rgb(0xfa, 0xfa, 0xfa), 0.f);
    utassert(none.r == 0xfa && none.g == 0xfa && none.b == 0xfa);
}

void TestTitleBar() {
    TestSuite("title_bar");
    DefaultTitleBarBackground();
}
