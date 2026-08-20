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

// Theme::focus_ring: off drops the ring outside the border and leaves the
// tinted border, which is the half that costs no room.
static void TheFocusRingIsOnUntilAnApplicationTurnsItOff() {
    utassert(ThemeFocusRing());
    ThemeSetFocusRing(false);
    utassert(!ThemeFocusRing());
    ThemeSetFocusRing(true);
    utassert(ThemeFocusRing());
}

// FocusableExt::focus_ring, which carries state and nothing else: the
// element remembers the answer, and the paint is what reads it.
static void AControlCanDeclineTheFocusAppearance() {
    Arena* a = ArenaNew();
    El* e = Div(a)->FocusId(7);
    utassert(e->style.focusRing);
    e->FocusRing(false);
    utassert(!e->style.focusRing);
    // Declining the appearance is not declining focus: the element keeps its
    // handle, so Tab still reaches it and Enter still fires.
    utassert(e->style.focusId == 7);
    ArenaDelete(a);
}

void TestThemeSettings() {
    TestSuite("theme settings");
    TheFocusRingIsOnUntilAnApplicationTurnsItOff();
    AControlCanDeclineTheFocusAppearance();
    TheLargeRadiusFollowsTheSmallOne();
    TheFontSizeIsTheOneEverythingIsMeasuredAgainst();
    TheScrollbarModeIsTheThemesUntilAnElementSaysOtherwise();
}
