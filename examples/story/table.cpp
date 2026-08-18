#include "Story.h"

struct TableStory {
    static El* Render(TableStory* self, Ctx* cx);
};

El* TableStory::Render(TableStory* self, Ctx* cx) {
    Arena* a = cx->a;
    static const char* heads[] = {"Component", "Status", "Version"};
    static const char* r0[] = {"gpui-base", "Stable", "0.4.1"};
    static const char* r1[] = {"gpui-component", "Active", "0.4.1"};
    static const char* r2[] = {"story-web", "Preview", "0.2.8"};
    static const char** rows[] = {r0, r1, r2};
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(cx, "Default", "A responsive table component.");
    StorySectionAdd(
        sec,
        component::Table::New(cx)->Heads(heads, 3)->Rows(rows, 3)->IntoEl());
    page->Child(sec);

    El* bordered = StorySection(cx, "Bordered", nullptr);
    StorySectionAdd(bordered, Div(a)
                                  ->Border(1, ThemeNow().border)
                                  ->Radius(ThemeNow().radius)
                                  ->Child(component::Table::New(cx)
                                              ->Heads(heads, 3)
                                              ->Rows(rows, 3)
                                              ->IntoEl()));
    page->Child(bordered);
    return page;
}

STORY_PAGE(StoryTable, TableStory);
