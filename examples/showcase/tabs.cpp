#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

enum {
    ClickTab0 = 520,
    ClickTab1 = 521,
    ClickTab2 = 522
};

El* ShowcaseTabs(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const char* labels[] = {"Overview", "Activity", "Settings"};
    El* bar = Tabs::New(a, StrL("example-tabs"))
                  ->FlexRow()
                  ->PadX(8)
                  ->PadT(4)
                  ->BorderB(1, Rgb(0xd4, 0xd4, 0xd4));
    for (int i = 0; i < 3; i++) {
        bool on = app->tab == i;
        El* tab =
            Tab::New(a, DupFmt(a, "tab-%d", i), ClickTab0 + i)
                ->H(28)
                ->PadX(8)
                ->ItemsCenter()
                ->BorderB(2, on ? Rgb(0x17, 0x17, 0x17) : Rgb(0xff, 0xff, 0xff))
                ->HoverBg(Rgb(0xf5, 0xf5, 0xf5));
        El* lab =
            TextEl(a, Str(labels[i]))->Font(12)->Fg(Rgb(0x17, 0x17, 0x17));
        if (on) {
            lab->Semibold();
        }
        tab->Child(lab);
        bar->Child(tab);
    }

    const char* title = "Workspace overview";
    const char* sub = "12 components · 4 contributors · updated today";
    if (app->tab == 1) {
        title = "Recent activity";
        sub = "Button example was updated 8 minutes ago.";
    } else if (app->tab == 2) {
        title = "Project settings";
        sub = "Manage notifications and member access.";
    }

    return Div(a)
        ->FlexCol()
        ->W(288)
        ->Border(1, ScBorder())
        ->Child(bar)
        ->Child(Div(a)
                    ->MinH(80)
                    ->Pad(12)
                    ->FlexCol()
                    ->Gap(4)
                    ->Child(TextEl(a, Str(title))
                                ->Font(12)
                                ->Fg(Rgb(0x17, 0x17, 0x17)))
                    ->Child(TextEl(a, Str(sub))
                                ->Font(12)
                                ->Fg(Rgb(0x73, 0x73, 0x73))
                                ->Wrap()
                                ->MaxW(260)));
}

void ShowcaseTabsClick(ShowcaseApp* app, int id) {
    if (id >= ClickTab0 && id <= ClickTab2) {
        app->tab = id - ClickTab0;
    }
}

SHOWCASE_PAGE(CompTabs, ShowcaseTabs, ShowcaseTabsClick);
