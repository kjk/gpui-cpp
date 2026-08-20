/* The theme knobs the story's Appearance menu writes — crates/story's
 * FontSizeSelector, which sets `Theme::global_mut(cx).font_size`, `.radius`
 * and `Theme::set_scrollbar_mode`. Rust keeps a Theme per app as a Global;
 * these are process-wide statics, so what is worth pinning is the rules
 * rather than the storage. */

#include "Test.h"

// on_select_radius: radius_lg is two more than the radius, except at zero,
// where a large radius is no radius either.
static void TheLargeRadiusFollowsTheSmallOne() {
    ThemeSetRadius(8);
    utassertnear(ThemeLight().radius, 8.f);
    utassertnear(ThemeLight().radiusLg, 10.f);
    // Both palettes move together: the mode can be switched under them.
    utassertnear(ThemeDark().radius, 8.f);
    utassertnear(ThemeDark().radiusLg, 10.f);

    ThemeSetRadius(0);
    utassertnear(ThemeLight().radius, 0.f);
    utassertnear(ThemeLight().radiusLg, 0.f);

    // theme/mod.rs: 6 and 8 are what a theme starts with.
    ThemeSetRadius(6);
    utassertnear(ThemeLight().radius, 6.f);
    utassertnear(ThemeLight().radiusLg, 8.f);
}

// on_select_font: the root size every element inherits from. A size of zero
// is not one, so it answers with the default rather than collapsing the text.
static void TheFontSizeIsTheOneEverythingIsMeasuredAgainst() {
    utassertnear(ThemeFontSize(), 16.f);
    ThemeSetFontSize(18);
    utassertnear(ThemeFontSize(), 18.f);
    ThemeSetFontSize(0);
    utassertnear(ThemeFontSize(), 16.f);
}

// Theme::set_scrollbar_mode: the default an element that names none gets.
static void TheScrollbarModeIsTheThemesUntilAnElementSaysOtherwise() {
    utassert(ScrollbarModeNow() == ScrollbarMode::Always);
    ScrollbarModeSet(ScrollbarMode::Hover);
    utassert(ScrollbarModeNow() == ScrollbarMode::Hover);
    ScrollbarModeSet(ScrollbarMode::Always);
}

void TestThemeSettings() {
    TestSuite("theme settings");
    TheLargeRadiusFollowsTheSmallOne();
    TheFontSizeIsTheOneEverythingIsMeasuredAgainst();
    TheScrollbarModeIsTheThemesUntilAnElementSaysOtherwise();
}
