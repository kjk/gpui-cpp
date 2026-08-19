#include "gpui.h"

using namespace gpui;

enum {
    ClickOpenDialog = 1,
    ClickOpenSheet = 2,
    ClickCloseOverlay = 3,
    ClickHoverArea = 4,
    ClickOverlayScrim = 5,
    ClickOverlayPanel = 6,
    ClickMenuBase = 10, // + item index
};

enum {
    OverlayNone = 0,
    OverlayDialog = 1,
    OverlaySheet = 2,
};

// dialog.rs: 448 wide, a tenth of the viewport down, and an overlay that is
// present for hit testing but not painted. sheet.rs: 350 from the right, over
// a visible overlay.
static const float kDialogWidth = 448;
static const float kSheetWidth = 350;

static const char* kMenuItems[] = {"Open", "Delete", "Export", "Info"};
static const int kMenuCount = 4;

struct DialogApp {
    static El* Render(DialogApp* self, Ctx* cx);

    int overlay = OverlayNone;
    bool menuOpen = false;
    float menuX = 0;
    float menuY = 0;
    // Text selection, as offsets into the frame's selectable text; -1 is no
    // selection. `selecting` is true between press and release.
    int selA = -1;
    int selB = -1;
    bool selecting = false;
};

static void OnMouse(DialogApp* app, Ctx* cx, const MouseEvent* ev) {
    if (ev->kind == MouseKind::Up) {
        app->selecting = false;
        return;
    }
    if (ev->kind == MouseKind::Move) {
        if (!app->selecting) {
            return;
        }
        // `nearest` clamps to the closest selectable run, so dragging past
        // the end of the dialog's text stops there instead of reaching the
        // paragraph behind it.
        int moveOff = TextHitOffsetAt(&cx->win->paint, ev->x, ev->y, true);
        if (moveOff >= 0) {
            app->selB = moveOff;
        }
        return;
    }
    // The context menu belongs to the dashed area, so a right click anywhere
    // else just dismisses it.
    if (ev->button == 2) {
        app->menuOpen = app->overlay == OverlayNone;
        app->menuX = ev->x;
        app->menuY = ev->y;
        Notify(cx);
        return;
    }
    if (app->menuOpen) {
        app->menuOpen = false;
        Notify(cx);
    }
    // A double click takes the word under the pointer and a triple click the
    // whole run, the way points_for_multi_click does in text_selection.rs.
    // The button stays down on a word, but the drag does not extend it: the
    // selection is already the unit the user asked for.
    int wordA = 0;
    int wordB = 0;
    if (TextMultiClickRange(&cx->win->paint, ev->x, ev->y, ev->clickCount,
                            &wordA, &wordB)) {
        app->selA = wordA;
        app->selB = wordB;
        app->selecting = false;
        return;
    }
    int off = TextHitOffsetAt(&cx->win->paint, ev->x, ev->y, false);
    if (off >= 0) {
        app->selA = off;
        app->selB = off;
        app->selecting = true;
        return;
    }
    app->selA = -1;
    app->selB = -1;
    app->selecting = false;
}

static void OpenOverlay(DialogApp* app, Ctx* cx, const ClickEvent*,
                        intptr_t kind) {
    app->overlay = (int)kind;
    app->menuOpen = false;
    Notify(cx);
}

static void CloseOverlay(DialogApp* app, Ctx* cx, const ClickEvent*) {
    app->overlay = OverlayNone;
    Notify(cx);
}

static void MenuPicked(DialogApp* app, Ctx* cx, const ClickEvent*,
                       intptr_t ix) {
    logf("menu %d", (int)ix);
    app->menuOpen = false;
    Notify(cx);
}

// The header both overlays share: the title, and the close button opposite.
static El* OverlayHeader(Ctx* cx, Str title) {
    Arena* a = cx->a;
    const Theme& th = cx->theme();
    El* row = Div(a)->FlexRow()->W(kFill)->ItemsCenter()->JustifyBetween();
    row->Child(TextEl(a, title)->Font(15)->Semibold()->Fg(th.foreground));
    row->Child(Div(a)
                   ->W(20)
                   ->H(20)
                   ->ItemsCenter()
                   ->JustifyCenter()
                   ->Radius(4)
                   ->HoverBg(th.muted)
                   ->Click(ClickCloseOverlay)
                   ->OnClick(Listen(cx, &CloseOverlay))
                   ->Child(IconEl(a, IconName::X, 14)->Fg(th.mutedFg)));
    return row;
}

El* DialogApp::Render(DialogApp* app, Ctx* cx) {
    Arena* frame = cx->a;
    WinSize size = WindowSize(cx->win);
    const Theme& th = cx->theme();
    cx->win->paint.selA = app->selA;
    cx->win->paint.selB = app->selB;

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
            // border_dashed over black, and gpui::yellow() at a fifth on
            // hover: raw colors, like the rest of this example.
            ->Border(1, Rgb(0, 0, 0))
            ->Dashed()
            ->HoverBg(RgbaOpacity(Rgb(255, 255, 0), 0.2f))
            ->Click(ClickHoverArea)
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
                                         StrL("Open dialog"), BtnKind::Outline)
                                    ->OnClick(Listen(cx, &OpenOverlay,
                                                     OverlayDialog)))
                        ->Child(ButtonEl(frame, ClickOpenSheet,
                                         StrL("Open Sheet"), BtnKind::Outline)
                                    ->OnClick(Listen(cx, &OpenOverlay,
                                                     OverlaySheet))))
            ->Child(component::TextView::New(
                        cx, StrL("**Background text** behind the modals. "
                                 "While a dialog or sheet is open, a "
                                 "selection started inside it must not "
                                 "extend onto this paragraph."))
                        ->Selectable(app->overlay == OverlayNone)
                        ->IntoEl())
            ->Child(hoverBox);

    El* root = Div(frame)
                   ->FlexCol()
                   ->SizeFull()
                   ->Bg(Rgb(255, 255, 255))
                   ->Child(bar)
                   ->Child(body);

    if (app->overlay != OverlayNone) {
        bool dialog = app->overlay == OverlayDialog;
        // The dialog's overlay is invisible, the sheet's is painted.
        El* scrim = Div(frame)
                        ->Absolute()
                        ->Top(0)
                        ->Left(0)
                        ->W(size.dipW)
                        ->H(size.dipH)
                        ->Click(ClickOverlayScrim)
                        ->OnClick(Listen(cx, &CloseOverlay));
        if (!dialog) {
            scrim->Bg(Rgba8(0, 0, 0, 13));
        }
        root->Child(scrim);

        // The panel takes the click itself; without a hit rect of its own
        // every press inside it reached the scrim and dismissed the overlay.
        El* panel = Div(frame)
                        ->Absolute()
                        ->FlexCol()
                        ->Gap(8)
                        ->Bg(th.background)
                        ->Click(ClickOverlayPanel);
        if (dialog) {
            panel->W(kDialogWidth)
                ->Left((size.dipW - kDialogWidth) * 0.5f)
                ->Top(size.dipH / 10.f)
                ->Pad(16)
                ->Radius(8)
                // GPUI drops a shadow here; a hairline is the nearest thing
                // this runtime draws.
                ->Border(1, th.border);
        } else {
            panel->W(kSheetWidth)
                ->Left(size.dipW - kSheetWidth)
                ->Top(0)
                ->H(size.dipH)
                ->Pad(16)
                ->BorderL(1, th.border);
        }
        panel->Child(OverlayHeader(
            cx, dialog ? StrL("Selectable dialog") : StrL("Selectable Sheet")));
        panel->Child(
            component::TextView::New(
                cx, dialog ? StrL("Select **this** text, then drag the mouse "
                                  "*out of the dialog* over the paragraph "
                                  "behind it. The text behind must NOT get "
                                  "selected")
                           : StrL("Select **this** text, then drag the mouse "
                                  "*out of the sheet* over the paragraph "
                                  "behind it. The text behind must NOT get "
                                  "selected"))
                ->Selectable()
                ->IntoEl());
        root->Child(panel);
    }

    if (app->menuOpen) {
        // PopupMenu: a card of plain rows at the cursor, no per-row frame.
        El* menu = Div(frame)
                       ->Absolute()
                       ->Left(app->menuX)
                       ->Top(app->menuY)
                       ->W(130)
                       ->PadY(4)
                       ->FlexCol()
                       ->Bg(th.background)
                       ->Border(1, th.border)
                       ->Radius(6);
        for (int i = 0; i < kMenuCount; i++) {
            menu->Child(Div(frame)
                            ->FlexRow()
                            ->W(kFill)
                            ->H(28)
                            ->PadX(12)
                            ->ItemsCenter()
                            ->HoverBg(th.muted)
                            ->Click(ClickMenuBase + i)
                            ->OnClick(Listen(cx, &MenuPicked, i))
                            ->Child(TextEl(frame, Str(kMenuItems[i]))
                                        ->Font(14)
                                        ->Fg(th.foreground)));
        }
        root->Child(menu);
    }

    return root;
}

int GpuiMain(int argc, char** argv) {
    (void)argc;
    (void)argv;
    App* app = AppNew();
    Entity<DialogApp> view = EntityNew<DialogApp>(app);
    ThemeSet(app, ThemeMode::Light);
    WinOpts opts = {};
    Window* win = WindowOpenView(app, StrL("Dialog Overlay C++"), 800, 600,
                                 view.id, opts);
    WindowOnMouse(win, ListenTo(view, &OnMouse));
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
