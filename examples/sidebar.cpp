#include "gpui/Gpui.h"
#include "gpui/Assets.h"

enum Collapsible : int {
    CollIcon = 0,
    CollOffcanvas = 1,
    CollNone = 2
};

enum {
    ClickToggle = 1,
    ClickModeIcon = 2,
    ClickModeOff = 3,
    ClickModeNone = 4,
    ClickDash = 10,
    ClickInbox = 11,
    ClickCal = 12,
    ClickProj = 13,
    ClickSet = 14,
};

struct SidebarApp {
    int mode = CollIcon;
    bool collapsed = false;
    bool projOpen = true;
};

static void OnInit(AppHost* host) {
    ThemeSet(ThemeMode::Light);
    AssetsClear();
    AssetsAddDefaultRoots(Str{});
    AssetsAddRoot(StrL("assets"));
    (void)host;
}

static void OnClick(AppHost* host, int id) {
    auto* app = (SidebarApp*)host->user;
    if (id == ClickToggle) {
        app->collapsed = !app->collapsed;
    } else if (id == ClickModeIcon) {
        app->mode = CollIcon;
    } else if (id == ClickModeOff) {
        app->mode = CollOffcanvas;
    } else if (id == ClickModeNone) {
        app->mode = CollNone;
    } else if (id == ClickProj) {
        app->projOpen = !app->projOpen;
    }
}

static const char* Description(const SidebarApp* app) {
    if (app->mode == CollOffcanvas) {
        return "The sidebar releases its layout width when collapsed and keeps "
               "hidden controls out of keyboard "
               "navigation, matching shadcn's collapsible=\"offcanvas\" "
               "behavior.";
    }
    if (app->mode == CollNone) {
        return "The sidebar ignores the collapsed state and remains expanded, "
               "matching shadcn's collapsible=\"none\" "
               "behavior.";
    }
    return "The sidebar collapses to icon width, matching shadcn's "
           "collapsible=\"icon\" behavior.";
}

static El* MenuItem(Arena* a, int id, IconName icon, const char* label,
                    bool active, bool iconOnly) {
    const Theme& th = ThemeNow();
    El* row = Div(a)
                  ->FlexRow()
                  ->H(36)
                  ->PadX(8)
                  ->Gap(8)
                  ->ItemsCenter()
                  ->Radius(6)
                  ->Click(id);
    if (active) {
        row->Bg(th.secondary);
    }
    row->HoverBg(th.secondaryHover);
    row->Child(IconEl(a, icon, 16)->Fg(th.sidebarFg));
    if (!iconOnly) {
        row->Child(TextEl(a, Str(label))->Font(14)->Fg(th.sidebarFg));
    }
    return row;
}

static El* OnRender(AppHost* host, Arena* frame, WinSize size) {
    (void)size;
    auto* app = (SidebarApp*)host->user;
    const Theme& th = ThemeNow();
    bool iconCollapsed = app->collapsed && app->mode == CollIcon;
    bool hide = app->collapsed && app->mode == CollOffcanvas;
    bool showToggle = app->mode != CollNone;

    El* root = Div(frame)->FlexRow()->SizeFull()->Bg(th.background);

    if (!hide) {
        float sw = iconCollapsed ? 56.f : 240.f;
        El* side = Div(frame)
                       ->FlexCol()
                       ->W(sw)
                       ->H(kFill)
                       ->Bg(th.sidebar)
                       ->Pad(8)
                       ->Gap(8)
                       ->Shrink0();

        El* header = Div(frame)->FlexRow()->H(40)->Gap(8)->ItemsCenter();
        El* logo = Div(frame)
                       ->W(iconCollapsed ? 16.f : 32.f)
                       ->H(iconCollapsed ? 16.f : 32.f)
                       ->Radius(th.radius)
                       ->Bg(iconCollapsed ? th.sidebar : th.sidebarPrimary)
                       ->ItemsCenter()
                       ->JustifyCenter()
                       ->Child(IconEl(frame, IconName::GalleryVerticalEnd, 16)
                                   ->Fg(iconCollapsed ? th.foreground
                                                      : th.sidebarPrimaryFg));
        header->Child(logo);
        if (!iconCollapsed) {
            header->Child(Div(frame)
                              ->FlexCol()
                              ->Grow()
                              ->Child(TextEl(frame, StrL("Acme Inc"))
                                          ->Font(14)
                                          ->Fg(th.sidebarFg))
                              ->Child(TextEl(frame, StrL("Enterprise"))
                                          ->Font(12)
                                          ->Fg(th.mutedFg)));
        }
        side->Child(header);
        side->Child(
            TextEl(frame, iconCollapsed ? StrL("") : StrL("Application"))
                ->Font(12)
                ->Fg(th.mutedFg));
        side->Child(MenuItem(frame, ClickDash, IconName::LayoutDashboard,
                             "Dashboard", true, iconCollapsed));
        side->Child(MenuItem(frame, ClickInbox, IconName::Inbox, "Inbox", false,
                             iconCollapsed));
        side->Child(MenuItem(frame, ClickCal, IconName::Calendar, "Calendar",
                             false, iconCollapsed));
        side->Child(MenuItem(frame, ClickProj, IconName::Folder, "Projects",
                             false, iconCollapsed));
        if (app->projOpen && !iconCollapsed) {
            side->Child(Div(frame)
                            ->PadL(28)
                            ->FlexCol()
                            ->Gap(2)
                            ->Child(TextEl(frame, StrL("Design"))
                                        ->Font(13)
                                        ->Fg(th.sidebarFg))
                            ->Child(TextEl(frame, StrL("Engineering"))
                                        ->Font(13)
                                        ->Fg(th.sidebarFg))
                            ->Child(TextEl(frame, StrL("Marketing"))
                                        ->Font(13)
                                        ->Fg(th.sidebarFg)));
        }
        side->Child(MenuItem(frame, ClickSet, IconName::Settings, "Settings",
                             false, iconCollapsed));
        El* foot = Div(frame)->FlexRow()->Gap(8)->ItemsCenter()->Grow();
        // spacer then footer at bottom: put grow spacer
        side->Child(Div(frame)->Grow());
        foot = Div(frame)->FlexRow()->Gap(8)->Pad(8)->ItemsCenter();
        foot->Child(IconEl(frame, IconName::CircleUser, 16)->Fg(th.sidebarFg));
        if (!iconCollapsed) {
            foot->Child(
                TextEl(frame, StrL("Jason Lee"))->Font(14)->Fg(th.sidebarFg));
        }
        side->Child(foot);
        root->Child(side);
    }

    El* main = Div(frame)->FlexCol()->Grow()->H(kFill)->Pad(16)->Gap(16);
    El* top = Div(frame)->FlexRow()->ItemsCenter()->Gap(12);
    if (showToggle) {
        top->Child(ButtonEl(frame, ClickToggle,
                            StrL(iconCollapsed || hide ? ">" : "<")));
    }
    top->Child(TextEl(frame, StrL("Sidebar collapsible modes"))
                   ->Font(16)
                   ->Bold()
                   ->Fg(th.foreground));
    main->Child(top);
    main->Child(
        Div(frame)
            ->FlexRow()
            ->ItemsCenter()
            ->Gap(8)
            ->Child(TextEl(frame, StrL("Mode:"))->Font(14)->Fg(th.foreground))
            ->Child(ButtonSmall(frame, ClickModeIcon, StrL("Icon"),
                                BtnKind::Default, app->mode == CollIcon))
            ->Child(ButtonSmall(frame, ClickModeOff, StrL("Offcanvas"),
                                BtnKind::Default, app->mode == CollOffcanvas))
            ->Child(ButtonSmall(frame, ClickModeNone, StrL("None"),
                                BtnKind::Default, app->mode == CollNone)));
    main->Child(Div(frame)
                    ->Grow()
                    ->Radius(th.radius)
                    ->Border(1, th.border)
                    ->Pad(20)
                    ->Child(TextEl(frame, Str(Description(app)))
                                ->Font(14)
                                ->Fg(th.foreground)
                                ->Wrap()));
    root->Child(main);
    return root;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    static SidebarApp app;
    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    hooks.onClick = OnClick;
    return RunApp(L"Sidebar", 900, 620, hooks, &app);
}
