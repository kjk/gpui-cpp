#include "Story.h"

static El* Swatch(Ctx* cx, const char* name, Rgba c) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(4)->W(88);
    col->Child(Div(a)->W(88)->H(40)->Bg(c)->Radius(6)->Border(1, th.border));
    col->Child(StoryTxt(cx, Str(name), 11, th.mutedFg));
    return col;
}

El* ThemeColorsRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* base = StorySection(cx, "Base", "Surface, text, and border tokens.");
    El* baseRow = Div(a)->FlexRow()->Gap(12)->Wrap();
    baseRow->Child(Swatch(cx, "background", th.background));
    baseRow->Child(Swatch(cx, "foreground", th.foreground));
    baseRow->Child(Swatch(cx, "muted", th.muted));
    baseRow->Child(Swatch(cx, "mutedFg", th.mutedFg));
    baseRow->Child(Swatch(cx, "border", th.border));
    StorySectionAdd(base, baseRow);
    page->Child(base);

    El* sem = StorySection(cx, "Semantic", "Status and action colors.");
    El* semRow = Div(a)->FlexRow()->Gap(12)->Wrap();
    semRow->Child(Swatch(cx, "primary", th.primary));
    semRow->Child(Swatch(cx, "secondary", th.secondary));
    semRow->Child(Swatch(cx, "accent", th.accent));
    semRow->Child(Swatch(cx, "info", th.info));
    semRow->Child(Swatch(cx, "success", th.success));
    semRow->Child(Swatch(cx, "warning", th.warning));
    semRow->Child(Swatch(cx, "danger", th.danger));
    StorySectionAdd(sem, semRow);
    page->Child(sem);

    El* chrome = StorySection(cx, "Chrome", "Window chrome and sidebar.");
    El* chromeRow = Div(a)->FlexRow()->Gap(12)->Wrap();
    chromeRow->Child(Swatch(cx, "titleBar", th.titleBar));
    chromeRow->Child(Swatch(cx, "tabBar", th.tabBar));
    chromeRow->Child(Swatch(cx, "sidebar", th.sidebar));
    chromeRow->Child(Swatch(cx, "scrollbar", th.scrollbarThumb));
    StorySectionAdd(chrome, chromeRow);
    page->Child(chrome);
    return page;
}

void ThemeColorsClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryThemeColors, ThemeColorsRender, ThemeColorsClick);
