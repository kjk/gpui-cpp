#include "Story.h"

static void OnCrumb(StoryApp* app, int i) {
    app->crumbClicked = i;
}

El* BreadcrumbRender(StoryApp* app, Ctx* cx) {
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
                   ->OnClick(MkFunc1(&OnCrumb, app))
                   ->IntoEl());
    if (app->crumbClicked >= 0) {
        static const char* kNames[] = {"Home", "Documents", "Projects",
                                       "Current"};
        int i = app->crumbClicked;
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

void BreadcrumbClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryBreadcrumb, BreadcrumbRender, BreadcrumbClick);
