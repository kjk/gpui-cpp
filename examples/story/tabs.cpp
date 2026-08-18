#include "Story.h"

struct TabsStory {
    int tab = 0;
    // The Dynamic Tabs section keeps its own bar: ids that keep counting up
    // as tabs come and go, and which of them is selected.
    int dynamicIds[12] = {1, 2, 3};
    int dynamicCount = 3;
    int dynamicNext = 4;
    int dynamicTab = 0;
    StoryToolbarState toolbar;

    static El* Render(TabsStory* self, Ctx* cx);
};

static void SetTab(TabsStory* self, Ctx* cx, const ClickEvent*, intptr_t ix) {
    self->tab = (int)ix;
    Notify(cx);
}
static void SetDynamicTab(TabsStory* self, Ctx* cx, const ClickEvent*,
                          intptr_t ix) {
    self->dynamicTab = (int)ix;
    Notify(cx);
}
static void AddDynamicTab(TabsStory* self, Ctx* cx, const ClickEvent*) {
    if (self->dynamicCount < 12) {
        self->dynamicIds[self->dynamicCount++] = self->dynamicNext++;
    }
    Notify(cx);
}
static void RemoveDynamicTab(TabsStory* self, Ctx* cx, const ClickEvent*) {
    if (self->dynamicCount > 0) {
        self->dynamicCount--;
    }
    if (self->dynamicTab >= self->dynamicCount) {
        self->dynamicTab = self->dynamicCount - 1;
    }
    if (self->dynamicTab < 0) {
        self->dynamicTab = 0;
    }
    Notify(cx);
}
// The close button on a tab drops that one rather than the last.
static void CloseDynamicTab(TabsStory* self, Ctx* cx, const ClickEvent*,
                            intptr_t ix) {
    int i = (int)ix;
    if (i < 0 || i >= self->dynamicCount) {
        return;
    }
    for (int k = i; k + 1 < self->dynamicCount; k++) {
        self->dynamicIds[k] = self->dynamicIds[k + 1];
    }
    self->dynamicCount--;
    if (self->dynamicTab >= self->dynamicCount) {
        self->dynamicTab = self->dynamicCount - 1;
    }
    if (self->dynamicTab < 0) {
        self->dynamicTab = 0;
    }
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

    // Dynamic Tabs: a ButtonGroup that grows and shrinks the bar, and tabs
    // that carry a prefix icon and their own close button.
    El* dynamic =
        StorySection(cx, "Dynamic Tabs",
                     "Tabs can be added, removed, and composed with prefix "
                     "and suffix content.");
    El* actions = Div(a)->FlexRow()->Border(1, th.border)->Radius(th.radius);
    actions->Child(component::Button::New(cx, StrL("add-tab"))
                       ->Label(StrL("Add Tab"))
                       ->Compact()
                       ->OnClick(Listen(cx, &AddDynamicTab))
                       ->IntoEl());
    actions->Child(Div(a)->W(1)->H(24)->Shrink0()->Bg(th.border));
    actions->Child(component::Button::New(cx, StrL("remove-tab"))
                       ->Label(StrL("Remove Last"))
                       ->Compact()
                       ->OnClick(Listen(cx, &RemoveDynamicTab))
                       ->IntoEl());
    El* dynCol = Div(a)->FlexCol()->W(kFill)->Gap(8);
    dynCol->Child(actions);
    El* dynBar =
        Div(a)->FlexRow()->W(kFill)->Pad(2)->Gap(2)->Bg(th.muted)->Radius(
            th.radius);
    for (int i = 0; i < self->dynamicCount; i++) {
        El* t = Div(a)
                    ->FlexRow()
                    ->H(26)
                    ->PadX(8)
                    ->Gap(4)
                    ->ItemsCenter()
                    ->Radius(th.radius)
                    ->OnClick(Listen(cx, &SetDynamicTab, i));
        if (i == self->dynamicTab) {
            t->Bg(th.background);
        }
        t->Child(IconEl(a, IconName::BookOpen, 12)->Fg(th.mutedFg));
        t->Child(StoryTxt(cx, StoryFmt(cx, "Tab %d", self->dynamicIds[i]), 13,
                          th.foreground));
        t->Child(component::Button::New(cx, StoryFmt(cx, "dynamic-tab-close-%d",
                                                     self->dynamicIds[i]))
                     ->Ghost()
                     ->WithSize(UiSize::XSmall)
                     ->Icon(IconName::X)
                     ->OnClick(Listen(cx, &CloseDynamicTab, i))
                     ->IntoEl());
        dynBar->Child(t);
    }
    dynCol->Child(dynBar);
    StorySectionAdd(dynamic, dynCol);
    page->Child(dynamic);

    // Filling Space: two segmented tabs sharing the width.
    El* filling =
        StorySection(cx, "Filling Space",
                     "Segmented tabs can share the available width equally.");
    El* fillBar =
        Div(a)->FlexRow()->W(kFill)->Pad(2)->Gap(2)->Bg(th.muted)->Radius(
            th.radius);
    static const char* kFillNames[2] = {"About", "Profile"};
    for (int i = 0; i < 2; i++) {
        El* t =
            Div(a)
                ->H(26)
                ->Grow()
                ->PadX(12)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Radius(th.radius)
                ->OnClick(Listen(cx, &SetTab, i))
                ->Child(StoryTxt(cx, Str(kFillNames[i]), 13, th.foreground));
        if (i == self->tab) {
            t->Bg(th.background);
        }
        fillBar->Child(t);
    }
    StorySectionAdd(filling, fillBar);
    page->Child(filling);
    return page;
}

STORY_PAGE(StoryTabs, TabsStory);
