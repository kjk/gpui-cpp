#include "Story.h"

struct SheetStory {
    bool sheetOpen = false;
    int selB = -1;

    static El* Render(SheetStory* self, Ctx* cx);
    static void Click(SheetStory* self, Ctx* cx, int id);
};

static void OpenSheet(SheetStory* self, Ctx* cx, const ClickEvent*,
                      intptr_t variant) {
    self->sheetOpen = true;
    self->selB = (int)variant;
    Notify(cx);
}

static void CloseSheet(SheetStory* self, Ctx* cx, const ClickEvent*) {
    self->sheetOpen = false;
}

El* SheetStory::Render(SheetStory* self, Ctx* cx) {
    WinSize size = WindowSize(cx->win);
    Arena* a = cx->a;
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* place = StorySection(cx, "Placement", nullptr);
    El* row = Div(a)->FlexRow()->Gap(8)->Wrap();
    row->Child(component::Button::New(cx, StrL("sheet-right"))
                   ->OnClick(Listen(cx, &OpenSheet, 0))
                   ->Label(StrL("Right"))
                   ->Outline()
                   ->IntoEl());
    row->Child(component::Button::New(cx, StrL("sheet-left"))
                   ->OnClick(Listen(cx, &OpenSheet, 0))
                   ->Label(StrL("Left"))
                   ->Outline()
                   ->IntoEl());
    row->Child(component::Button::New(cx, StrL("sheet-top"))
                   ->OnClick(Listen(cx, &OpenSheet, 0))
                   ->Label(StrL("Top"))
                   ->Outline()
                   ->IntoEl());
    row->Child(component::Button::New(cx, StrL("sheet-bottom"))
                   ->OnClick(Listen(cx, &OpenSheet, 0))
                   ->Label(StrL("Bottom"))
                   ->Outline()
                   ->IntoEl());
    StorySectionAdd(place, row);
    page->Child(place);

    El* scroll = StorySection(cx, "Scrollable Sheet", nullptr);
    StorySectionAdd(scroll, component::Button::New(cx, StrL("sheet-scroll"))
                                ->OnClick(Listen(cx, &OpenSheet, 1))
                                ->Label(StrL("Open scrollable"))
                                ->Outline()
                                ->IntoEl());
    page->Child(scroll);

    El* focus = StorySection(cx, "Focus back test", nullptr);
    StorySectionAdd(focus, component::Button::New(cx, StrL("sheet-focus"))
                               ->OnClick(Listen(cx, &OpenSheet, 2))
                               ->Label(StrL("Open and return focus"))
                               ->Outline()
                               ->IntoEl());
    page->Child(focus);

    if (self->sheetOpen) {
        El* body = Div(a)->FlexCol()->Gap(8);
        body->Child(StoryTxt(cx, StrL("Workspace preferences for your team."),
                             13, ThemeNow().mutedFg)
                        ->Wrap()
                        ->MaxW(280));
        if (self->selB == 1) {
            for (int i = 0; i < 12; i++) {
                body->Child(StoryTxt(cx,
                                     StoryFmt(cx, "Preference row %d", i + 1),
                                     13, ThemeNow().foreground));
            }
        }
        page->Child(component::Sheet::New(cx)
                        ->Open(true)
                        ->Title(StrL("Settings"))
                        ->Body(body)
                        ->OnClose(Listen(cx, &CloseSheet))
                        ->IntoEl(size));
    }
    return page;
}

void SheetStory::Click(SheetStory* self, Ctx* cx, int id) {
    (void)self;
    (void)cx;
    (void)id;
}

STORY_PAGE(StorySheet, SheetStory);
