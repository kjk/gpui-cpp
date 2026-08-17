#include "Story.h"

static El* PopCard(Arena* a, const char* title, const char* body) {
    const Theme& th = ThemeNow();
    El* content = Div(a)
                      ->W(220)
                      ->Pad(12)
                      ->FlexCol()
                      ->Gap(4)
                      ->Border(1, th.border)
                      ->Bg(th.background)
                      ->Radius(th.radius);
    content->Child(StoryTxt(a, Str(title), 14, th.foreground)->Semibold());
    content->Child(StoryTxt(a, Str(body), 12, th.mutedFg)->Wrap()->MaxW(200));
    return content;
}

El* PopoverRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(
        a, "Default",
        "Displays rich content in a portal, triggered by a button.");
    StorySectionAdd(def,
                    component::Popover::New(a)
                        ->Trigger(component::Button::New(a, StrL("open-pop"))
                                      ->Label(StrL("Open popover"))
                                      ->Outline()
                                      ->IntoEl())
                        ->Content(app->selectOpen && app->selB == 0
                                      ? PopCard(a, "Dimensions",
                                                "Set the width and height "
                                                "of the layer.")
                                      : nullptr)
                        ->Open(app->selectOpen && app->selB == 0)
                        ->IntoEl());
    page->Child(def);

    El* form = StorySection(a, "Form", nullptr);
    StorySectionAdd(
        form, component::Popover::New(a)
                  ->Trigger(component::Button::New(a, StrL("pop-form"))
                                ->Label(StrL("Edit"))
                                ->Outline()
                                ->IntoEl())
                  ->Content(app->selectOpen && app->selB == 1
                                ? PopCard(a, "Name", "Update the display name.")
                                : nullptr)
                  ->Open(app->selectOpen && app->selB == 1)
                  ->IntoEl());
    page->Child(form);

    El* list = StorySection(a, "List", nullptr);
    StorySectionAdd(list,
                    component::Popover::New(a)
                        ->Trigger(component::Button::New(a, StrL("pop-list"))
                                      ->Label(StrL("Assign"))
                                      ->Outline()
                                      ->IntoEl())
                        ->Content(app->selectOpen && app->selB == 2
                                      ? component::Menu::New(a)
                                            ->Item(StrL("Jason Lee"))
                                            ->Item(StrL("Ada Lovelace"))
                                            ->IntoEl()
                                      : nullptr)
                        ->Open(app->selectOpen && app->selB == 2)
                        ->IntoEl());
    page->Child(list);

    El* right = StorySection(a, "Right click", nullptr);
    StorySectionAdd(right, component::Button::New(a, StrL("pop-right"))
                               ->Label(StrL("Right click me"))
                               ->Ghost()
                               ->IntoEl());
    page->Child(right);
    return page;
}

void PopoverClick(StoryApp* app, int id) {
    int which = -1;
    if (id == HashClickId(StrL("open-pop"))) {
        which = 0;
    } else if (id == HashClickId(StrL("pop-form"))) {
        which = 1;
    } else if (id == HashClickId(StrL("pop-list"))) {
        which = 2;
    } else if (id == HashClickId(StrL("pop-right"))) {
        which = 3;
    }
    if (which < 0) {
        return;
    }
    if (app->selectOpen && app->selB == which) {
        app->selectOpen = false;
    } else {
        app->selectOpen = true;
        app->selB = which;
    }
}

STORY_PAGE(StoryPopover, PopoverRender, PopoverClick);
