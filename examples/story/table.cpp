#include "Story.h"

El* TableRender(StoryApp* app, Arena* a) {
    (void)app;
    static const char* heads[] = {"Component", "Status", "Version"};
    static const char* r0[] = {"gpui-base", "Stable", "0.4.1"};
    static const char* r1[] = {"gpui-component", "Active", "0.4.1"};
    static const char* r2[] = {"story-web", "Preview", "0.2.8"};
    static const char** rows[] = {r0, r1, r2};
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "A responsive table component.");
    StorySectionAdd(
        sec,
        component::Table::New(a)->Heads(heads, 3)->Rows(rows, 3)->IntoEl());
    page->Child(sec);

    El* bordered = StorySection(a, "Bordered", nullptr);
    StorySectionAdd(bordered, Div(a)
                                  ->Border(1, ThemeNow().border)
                                  ->Radius(ThemeNow().radius)
                                  ->Child(component::Table::New(a)
                                              ->Heads(heads, 3)
                                              ->Rows(rows, 3)
                                              ->IntoEl()));
    page->Child(bordered);
    return page;
}

void TableClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryTable, TableRender, TableClick);
