#include "Story.h"

static void CloseSheet(StoryApp* app) {
    app->sheetOpen = false;
}

El* SheetRender(StoryApp* app, Arena* a, WinSize size) {
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);

    El* place = StorySection(a, "Placement", nullptr);
    El* row = Div(a)->FlexRow()->Gap(8)->Wrap();
    row->Child(component::Button::New(a, StrL("sheet-right"))
                   ->Label(StrL("Right"))
                   ->Outline()
                   ->IntoEl());
    row->Child(component::Button::New(a, StrL("sheet-left"))
                   ->Label(StrL("Left"))
                   ->Outline()
                   ->IntoEl());
    row->Child(component::Button::New(a, StrL("sheet-top"))
                   ->Label(StrL("Top"))
                   ->Outline()
                   ->IntoEl());
    row->Child(component::Button::New(a, StrL("sheet-bottom"))
                   ->Label(StrL("Bottom"))
                   ->Outline()
                   ->IntoEl());
    StorySectionAdd(place, row);
    page->Child(place);

    El* scroll = StorySection(a, "Scrollable Sheet", nullptr);
    StorySectionAdd(scroll, component::Button::New(a, StrL("sheet-scroll"))
                                ->Label(StrL("Open scrollable"))
                                ->Outline()
                                ->IntoEl());
    page->Child(scroll);

    El* focus = StorySection(a, "Focus back test", nullptr);
    StorySectionAdd(focus, component::Button::New(a, StrL("sheet-focus"))
                               ->Label(StrL("Open and return focus"))
                               ->Outline()
                               ->IntoEl());
    page->Child(focus);

    if (app->sheetOpen) {
        El* body = Div(a)->FlexCol()->Gap(8);
        body->Child(StoryTxt(a, StrL("Workspace preferences for your team."),
                             13, ThemeNow().mutedFg)
                        ->Wrap()
                        ->MaxW(280));
        if (app->selB == 1) {
            for (int i = 0; i < 12; i++) {
                body->Child(StoryTxt(a, StoryFmt(a, "Preference row %d", i + 1),
                                     13, ThemeNow().foreground));
            }
        }
        page->Child(component::Sheet::New(a)
                        ->Open(true)
                        ->Title(StrL("Settings"))
                        ->Body(body)
                        ->OnClose(MkFunc0(&CloseSheet, app))
                        ->IntoEl(size));
    }
    return page;
}

void SheetClick(StoryApp* app, int id) {
    if (id == HashClickId(StrL("sheet-right")) ||
        id == HashClickId(StrL("sheet-left")) ||
        id == HashClickId(StrL("sheet-top")) ||
        id == HashClickId(StrL("sheet-bottom"))) {
        app->sheetOpen = true;
        app->selB = 0;
    } else if (id == HashClickId(StrL("sheet-scroll"))) {
        app->sheetOpen = true;
        app->selB = 1;
    } else if (id == HashClickId(StrL("sheet-focus"))) {
        app->sheetOpen = true;
        app->selB = 2;
    }
}

STORY_PAGE_SZ(StorySheet, SheetRender, SheetClick);
