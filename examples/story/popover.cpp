#include "Story.h"

struct PopoverStory {
    bool selectOpen = false;
    int selB = -1;

    static El* Render(PopoverStory* self, Ctx* cx);
    static void OnKey(PopoverStory* self, Ctx* cx, const KeyEvent* ev);
};

static void TogglePop(PopoverStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t which) {
    if (self->selectOpen && self->selB == (int)which) {
        self->selectOpen = false;
    } else {
        self->selectOpen = true;
        self->selB = (int)which;
    }
    Notify(cx);
}

static El* PopCard(Ctx* cx, const char* title, const char* body) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
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
                                      ->OnClick(Listen(cx, &TogglePop, 0))
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
                          ->OnClick(Listen(cx, &TogglePop, 1))
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
                                      ->OnClick(Listen(cx, &TogglePop, 2))
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
                               ->OnClick(Listen(cx, &TogglePop, 3))
                               ->Label(StrL("Right click me"))
                               ->Ghost()
                               ->IntoEl());
    page->Child(right);
    return page;
}

// Esc closes what this page has open, like an overlay dismiss.
void PopoverStory::OnKey(PopoverStory* self, Ctx* cx, const KeyEvent* ev) {
    if (ev->vk != VK_ESCAPE) {
        return;
    }
    self->selectOpen = false;
    Notify(cx);
}

STORY_PAGE_KEYS(StoryPopover, PopoverStory);
