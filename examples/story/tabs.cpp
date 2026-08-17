#include "Story.h"

static void SetTab(StoryApp* app, int i) {
    app->tab = i;
}

static El* TabPanel(Ctx* cx, StoryApp* app, El* bar) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    const char* bodies[] = {"Workspace overview", "Recent activity",
                            "Workspace settings"};
    int i = app->tab;
    if (i < 0 || i > 2) {
        i = 0;
    }
    return Div(a)->FlexCol()->W(400)->Child(bar)->Child(
        Div(a)->Pad(12)->Child(StoryTxt(cx, Str(bodies[i]), 13, th.mutedFg)));
}

El* TabsRender(StoryApp* app, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = ThemeNow();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, app));

    El* tabs = StorySection(cx, "Tabs", nullptr);
    StorySectionAdd(tabs, TabPanel(cx, app,
                                   component::Tabs::New(cx)
                                       ->Tab(StrL("Overview"))
                                       ->Tab(StrL("Activity"))
                                       ->Tab(StrL("Settings"))
                                       ->Selected(app->tab)
                                       ->OnChange(MkFunc1(&SetTab, app))
                                       ->IntoEl()));
    page->Child(tabs);

    El* under = StorySection(cx, "Underline Tabs", nullptr);
    StorySectionAdd(under, TabPanel(cx, app,
                                    component::Tabs::New(cx)
                                        ->Tab(StrL("Overview"))
                                        ->Tab(StrL("Activity"))
                                        ->Tab(StrL("Settings"))
                                        ->Selected(app->tab)
                                        ->OnChange(MkFunc1(&SetTab, app))
                                        ->IntoEl()));
    page->Child(under);

    El* pill = StorySection(cx, "Pill Tabs", nullptr);
    El* pillBar = Div(a)->FlexRow()->Gap(4);
    const char* names[] = {"Overview", "Activity", "Settings"};
    for (int i = 0; i < 3; i++) {
        El* t =
            Div(a)
                ->H(28)
                ->PadX(12)
                ->ItemsCenter()
                ->Radius(14)
                ->Click(HashClickId(Str(names[i])))
                ->Child(StoryTxt(cx, Str(names[i]), 13,
                                 i == app->tab ? th.primaryFg : th.foreground));
        if (i == app->tab) {
            t->Bg(th.primary);
        } else {
            t->HoverBg(th.muted);
        }
        pillBar->Child(t);
    }
    StorySectionAdd(pill, TabPanel(cx, app, pillBar));
    page->Child(pill);

    El* outline = StorySection(cx, "Outline Tabs", nullptr);
    El* outBar = Div(a)->FlexRow()->Gap(4);
    for (int i = 0; i < 3; i++) {
        El* t = Div(a)
                    ->H(28)
                    ->PadX(12)
                    ->ItemsCenter()
                    ->Radius(th.radius)
                    ->Border(1, th.border)
                    ->Click(HashClickId(Str(names[i])))
                    ->Child(StoryTxt(cx, Str(names[i]), 13, th.foreground));
        if (i == app->tab) {
            t->Bg(th.muted);
        }
        outBar->Child(t);
    }
    StorySectionAdd(outline, TabPanel(cx, app, outBar));
    page->Child(outline);

    El* seg = StorySection(cx, "Segmented Tabs", nullptr);
    El* segBar =
        Div(a)->FlexRow()->Pad(2)->Gap(2)->Bg(th.muted)->Radius(th.radius);
    for (int i = 0; i < 3; i++) {
        El* t = Div(a)
                    ->H(26)
                    ->PadX(12)
                    ->ItemsCenter()
                    ->Radius(th.radius)
                    ->Click(HashClickId(Str(names[i])))
                    ->Child(StoryTxt(cx, Str(names[i]), 13, th.foreground));
        if (i == app->tab) {
            t->Bg(th.background);
        }
        segBar->Child(t);
    }
    StorySectionAdd(seg, TabPanel(cx, app, segBar));
    page->Child(seg);
    return page;
}

void TabsClick(StoryApp* app, int id) {
    if (id == HashClickId(StrL("Overview"))) {
        app->tab = 0;
    } else if (id == HashClickId(StrL("Activity"))) {
        app->tab = 1;
    } else if (id == HashClickId(StrL("Settings"))) {
        app->tab = 2;
    }
}

STORY_PAGE(StoryTabs, TabsRender, TabsClick);
