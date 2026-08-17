#include "Story.h"

El* SeparatorRender(StoryApp* app, Arena* a) {
    (void)app;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* h = StorySection(a, "Horizontal", "Separates stacked content, with optional labels and dashed rules.");
    El* col = Div(a)->FlexCol()->Gap(16)->W(520);
    col->Child(component::Separator::Horizontal(a)->IntoEl());
    col->Child(component::Separator::Horizontal(a)->Label(StrL("With Label"))->IntoEl());
    col->Child(component::Separator::Horizontal(a)->Dashed()->IntoEl());
    col->Child(component::Separator::Horizontal(a)->Dashed()->Label(StrL("Dashed With Label"))->IntoEl());
    StorySectionAdd(h, col);
    page->Child(h);

    El* v = StorySection(a, "Vertical", "Separates actions or values arranged in a row.");
    El* row = Div(a)->FlexRow()->Gap(16)->H(100)->ItemsCenter();
    row->Child(component::Separator::Vertical(a)->IntoEl());
    row->Child(component::Separator::Vertical(a)->Label(StrL("Solid"))->IntoEl());
    row->Child(component::Separator::Vertical(a)->Dashed()->IntoEl());
    row->Child(component::Separator::Vertical(a)->Dashed()->Label(StrL("Dashed"))->IntoEl());
    StorySectionAdd(v, row);
    page->Child(v);
    return page;
}

void SeparatorClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StorySeparator, SeparatorRender, SeparatorClick);
