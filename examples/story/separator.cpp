#include "Story.h"

El* SeparatorRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* h = StorySection(
        a, "Horizontal",
        "Separates stacked content, with optional labels and dashed rules.");
    El* col = Div(a)->FlexCol()->Gap(16)->W(520);
    col->Child(component::Separator::Horizontal(a)->IntoEl());
    col->Child(component::Separator::Horizontal(a)
                   ->Label(StrL("With Label"))
                   ->IntoEl());
    col->Child(component::Separator::Horizontal(a)->Dashed()->IntoEl());
    col->Child(component::Separator::Horizontal(a)
                   ->Dashed()
                   ->Label(StrL("Dashed With Label"))
                   ->IntoEl());
    StorySectionAdd(h, col);
    page->Child(h);

    El* v = StorySection(a, "Vertical",
                         "Separates actions or values arranged in a row.");
    El* row = Div(a)->FlexRow()->Gap(16)->H(100)->ItemsCenter();
    row->Child(component::Separator::Vertical(a)->IntoEl());
    row->Child(
        component::Separator::Vertical(a)->Label(StrL("Solid"))->IntoEl());
    row->Child(component::Separator::Vertical(a)->Dashed()->IntoEl());
    row->Child(component::Separator::Vertical(a)
                   ->Dashed()
                   ->Label(StrL("Dashed"))
                   ->IntoEl());
    StorySectionAdd(v, row);
    page->Child(v);

    El* ctx = StorySection(
        a, "In Context",
        "Horizontal and vertical rules can structure compact content.");
    El* box = Div(a)->FlexCol()->Gap(16)->W(480);
    box->Child(StoryTxt(a, StrL("Hello GPUI Component"), 14, th.foreground));
    box->Child(
        StoryTxt(a,
                 StrL("GPUI Component is a Rust GUI components for building "
                      "fantastic cross-platform desktop application by using "
                      "GPUI."),
                 13, th.mutedFg)
            ->Wrap()
            ->MaxW(460));
    box->Child(component::Separator::Horizontal(a)->IntoEl());
    El* links = Div(a)->FlexRow()->Gap(16)->ItemsCenter();
    links->Child(StoryTxt(a, StrL("Docs"), 14, th.foreground));
    links->Child(component::Separator::Vertical(a)->Dashed()->IntoEl());
    links->Child(StoryTxt(a, StrL("GitHub"), 14, th.foreground));
    links->Child(component::Separator::Vertical(a)->Dashed()->IntoEl());
    links->Child(StoryTxt(a, StrL("Source"), 14, th.foreground));
    box->Child(links);
    StorySectionAdd(ctx, box);
    page->Child(ctx);
    return page;
}

void SeparatorClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySeparator, SeparatorRender, SeparatorClick);
