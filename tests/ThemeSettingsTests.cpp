/* The theme knobs the story's Appearance menu writes — crates/story's
 * FontSizeSelector, which sets `Theme::global_mut(cx).font_size`, `.radius`
 * and `Theme::set_scrollbar_mode`. Both the styled theme and its Base
 * projection are application-owned globals, as they are in Rust. */

#include "Test.h"

static bool SameColor(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

// on_select_radius: radius_lg is two more than the radius, except at zero,
// where a large radius is no radius either.
static void TheLargeRadiusFollowsTheSmallOne() {
    App app;
    ThemeSetRadius(&app, 8);
    utassertnear(ThemeLight(&app).radius, 8.f);
    utassertnear(ThemeLight(&app).radiusLg, 10.f);
    // Both palettes move together: the mode can be switched under them.
    utassertnear(ThemeDark(&app).radius, 8.f);
    utassertnear(ThemeDark(&app).radiusLg, 10.f);

    ThemeSetRadius(&app, 0);
    utassertnear(ThemeLight(&app).radius, 0.f);
    utassertnear(ThemeLight(&app).radiusLg, 0.f);

    // theme/mod.rs: 6 and 8 are what a theme starts with.
    ThemeSetRadius(&app, 6);
    utassertnear(ThemeLight(&app).radius, 6.f);
    utassertnear(ThemeLight(&app).radiusLg, 8.f);
    AppGlobalClear(&app);
}

// on_select_font: the root size every element inherits from. A size of zero
// is not one, so it answers with the default rather than collapsing the text.
static void TheFontSizeIsTheOneEverythingIsMeasuredAgainst() {
    App app;
    utassertnear(ThemeFontSize(&app), 16.f);
    ThemeSetFontSize(&app, 18);
    utassertnear(ThemeFontSize(&app), 18.f);
    ThemeSetFontSize(&app, 0);
    utassertnear(ThemeFontSize(&app), 16.f);
    AppGlobalClear(&app);
}

// Theme::set_scrollbar_mode: the default an element that names none gets.
static void TheScrollbarModeIsTheThemesUntilAnElementSaysOtherwise() {
    App app;
    utassert(ScrollbarModeNow(&app) == ScrollbarMode::Scrolling);
    ScrollbarModeSet(&app, ScrollbarMode::Hover);
    utassert(ScrollbarModeNow(&app) == ScrollbarMode::Hover);
    ScrollbarModeSet(&app, ScrollbarMode::Always);
    AppGlobalClear(&app);
}

// Theme::focus_ring: off drops the ring outside the border and leaves the
// tinted border, which is the half that costs no room.
static void TheFocusRingIsOnUntilAnApplicationTurnsItOff() {
    App app;
    utassert(ThemeFocusRing(&app));
    ThemeSetFocusRing(&app, false);
    utassert(!ThemeFocusRing(&app));
    ThemeSetFocusRing(&app, true);
    utassert(ThemeFocusRing(&app));
    AppGlobalClear(&app);
}

static void ThemeSettingsAreIsolatedPerApplication() {
    App first;
    App second;
    ThemeSetRadius(&first, 12);
    ThemeSetFontSize(&first, 20);
    ThemeSetFocusRing(&first, false);
    ScrollbarModeSet(&first, ScrollbarMode::Hover);
    utassertnear(ThemeLight(&first).radius, 12.f);
    utassertnear(ThemeLight(&second).radius, 6.f);
    utassertnear(ThemeFontSize(&first), 20.f);
    utassertnear(ThemeFontSize(&second), 16.f);
    utassert(!ThemeFocusRing(&first));
    utassert(ThemeFocusRing(&second));
    utassert(ScrollbarModeNow(&first) == ScrollbarMode::Hover);
    utassert(ScrollbarModeNow(&second) == ScrollbarMode::Scrolling);
    AppGlobalClear(&first);
    AppGlobalClear(&second);
}

static void StyledThemeChangesProjectIntoBase() {
    App first;
    App second;
    component::Init(&first);
    component::Init(&second);

    ThemeSet(&first, ThemeMode::Dark);
    ThemeSetRadius(&first, 0);
    ScrollbarModeSet(&first, ScrollbarMode::Hover);
    const BaseTheme* a = BaseThemeGlobal(&first);
    const BaseTheme* b = BaseThemeGlobal(&second);
    utassert(a && b);
    utassert(a->scrollbar.mode == ScrollbarMode::Hover);
    utassert(b->scrollbar.mode == ScrollbarMode::Scrolling);
    utassert(a->scrollbar.motion.thumbHoverEntrance ==
             ScrollbarEntrance::SlideAndFade);
    utassert(a->scrollbar.styles.thumb.hasBackground);
    utassert(a->scrollbar.styles.thumb.hasRadius);
    utassertnear(a->scrollbar.styles.thumb.radius, 0.f);
    utassert(SameColor(a->resizable.handle, ThemeNow(&first).border));
    utassert(SameColor(a->resizable.activeHandle,
                       ThemeNow(&first).dragBorder));
    utassert(SameColor(a->tokens.colors.background,
                       ThemeNow(&first).background));
    utassert(SameColor(b->tokens.colors.background,
                       ThemeNow(&second).background));

    Arena* arena = ArenaNew();
    Ctx cx = {};
    cx.a = arena;
    cx.app = &first;
    El* scrollbar = Scrollbar::New(&cx);
    utassert(scrollbar->scrollThemeSet);
    utassert(scrollbar->scrollMode == ScrollbarMode::Hover);
    utassert(scrollbar->scrollMotion.thumbHoverEntrance ==
             ScrollbarEntrance::SlideAndFade);
    utassertnear(scrollbar->scrollThumbRadius, 0.f);
    ArenaDelete(arena);

    AppGlobalClear(&first);
    AppGlobalClear(&second);
}

// The renderer sees only the narrow projection. Component-only table, list,
// sidebar and button families never enter src/gpui's style state.
static void StyledThemeChangesProjectIntoTheRuntimeSeam() {
    App app;
    component::Init(&app);
    ThemeSet(&app, ThemeMode::Dark);
    ThemeSetRadius(&app, 9);
    ThemeSetFontSize(&app, 18);
    ThemeSetFocusRing(&app, false);
    ScrollbarModeSet(&app, ScrollbarMode::Hover);

    const Theme& theme = ThemeNow(&app);
    const RuntimeStyle& runtime = RuntimeStyleNow(&app);
    utassert(SameColor(runtime.background, theme.background));
    utassert(SameColor(runtime.foreground, theme.foreground));
    utassert(SameColor(runtime.mutedForeground, theme.mutedFg));
    utassert(SameColor(runtime.border, theme.border));
    utassert(SameColor(runtime.ring, theme.ring));
    utassert(SameColor(runtime.popover, theme.popover));
    utassert(SameColor(runtime.popoverForeground, theme.popoverFg));
    utassert(runtime.progress.gradient == theme.tokens.progress.gradient);
    utassert(runtime.scrollbarThumb.gradient ==
             theme.tokens.scrollbarThumb.gradient);
    utassertnear(runtime.radius, 9.f);
    utassertnear(runtime.fontSize, 18.f);
    utassert(runtime.scrollbarMode == ScrollbarMode::Hover);
    utassert(!runtime.focusRing);

    AppGlobalClear(&app);
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
    ThemeSettingsAreIsolatedPerApplication();
    StyledThemeChangesProjectIntoBase();
    StyledThemeChangesProjectIntoTheRuntimeSeam();
}
