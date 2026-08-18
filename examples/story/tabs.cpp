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

// The Rust story runs the same eight tabs through every variant; Profile is
// disabled in the first bar.
static const char* kTabNames[] = {"Account", "Profile",    "Documents",
                                  "Mail",    "Appearance", "Settings",
                                  "About",   "License"};
static const int kTabCount = 8;

El* TabsStory::Render(TabsStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* page = Div(a)->FlexCol()->Gap(12)->W(kFill);
    page->Child(StoryToolbar(cx, self));

    El* tabs = StorySection(cx, "Tabs", nullptr);
    component::Tabs* barTabs = component::Tabs::New(cx);
    for (int i = 0; i < kTabCount; i++) {
        barTabs->Tab(Str(kTabNames[i]));
    }
    StorySectionAdd(tabs, barTabs->Selected(self->tab)
                              ->OnChange(Listen(cx, &SetTab))
                              ->IntoEl());
    page->Child(tabs);

    El* under = StorySection(cx, "Underline Tabs", nullptr);
    component::Tabs* underTabs = component::Tabs::New(cx);
    for (int i = 0; i < kTabCount; i++) {
        underTabs->Tab(Str(kTabNames[i]));
    }
    StorySectionAdd(under, underTabs->Selected(self->tab)
                               ->OnChange(Listen(cx, &SetTab))
                               ->IntoEl());
    page->Child(under);

    El* pill = StorySection(cx, "Pill Tabs", nullptr);
    El* pillBar = Div(a)->FlexRow()->FlexWrap()->Gap(4);
    const char** names = kTabNames;
    for (int i = 0; i < kTabCount; i++) {
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
    StorySectionAdd(pill, pillBar);
    page->Child(pill);

    El* outline = StorySection(cx, "Outline Tabs", nullptr);
    // Outline tabs are pills: a light border, the selected one in the
    // foreground color.
    El* outBar = Div(a)->FlexRow()->FlexWrap()->Gap(8);
    for (int i = 0; i < kTabCount; i++) {
        El* t =
            Div(a)
                ->H(32)
                ->PadX(12)
                ->ItemsCenter()
                ->Radius(16)
                ->Border(1, i == self->tab ? th.foreground : th.border)
                ->OnClick(Listen(cx, &SetTab, i))
                ->Child(StoryTxt(cx, Str(names[i]), 14,
                                 i == self->tab ? th.foreground : th.mutedFg));
        outBar->Child(t);
    }
    StorySectionAdd(outline, outBar);
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
    StorySectionAdd(seg, segBar);
    page->Child(seg);
    return page;
}

STORY_PAGE(StoryTabs, TabsStory);
