#include "Story.h"

static El* Swatch(Arena* a, const char* name, Rgba c) {
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(4)->W(88);
    col->Child(Div(a)->W(88)->H(40)->Bg(c)->Radius(6)->Border(1, th.border));
    col->Child(StoryTxt(a, Str(name), 11, th.mutedFg));
    return col;
}

El* ThemeColorsRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* base = StorySection(a, "Base", "Surface, text, and border tokens.");
    El* baseRow = Div(a)->FlexRow()->Gap(12)->Wrap();
    baseRow->Child(Swatch(a, "background", th.background));
    baseRow->Child(Swatch(a, "foreground", th.foreground));
    baseRow->Child(Swatch(a, "muted", th.muted));
    baseRow->Child(Swatch(a, "mutedFg", th.mutedFg));
    baseRow->Child(Swatch(a, "border", th.border));
    StorySectionAdd(base, baseRow);
    page->Child(base);

    El* sem = StorySection(a, "Semantic", "Status and action colors.");
    El* semRow = Div(a)->FlexRow()->Gap(12)->Wrap();
    semRow->Child(Swatch(a, "primary", th.primary));
    semRow->Child(Swatch(a, "secondary", th.secondary));
    semRow->Child(Swatch(a, "accent", th.accent));
    semRow->Child(Swatch(a, "info", th.info));
    semRow->Child(Swatch(a, "success", th.success));
    semRow->Child(Swatch(a, "warning", th.warning));
    semRow->Child(Swatch(a, "danger", th.danger));
    StorySectionAdd(sem, semRow);
    page->Child(sem);

    El* chrome = StorySection(a, "Chrome", "Window chrome and sidebar.");
    El* chromeRow = Div(a)->FlexRow()->Gap(12)->Wrap();
    chromeRow->Child(Swatch(a, "titleBar", th.titleBar));
    chromeRow->Child(Swatch(a, "tabBar", th.tabBar));
    chromeRow->Child(Swatch(a, "sidebar", th.sidebar));
    chromeRow->Child(Swatch(a, "scrollbar", th.scrollbarThumb));
    StorySectionAdd(chrome, chromeRow);
    page->Child(chrome);
    return page;
}

void ThemeColorsClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryThemeColors, ThemeColorsRender, ThemeColorsClick);
