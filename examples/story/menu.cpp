#include "Story.h"

struct MenuStory {
    bool editOpen = false;
    bool scrollOpen = false;
    int scrollItems = 0;

    static El* Render(MenuStory* self, Ctx* cx);
};

static void ToggleEdit(MenuStory* self, Ctx* cx, const ClickEvent*) {
    self->editOpen = !self->editOpen;
    Notify(cx);
}

static void OpenScroll(MenuStory* self, Ctx* cx, const ClickEvent*,
                       intptr_t items) {
    self->scrollOpen = !(self->scrollOpen && self->scrollItems == (int)items);
    self->scrollItems = (int)items;
    Notify(cx);
}

// The context menu areas are dashed panels; the menu itself follows a right
// click, which this port does not route to a page.
static El* ContextArea(Ctx* cx, Str title, Str hint) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* box = Div(a)
                  ->FlexCol()
                  ->W(kFill)
                  ->Gap(4)
                  ->PadY(24)
                  ->ItemsCenter()
                  ->JustifyCenter()
                  ->Radius(th.radius)
                  ->Border(1, th.border)
                  ->Dashed();
    box->Child(StoryTxt(cx, title, 16, th.foreground));
    if (hint.s) {
        box->Child(StoryTxt(cx, hint, 14, th.mutedFg));
    }
    return box;
}

El* MenuStory::Render(MenuStory* self, Ctx* cx) {
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* popup = StorySection(
        cx, "Popup Menu",
        "Supports actions, links, checks, icons, custom rows, and nested "
        "menus.");
    El* editWrap = Div(a)->FlexCol();
    editWrap->Child(component::Button::New(cx, StrL("edit-menu"))
                        ->Outline()
                        ->Label(StrL("Edit"))
                        ->OnClick(Listen(cx, &ToggleEdit))
                        ->IntoEl());
    if (self->editOpen) {
        editWrap->Child(component::Menu::New(cx)
                            ->Item(StrL("About"))
                            ->Item(StrL("Settings"))
                            ->Item(StrL("Copy"))
                            ->Item(StrL("Cut"))
                            ->Item(StrL("Paste"))
                            ->Item(StrL("Search"))
                            ->IntoEl()
                            ->AnchorBelow(4)
                            ->Left(0));
    }
    StorySectionAdd(popup, editWrap);
    page->Child(popup);

    El* ctxSec =
        StorySection(cx, "Context Menu",
                     "Different regions can provide their own right-click "
                     "actions.");
    El* areas = Div(a)->FlexCol()->W(kFill)->Gap(16);
    areas->Child(ContextArea(
        cx, StrL("Right click to open ContextMenu"),
        StrL("You can right click anywhere in this area to open the context "
             "menu.")));
    areas->Child(ContextArea(
        cx, StrL("Here is another area with context menu."), Str{}));
    areas->Child(ContextArea(cx, StrL("ContextMenu area 1"), Str{}));
    StorySectionAdd(ctxSec, areas);
    page->Child(ctxSec);

    El* scroll = StorySection(
        cx, "Scrollable",
        "Long menus constrain their height while short menus stay compact.");
    El* scrollRow = Div(a)->FlexRow()->FlexWrap()->Gap(8)->ItemsStart();
    El* longWrap = Div(a)->FlexCol();
    longWrap->Child(component::Button::New(cx, StrL("scroll-100"))
                        ->Outline()
                        ->Label(StrL("Scrollable Menu (100 items)"))
                        ->OnClick(Listen(cx, &OpenScroll, 100))
                        ->IntoEl());
    El* shortWrap = Div(a)->FlexCol();
    shortWrap->Child(component::Button::New(cx, StrL("scroll-5"))
                         ->Outline()
                         ->Label(StrL("Scrollable Menu (5 items)"))
                         ->OnClick(Listen(cx, &OpenScroll, 5))
                         ->IntoEl());
    if (self->scrollOpen) {
        component::Menu* m = component::Menu::New(cx);
        int shown = self->scrollItems > 12 ? 12 : self->scrollItems;
        for (int i = 1; i <= shown; i++) {
            m->Item(StoryFmt(cx, "Item %d", i));
        }
        El* menu = m->IntoEl()->AnchorBelow(4)->Left(0);
        (self->scrollItems == 5 ? shortWrap : longWrap)->Child(menu);
    }
    scrollRow->Child(longWrap)->Child(shortWrap);
    StorySectionAdd(scroll, scrollRow);
    page->Child(scroll);
    return page;
}

STORY_PAGE(StoryMenu, MenuStory);
