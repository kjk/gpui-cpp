#include "Story.h"

static El* Swatch(Arena* a, const char* name, Rgba c) {
    const Theme& th = ThemeNow();
    El* col = Div(a)->FlexCol()->Gap(4)->W(88);
    col->Child(Div(a)->W(88)->H(40)->Bg(c)->Radius(6)->Border(1, th.border));
    col->Child(StoryTxt(a, Str(name), 11, th.mutedFg));
    return col;
}

El* ThemeColorsRender(StoryApp* app, Arena* a) {
    (void)app;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Tokens", "Theme color tokens used by the components.");
    El* row = Div(a)->FlexRow()->Gap(12);
    row->Child(Swatch(a, "background", th.background));
    row->Child(Swatch(a, "foreground", th.foreground));
    row->Child(Swatch(a, "primary", th.primary));
    row->Child(Swatch(a, "secondary", th.secondary));
    row->Child(Swatch(a, "muted", th.muted));
    row->Child(Swatch(a, "accent", th.accent));
    row->Child(Swatch(a, "danger", th.danger));
    row->Child(Swatch(a, "border", th.border));
    StorySectionAdd(sec, row);
    page->Child(sec);
    return page;
}

void ThemeColorsClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryThemeColors, ThemeColorsRender, ThemeColorsClick);
