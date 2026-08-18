#include "gpui.h"

using namespace gpui;

enum CollapseMode : int {
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
    static El* Render(SidebarApp* self, Ctx* cx);
    int mode = CollIcon;
    bool collapsed = false;
    bool projOpen = true;
};

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

static void ToggleCollapse(SidebarApp* app, Ctx* cx, const ClickEvent*) {
    app->collapsed = !app->collapsed;
    Notify(cx);
}

static void ToggleProjects(SidebarApp* app, Ctx* cx, const ClickEvent*) {
    app->projOpen = !app->projOpen;
    Notify(cx);
}

static void SetMode(SidebarApp* app, Ctx* cx, const ClickEvent*,
                    intptr_t mode) {
    app->mode = (int)mode;
    Notify(cx);
}

static El* MenuItem(Ctx* cx, Listener onClick, IconName icon, const char* label,
                    bool active, bool iconOnly) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* row = Div(a)
                  ->FlexRow()
                  ->H(36)
                  ->PadX(8)
                  ->Gap(8)
                  ->ItemsCenter()
                  ->Radius(6)
                  ->OnClick(onClick);
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

El* SidebarApp::Render(SidebarApp* app, Ctx* cx) {
    Arena* frame = cx->a;

    const Theme& th = cx->theme();
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
        side->Child(MenuItem(cx, Listener{}, IconName::LayoutDashboard,
                             "Dashboard", true, iconCollapsed));
        side->Child(MenuItem(cx, Listener{}, IconName::Inbox, "Inbox", false,
                             iconCollapsed));
        side->Child(MenuItem(cx, Listener{}, IconName::Calendar, "Calendar",
                             false, iconCollapsed));
        side->Child(MenuItem(cx, Listen(cx, &ToggleProjects), IconName::Folder,
                             "Projects", false, iconCollapsed));
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
        side->Child(MenuItem(cx, Listener{}, IconName::Settings, "Settings",
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
                            StrL(iconCollapsed || hide ? ">" : "<"))
                       ->OnClick(Listen(cx, &ToggleCollapse)));
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
            ->Child(ButtonSmall(frame, 0, StrL("Icon"), BtnKind::Default,
                                app->mode == CollIcon)
                        ->OnClick(Listen(cx, &SetMode, CollIcon)))
            ->Child(ButtonSmall(frame, 0, StrL("Offcanvas"), BtnKind::Default,
                                app->mode == CollOffcanvas)
                        ->OnClick(Listen(cx, &SetMode, CollOffcanvas)))
            ->Child(ButtonSmall(frame, 0, StrL("None"), BtnKind::Default,
                                app->mode == CollNone)
                        ->OnClick(Listen(cx, &SetMode, CollNone))));
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

int GpuiMain(int argc, char** argv) {
    (void)argc;
    (void)argv;
    App* app = AppNew();
    Entity<SidebarApp> view = EntityNew<SidebarApp>(app);
    SidebarApp* self = view.Get(app);
    (void)self;
    ThemeSet(app, ThemeMode::Light);
    AssetsClear();
    AssetsAddDefaultRoots(Str{});
    AssetsAddRoot(StrL("assets"));
    WinOpts opts = {};
    WindowOpenView(app, StrL("Sidebar C++"), 900, 620, view.id, opts);
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
