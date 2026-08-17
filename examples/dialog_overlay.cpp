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
    static El* Render(DialogApp* self, Ctx* cx);
    int overlay = 0; // 1 dialog, 2 sheet
};

static void OnClick(DialogApp* app, Ctx* cx, const ClickEvent* ev) {
    (void)cx;
    int id = ev->id;
    if (id == ClickOpenDialog) {
        app->overlay = 1;
        cx->win->menu.open = false;
    } else if (id == ClickOpenSheet) {
        app->overlay = 2;
        cx->win->menu.open = false;
    } else if (id == ClickDismiss || id == ClickCtx) {
        if (app->overlay) {
            app->overlay = 0;
        }
        cx->win->menu.open = false;
    } else if (id >= ClickMenuOpen && id <= ClickMenuInfo) {
        logf("menu %d", id);
        cx->win->menu.open = false;
    }
}

static void OnMouse(DialogApp* app, Ctx* cx, const MouseEvent* ev) {
    (void)app;
    Window* host = cx->win;
    if (ev->kind != MouseKind::Down) {
        return;
    }
    float x = ev->x;
    float y = ev->y;
    int button = ev->button;
    if (button == 2) {
        cx->win->menu.open = true;
        cx->win->menu.x = x;
        cx->win->menu.y = y;
    }
}

static El* MdLine(Arena* a, Str s, Rgba c) {
    return TextEl(a, s)->Font(14)->Fg(c)->Wrap();
}

El* DialogApp::Render(DialogApp* app, Ctx* cx) {
    Arena* frame = cx->a;
    Window* host = cx->win;

    WinSize size = WindowSize(cx->win);
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

    if (cx->win->menu.open) {
        El* menu = Div(frame)
                       ->Absolute()
                       ->Left(cx->win->menu.x)
                       ->Top(cx->win->menu.y)
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
    App* app = AppNew();
    Entity<DialogApp> view = EntityNew<DialogApp>(app);
    DialogApp* self = view.Get(app);
    (void)self;
    ThemeSet(ThemeMode::Light);
    AppWinOpts opts = {};
    Window* win =
        WindowOpenView(app, L"Dialog Overlay", 800, 600, view.id, opts);
    WindowOnClick(win, ListenTo(view, &OnClick));
    WindowOnMouse(win, ListenTo(view, &OnMouse));
    win->menu.nItems = 4;
    strncpy_s(win->menu.items[0], "Open", _TRUNCATE);
    strncpy_s(win->menu.items[1], "Delete", _TRUNCATE);
    strncpy_s(win->menu.items[2], "Export", _TRUNCATE);
    strncpy_s(win->menu.items[3], "Info", _TRUNCATE);
    win->menu.clickBase = ClickMenuOpen;
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
