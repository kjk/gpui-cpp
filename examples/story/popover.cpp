#include "Story.h"

struct PopoverStory {
    bool selectOpen = false;
    int selB = -1;

    static El* Render(PopoverStory* self, Ctx* cx);
    static void Click(PopoverStory* self, Ctx* cx, int id);
};

static El* PopCard(Ctx* cx, const char* title, const char* body) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* content = Div(a)
                      ->W(220)
                      ->Pad(12)
                      ->FlexCol()
                      ->Gap(4)
                      ->Border(1, th.border)
                      ->Bg(th.background)
                      ->Radius(th.radius);
    content->Child(StoryTxt(cx, Str(title), 14, th.foreground)->Semibold());
    content->Child(StoryTxt(cx, Str(body), 12, th.mutedFg)->Wrap()->MaxW(200));
    return content;
}

El* PopoverStory::Render(PopoverStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* def = StorySection(
        cx, "Default",
        "Displays rich content in a portal, triggered by a button.");
    StorySectionAdd(def,
                    component::Popover::New(cx)
                        ->Trigger(component::Button::New(cx, StrL("open-pop"))
                                      ->Label(StrL("Open popover"))
                                      ->Outline()
                                      ->IntoEl())
                        ->Content(self->selectOpen && self->selB == 0
                                      ? PopCard(cx, "Dimensions",
                                                "Set the width and height "
                                                "of the layer.")
                                      : nullptr)
                        ->Open(self->selectOpen && self->selB == 0)
                        ->IntoEl());
    page->Child(def);

    El* form = StorySection(cx, "Form", nullptr);
    StorySectionAdd(
        form,
        component::Popover::New(cx)
            ->Trigger(component::Button::New(cx, StrL("pop-form"))
                          ->Label(StrL("Edit"))
                          ->Outline()
                          ->IntoEl())
            ->Content(self->selectOpen && self->selB == 1
                          ? PopCard(cx, "Name", "Update the display name.")
                          : nullptr)
            ->Open(self->selectOpen && self->selB == 1)
            ->IntoEl());
    page->Child(form);

    El* list = StorySection(cx, "List", nullptr);
    StorySectionAdd(list,
                    component::Popover::New(cx)
                        ->Trigger(component::Button::New(cx, StrL("pop-list"))
                                      ->Label(StrL("Assign"))
                                      ->Outline()
                                      ->IntoEl())
                        ->Content(self->selectOpen && self->selB == 2
                                      ? component::Menu::New(cx)
                                            ->Item(StrL("Jason Lee"))
                                            ->Item(StrL("Ada Lovelace"))
                                            ->IntoEl()
                                      : nullptr)
                        ->Open(self->selectOpen && self->selB == 2)
                        ->IntoEl());
    page->Child(list);

    El* right = StorySection(cx, "Right click", nullptr);
    StorySectionAdd(right, component::Button::New(cx, StrL("pop-right"))
                               ->Label(StrL("Right click me"))
                               ->Ghost()
                               ->IntoEl());
    page->Child(right);
    return page;
}

void PopoverStory::Click(PopoverStory* self, Ctx* cx, int id) {
    (void)cx;
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
    if (self->selectOpen && self->selB == which) {
        self->selectOpen = false;
    } else {
        self->selectOpen = true;
        self->selB = which;
    }
}

STORY_PAGE(StoryPopover, PopoverStory);
