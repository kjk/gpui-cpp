#include "Story.h"

struct TabsStory {
    int tab = 0;
    StoryToolbarState toolbar;

    static El* Render(TabsStory* self, Ctx* cx);
};

static void SetTab(TabsStory* self, Ctx* cx, const ClickEvent*, intptr_t ix) {
    self->tab = (int)ix;
    Notify(cx);
}

static El* TabPanel(Ctx* cx, TabsStory* self, El* bar) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    const char* bodies[] = {"Workspace overview", "Recent activity",
                            "Workspace settings"};
    int i = self->tab;
    if (i < 0 || i > 2) {
        i = 0;
    }
    return Div(a)->FlexCol()->W(400)->Child(bar)->Child(
        Div(a)->Pad(12)->Child(StoryTxt(cx, Str(bodies[i]), 13, th.mutedFg)));
}

El* TabsStory::Render(TabsStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* tabs = StorySection(cx, "Tabs", nullptr);
    StorySectionAdd(tabs, TabPanel(cx, self,
                                   component::Tabs::New(cx)
                                       ->Tab(StrL("Overview"))
                                       ->Tab(StrL("Activity"))
                                       ->Tab(StrL("Settings"))
                                       ->Selected(self->tab)
                                       ->OnChange(Listen(cx, &SetTab))
                                       ->IntoEl()));
    page->Child(tabs);

    El* under = StorySection(cx, "Underline Tabs", nullptr);
    StorySectionAdd(under, TabPanel(cx, self,
                                    component::Tabs::New(cx)
                                        ->Tab(StrL("Overview"))
                                        ->Tab(StrL("Activity"))
                                        ->Tab(StrL("Settings"))
                                        ->Selected(self->tab)
                                        ->OnChange(Listen(cx, &SetTab))
                                        ->IntoEl()));
    page->Child(under);

    El* pill = StorySection(cx, "Pill Tabs", nullptr);
    El* pillBar = Div(a)->FlexRow()->Gap(4);
    const char* names[] = {"Overview", "Activity", "Settings"};
    for (int i = 0; i < 3; i++) {
        El* t = Div(a)
                    ->H(28)
                    ->PadX(12)
                    ->ItemsCenter()
                    ->Radius(14)
                    ->OnClick(Listen(cx, &SetTab, i))
                    ->Child(StoryTxt(
                        cx, Str(names[i]), 13,
                        i == self->tab ? th.primaryFg : th.foreground));
        if (i == self->tab) {
            t->Bg(th.primary);
        } else {
            t->HoverBg(th.muted);
        }
        pillBar->Child(t);
    }
    StorySectionAdd(pill, TabPanel(cx, self, pillBar));
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
                    ->OnClick(Listen(cx, &SetTab, i))
                    ->Child(StoryTxt(cx, Str(names[i]), 13, th.foreground));
        if (i == self->tab) {
            t->Bg(th.muted);
        }
        outBar->Child(t);
    }
    StorySectionAdd(outline, TabPanel(cx, self, outBar));
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
                    ->OnClick(Listen(cx, &SetTab, i))
                    ->Child(StoryTxt(cx, Str(names[i]), 13, th.foreground));
        if (i == self->tab) {
            t->Bg(th.background);
        }
        segBar->Child(t);
    }
    StorySectionAdd(seg, TabPanel(cx, self, segBar));
    page->Child(seg);
    return page;
}

STORY_PAGE(StoryTabs, TabsStory);
