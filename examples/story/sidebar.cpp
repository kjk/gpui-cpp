#include "Story.h"

// The two sidebar groups, in the order the Rust story lists them.
struct SidebarItem {
    const char* label;
    IconName icon;
    bool expandable;
};

static const SidebarItem kPlatform[] = {
    {"Playground", IconName::SquareTerminal, true},
    {"Models", IconName::Bot, true},
    {"Documentation", IconName::BookOpen, true},
    {"Settings", IconName::Settings2, true},
};
static const SidebarItem kProjects[] = {
    {"Design Engineering", IconName::Frame, false},
    {"Sales and Marketing", IconName::ChartPie, false},
    {"Travel", IconName::Map, false},
};
static const char* kPlaygroundSubs[] = {"History", "Starred", "Settings"};

enum {
    SidebarOptIcon = 600,
    SidebarOptOffcanvas,
    SidebarOptFixed,
    SidebarOptRight,
    SidebarOptClickToOpen,
    SidebarOptDynamic
};

struct SidebarStory {
    int active = 0;
    int activeSub = -1;
    bool optionsOpen = false;
    bool historySwitch = false;
    int collapsible = 0; // Icon
    bool rightSide = false;
    bool clickToOpen = false;
    bool dynamicChildren = false;

    static El* Render(SidebarStory* self, Ctx* cx);
};

static void SidebarPick(SidebarStory* self, Ctx* cx, const ClickEvent*,
                        intptr_t ix) {
    self->active = (int)ix;
    self->activeSub = -1;
    Notify(cx);
}
static void SidebarPickSub(SidebarStory* self, Ctx* cx, const ClickEvent*,
                           intptr_t ix) {
    self->activeSub = (int)ix;
    Notify(cx);
}
static void ToggleSidebarOptions(SidebarStory* self, Ctx* cx,
                                 const ClickEvent*) {
    self->optionsOpen = !self->optionsOpen;
    Notify(cx);
}
static void SidebarOptionAct(SidebarStory* self, Ctx* cx, const ClickEvent*,
                             intptr_t act) {
    switch (act) {
        case SidebarOptIcon:
            self->collapsible = 0;
            break;
        case SidebarOptOffcanvas:
            self->collapsible = 1;
            break;
        case SidebarOptFixed:
            self->collapsible = 2;
            break;
        case SidebarOptRight:
            self->rightSide = !self->rightSide;
            break;
        case SidebarOptClickToOpen:
            self->clickToOpen = !self->clickToOpen;
            break;
        default:
            self->dynamicChildren = !self->dynamicChildren;
            break;
    }
    self->optionsOpen = false;
    Notify(cx);
}
static void ToggleHistory(SidebarStory* self, Ctx* cx, const ClickEvent*) {
    self->historySwitch = !self->historySwitch;
    Notify(cx);
}

static El* GroupLabel(Ctx* cx, const char* text) {
    Arena* a = cx->a;
    return Div(a)->PadX(8)->PadY(6)->Child(
        StoryTxt(cx, Str(text), 12, cx->theme().mutedFg));
}

El* SidebarStory::Render(SidebarStory* self, Ctx* cx) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    Listener pick = Listen(cx, &SidebarPick);
    Listener pickSub = Listen(cx, &SidebarPickSub);

    El* frame = Div(a)
                    ->FlexRow()
                    ->W(kFill)
                    ->H(WindowSize(cx->win).dipH - 190)
                    ->Radius(th.radius)
                    ->Border(1, th.border);

    // The sidebar: header, two groups, footer.
    El* side =
        Div(a)->FlexCol()->W(220)->H(kFill)->Pad(8)->BorderR(1, th.border);
    El* header = Div(a)->FlexRow()->W(kFill)->Gap(8)->PadY(8)->ItemsCenter();
    header->Child(Div(a)
                      ->W(32)
                      ->H(32)
                      ->Shrink0()
                      ->ItemsCenter()
                      ->JustifyCenter()
                      ->Radius(th.radius)
                      ->Bg(th.success)
                      ->Child(IconEl(a, IconName::GalleryVerticalEnd, 16)
                                  ->Fg(th.successFg)));
    El* company = Div(a)->FlexCol()->Grow();
    company->Child(StoryTxt(cx, StrL("Company Name"), 14, th.foreground)
                       ->LineHeight(1.25f));
    company->Child(StoryTxt(cx, StrL("Enterprise"), 12, th.foreground)
                       ->LineHeight(1.25f));
    header->Child(company);
    header->Child(
        IconEl(a, IconName::ChevronsUpDown, 16)->Fg(th.foreground)->Shrink0());
    side->Child(header);

    side->Child(GroupLabel(cx, "Platform"));
    for (int i = 0; i < 4; i++) {
        const SidebarItem& item = kPlatform[i];
        bool active = self->active == i;
        El* row = Div(a)
                      ->FlexRow()
                      ->W(kFill)
                      ->H(32)
                      ->PadX(8)
                      ->Gap(8)
                      ->ItemsCenter()
                      ->Radius(th.radius)
                      ->HoverBg(th.muted);
        if (active) {
            row->Bg(th.accent);
        }
        row->Child(IconEl(a, item.icon, 16)->Fg(th.foreground));
        row->Child(Div(a)->Grow()->Child(
            StoryTxt(cx, Str(item.label), 16, th.foreground)));
        row->Child(
            IconEl(a, active ? IconName::ChevronDown : IconName::ChevronRight,
                   16)
                ->Fg(th.mutedFg));
        row->Click(HashClickId(StoryFmt(cx, "sidebar-%d", i)))
            ->OnClick(ListenerArg(pick, i));
        side->Child(row);
        if (!active) {
            continue;
        }
        // The open item shows its children against a rail.
        El* subs = Div(a)->FlexRow()->W(kFill)->PadL(16);
        subs->Child(Div(a)->W(1)->H(kFill)->Bg(th.border));
        El* subCol = Div(a)->FlexCol()->Grow();
        for (int j = 0; j < 3; j++) {
            El* sub = Div(a)
                          ->FlexRow()
                          ->W(kFill)
                          ->H(32)
                          ->PadX(12)
                          ->Gap(8)
                          ->ItemsCenter()
                          ->JustifyBetween()
                          ->Radius(th.radius)
                          ->HoverBg(th.muted);
            sub->Child(
                StoryTxt(cx, Str(kPlaygroundSubs[j]), 16, th.foreground));
            if (j == 0) {
                // The first child carries a switch, as the Rust story shows.
                sub->Child(component::Switch::New(cx, StrL("sidebar-history"))
                               ->Checked(self->historySwitch)
                               ->WithSize(UiSize::XSmall)
                               ->OnClick(Listen(cx, &ToggleHistory))
                               ->IntoEl());
            }
            sub->Click(HashClickId(StoryFmt(cx, "sidebar-sub-%d", j)))
                ->OnClick(ListenerArg(pickSub, j));
            subCol->Child(sub);
        }
        subs->Child(subCol);
        side->Child(subs);
    }

    side->Child(GroupLabel(cx, "Projects"));
    for (int i = 0; i < 3; i++) {
        const SidebarItem& item = kProjects[i];
        El* row = Div(a)
                      ->FlexRow()
                      ->W(kFill)
                      ->H(32)
                      ->PadX(8)
                      ->Gap(8)
                      ->ItemsCenter()
                      ->Radius(th.radius)
                      ->HoverBg(th.muted);
        Rgba fg = i == 2 ? th.mutedFg : th.foreground;
        row->Child(IconEl(a, item.icon, 16)->Fg(fg));
        row->Child(
            Div(a)->Grow()->Child(StoryTxt(cx, Str(item.label), 16, fg)));
        if (i == 0) {
            row->Child(component::Badge::New(cx)
                           ->Dot()
                           ->Child(IconEl(a, IconName::Bell, 16)->Fg(fg))
                           ->IntoEl());
        } else if (i == 1) {
            row->Child(IconEl(a, IconName::Settings2, 16)->Fg(fg));
        }
        side->Child(row);
    }

    side->Child(Div(a)->Grow());
    El* footer =
        Div(a)->FlexRow()->W(kFill)->H(40)->PadX(8)->Gap(8)->ItemsCenter();
    footer->Child(IconEl(a, IconName::CircleUser, 16)->Fg(th.foreground));
    footer->Child(Div(a)->Grow()->Child(
        StoryTxt(cx, StrL("Jason Lee"), 16, th.foreground)));
    footer->Child(IconEl(a, IconName::ChevronsUpDown, 16)->Fg(th.foreground));
    side->Child(footer);
    frame->Child(side);

    // The content pane: breadcrumb, heading with the Options menu, metric
    // cards and the activity list.
    El* content = Div(a)->FlexCol()->Grow()->H(kFill)->Pad(16)->Gap(16);
    El* crumbs = Div(a)->FlexRow()->W(kFill)->Gap(8)->ItemsCenter();
    crumbs->Child(IconEl(a, IconName::PanelLeft, 16)->Fg(th.foreground));
    crumbs->Child(component::Separator::Vertical(cx)->IntoEl()->H(16));
    crumbs->Child(component::Breadcrumb::New(cx)
                      ->Item(StrL("Breadcrumb"))
                      ->Item(StrL("Home"))
                      ->Item(StrL("Playground"))
                      ->IntoEl());
    content->Child(crumbs);

    El* headRow =
        Div(a)->FlexRow()->W(kFill)->Gap(16)->ItemsStart()->JustifyBetween();
    El* headText = Div(a)->FlexCol()->Gap(4);
    headText->Child(
        StoryTxt(cx, Str(kPlatform[self->active].label), 24, th.foreground)
            ->Semibold());
    headText->Child(StoryTxt(
        cx, StrL("A quick view of your workspace activity."), 14, th.mutedFg));
    headRow->Child(headText);
    El* optGroup = StoryToolbarGroup(cx);
    StoryToolbarOpt opts[6] = {
        {"Icon mode", self->collapsible == 0, SidebarOptIcon},
        {"Offcanvas mode", self->collapsible == 1, SidebarOptOffcanvas},
        {"Fixed mode", self->collapsible == 2, SidebarOptFixed},
        {"Right Side", self->rightSide, SidebarOptRight},
        {"Click to Open", self->clickToOpen, SidebarOptClickToOpen},
        {"Dynamic Children", self->dynamicChildren, SidebarOptDynamic},
    };
    optGroup->Child(StoryToolbarDropdown(
        cx, StrL("sidebar-options"), StrL("Options"), self->optionsOpen,
        Listen(cx, &ToggleSidebarOptions), opts, 6,
        Listen(cx, &SidebarOptionAct)));
    headRow->Child(optGroup);
    content->Child(headRow);

    struct Metric {
        const char* label;
        const char* value;
        const char* detail;
    };
    static const Metric kMetrics[] = {
        {"Active projects", "12", "+2 this week"},
        {"Team members", "28", "4 online"},
        {"Tasks completed", "84%", "+6% this month"},
    };
    El* metrics = Div(a)->FlexRow()->W(kFill)->Gap(12);
    for (int i = 0; i < 3; i++) {
        El* card = Div(a)
                       ->FlexCol()
                       ->Grow()
                       ->Gap(8)
                       ->Pad(16)
                       ->Radius(th.radiusLg)
                       ->Border(1, th.border);
        card->Child(StoryTxt(cx, Str(kMetrics[i].label), 16, th.mutedFg));
        card->Child(StoryTxt(cx, Str(kMetrics[i].value), 24, th.foreground)
                        ->Semibold());
        card->Child(StoryTxt(cx, Str(kMetrics[i].detail), 12, th.mutedFg));
        metrics->Child(card);
    }
    content->Child(metrics);

    struct Activity {
        IconName icon;
        const char* title;
        const char* time;
    };
    static const Activity kActivity[] = {
        {IconName::CircleCheck, "Design review completed", "12 minutes ago"},
        {IconName::File, "Project brief updated", "1 hour ago"},
        {IconName::CircleUser, "Maya joined the workspace", "3 hours ago"},
    };
    El* activity = Div(a)
                       ->FlexCol()
                       ->W(kFill)
                       ->Grow()
                       ->Radius(th.radiusLg)
                       ->Border(1, th.border);
    El* actHead = Div(a)
                      ->FlexRow()
                      ->W(kFill)
                      ->PadX(16)
                      ->PadY(8)
                      ->ItemsCenter()
                      ->JustifyBetween();
    actHead->Child(StoryTxt(cx, StrL("Recent activity"), 16, th.foreground)
                       ->Medium());
    actHead->Child(StoryTxt(cx, StrL("Today"), 12, th.mutedFg));
    activity->Child(actHead);
    activity->Child(component::Separator::Horizontal(cx)->IntoEl());
    for (int i = 0; i < 3; i++) {
        El* row = Div(a)
                      ->FlexRow()
                      ->W(kFill)
                      ->PadX(16)
                      ->PadY(12)
                      ->Gap(12)
                      ->ItemsCenter();
        row->Child(
            Div(a)
                ->W(32)
                ->H(32)
                ->ItemsCenter()
                ->JustifyCenter()
                ->Radius(16)
                ->Bg(th.muted)
                ->Child(IconEl(a, kActivity[i].icon, 16)->Fg(th.foreground)));
        El* col = Div(a)->FlexCol()->Grow()->Gap(2);
        col->Child(StoryTxt(cx, Str(kActivity[i].title), 14, th.foreground));
        col->Child(StoryTxt(cx, Str(kActivity[i].time), 12, th.mutedFg));
        row->Child(col);
        activity->Child(row);
    }
    content->Child(activity);
    frame->Child(content);

    El* page = Div(a)->FlexCol()->W(kFill);
    page->Child(frame);
    return page;
}

STORY_PAGE(StorySidebar, SidebarStory);
