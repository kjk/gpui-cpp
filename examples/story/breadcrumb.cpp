#include "Story.h"

struct BreadcrumbStory {
    int crumbClicked = -1;
    static El* Render(BreadcrumbStory* self, Ctx* cx);
};

static void OnCrumb(BreadcrumbStory* self, Ctx* cx, const ClickEvent*,
                    intptr_t i) {
    self->crumbClicked = i;
}

El* BreadcrumbStory::Render(BreadcrumbStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(cx, "Default",
                           "Shows the current location in a hierarchy.");
    StorySectionAdd(def, component::Breadcrumb::New(cx)
                             ->Item(StrL("Home"))
                             ->Item(StrL("Documents"))
                             ->Item(StrL("Projects"))
                             ->IntoEl());
    page->Child(def);

    El* inter = StorySection(
        cx, "Interactive", "Earlier levels can respond to navigation clicks.");
    El* col = Div(a)->FlexCol()->Gap(16)->ItemsCenter();
    col->Child(component::Breadcrumb::New(cx)
                   ->Item(StrL("Home"))
                   ->Item(StrL("Documents"))
                   ->Item(StrL("Projects"))
                   ->Item(StrL("Current"))
                   ->OnClick(Listen(cx, &OnCrumb))
                   ->IntoEl());
    if (self->crumbClicked >= 0) {
        static const char* kNames[] = {"Home", "Documents", "Projects",
                                       "Current"};
        int i = self->crumbClicked;
        if (i > 3) {
            i = 3;
        }
        col->Child(StoryTxt(cx, StoryFmt(cx, "Selected: %s", kNames[i]), 13,
                            th.foreground));
    }
    StorySectionAdd(inter, col);
    page->Child(inter);
    return page;
}

STORY_PAGE(StoryBreadcrumb, BreadcrumbStory);
