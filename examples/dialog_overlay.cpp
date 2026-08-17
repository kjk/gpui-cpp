#include "gpui.h"

using namespace gpui;

enum {
    ClickOpenDialog = 1,
    ClickOpenSheet = 2,
    ClickDismiss = 3,
    ClickMenuOpen = 10,
    ClickMenuDelete = 11,
    ClickMenuExport = 12,
    ClickMenuInfo = 13,
    ClickCtx = 20,
};

struct DialogApp {
    int overlay = 0; // 1 dialog, 2 sheet
};

static void OnInit(AppHost* host) {
    ThemeSet(ThemeMode::Light);
    host->menu.nItems = 4;
    strncpy_s(host->menu.items[0], "Open", _TRUNCATE);
    strncpy_s(host->menu.items[1], "Delete", _TRUNCATE);
    strncpy_s(host->menu.items[2], "Export", _TRUNCATE);
    strncpy_s(host->menu.items[3], "Info", _TRUNCATE);
    host->menu.clickBase = ClickMenuOpen;
}

static void OnClick(AppHost* host, int id) {
    auto* app = (DialogApp*)host->user;
    if (id == ClickOpenDialog) {
        app->overlay = 1;
        host->menu.open = false;
    } else if (id == ClickOpenSheet) {
        app->overlay = 2;
        host->menu.open = false;
    } else if (id == ClickDismiss || id == ClickCtx) {
        if (app->overlay) {
            app->overlay = 0;
        }
        host->menu.open = false;
    } else if (id >= ClickMenuOpen && id <= ClickMenuInfo) {
        logf("menu %d", id);
        host->menu.open = false;
    }
}

static void OnMouseDown(AppHost* host, float x, float y, int button) {
    if (button == 2) {
        host->menu.open = true;
        host->menu.x = x;
        host->menu.y = y;
    }
}

static El* MdLine(Arena* a, Str s, Rgba c) {
    return TextEl(a, s)->Font(14)->Fg(c)->Wrap();
}

static El* OnRender(AppHost* host, Arena* frame, WinSize size) {
    auto* app = (DialogApp*)host->user;
    const Theme& th = ThemeNow();

    El* bar = Div(frame)
                  ->FlexRow()
                  ->H(34)
                  ->PadX(12)
                  ->ItemsCenter()
                  ->Bg(th.titleBar)
                  ->Child(TextEl(frame, StrL("Dialog & Sheet"))
                              ->Font(14)
                              ->Fg(th.foreground));

    El* hoverBox =
        Div(frame)
            ->H(160)
            ->W(kFill)
            ->ItemsCenter()
            ->JustifyCenter()
            ->FlexCol()
            ->Gap(4)
            ->Border(1, th.foreground)
            ->Dashed()
            ->HoverBg(RgbaOpacity(th.yellow, 0.2f))
            ->Click(ClickCtx)
            ->Child(TextEl(frame, StrL("Hover test here."))
                        ->Font(14)
                        ->Fg(th.foreground))
            ->Child(TextEl(frame, StrL("Right click to show Context Menu"))
                        ->Font(14)
                        ->Fg(th.foreground));

    El* body =
        Div(frame)
            ->FlexCol()
            ->Grow()
            ->Pad(32)
            ->Gap(8)
            ->Child(Div(frame)
                        ->FlexRow()
                        ->Gap(16)
                        ->Child(ButtonEl(frame, ClickOpenDialog,
                                         StrL("Open dialog"), BtnKind::Outline))
                        ->Child(ButtonEl(frame, ClickOpenSheet,
                                         StrL("Open Sheet"), BtnKind::Outline)))
            ->Child(MdLine(frame,
                           StrL("Background text behind the modals. While a "
                                "dialog or sheet is open, "
                                "a selection started inside it must not extend "
                                "onto this paragraph."),
                           th.foreground))
            ->Child(hoverBox);

    El* root = Div(frame)
                   ->FlexCol()
                   ->SizeFull()
                   ->Bg(Rgb(255, 255, 255))
                   ->Child(bar)
                   ->Child(body);

    if (app->overlay) {
        const char* title =
            app->overlay == 1 ? "Selectable dialog" : "Selectable Sheet";
        const char* msg =
            "Select this text, then drag the mouse out of the dialog over the "
            "paragraph behind it. "
            "The text behind must NOT get selected.";
        El* panel = Div(frame)
                        ->W(app->overlay == 1 ? 420.f : size.dipW * 0.5f)
                        ->Pad(20)
                        ->Gap(12)
                        ->FlexCol()
                        ->Radius(8)
                        ->Bg(th.background)
                        ->Border(1, th.border)
                        ->Child(TextEl(frame, Str(title))
                                    ->Font(18)
                                    ->Semibold()
                                    ->Fg(th.foreground))
                        ->Child(TextEl(frame, Str(msg))
                                    ->Font(14)
                                    ->Fg(th.foreground)
                                    ->Wrap())
                        ->Child(ButtonEl(frame, ClickDismiss, StrL("Close"),
                                         BtnKind::Primary));
        El* backdrop = Div(frame)
                           ->Absolute()
                           ->Top(0)
                           ->Left(0)
                           ->W(size.dipW)
                           ->H(size.dipH)
                           ->Bg(Rgba8(0, 0, 0, 80))
                           ->Click(ClickDismiss)
                           ->ItemsCenter()
                           ->JustifyCenter()
                           ->Child(panel);
        if (app->overlay == 2) {
            backdrop->ItemsStart()->JustifyCenter();
            panel->W(size.dipW);
        }
        root->Child(backdrop);
    }

    if (host->menu.open) {
        El* menu = Div(frame)
                       ->Absolute()
                       ->Left(host->menu.x)
                       ->Top(host->menu.y)
                       ->W(140)
                       ->FlexCol()
                       ->Bg(th.background)
                       ->Border(1, th.border)
                       ->Radius(6);
        const char* labels[] = {"Open", "Delete", "Export", "Info"};
        for (int i = 0; i < 4; i++) {
            menu->Child(ButtonEl(frame, ClickMenuOpen + i, Str(labels[i]),
                                 BtnKind::Default));
        }
        root->Child(menu);
    }

    return root;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    static DialogApp app;
    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    hooks.onClick = OnClick;
    hooks.onMouseDown = OnMouseDown;
    return RunApp(L"Dialog Overlay", 800, 600, hooks, &app);
}
