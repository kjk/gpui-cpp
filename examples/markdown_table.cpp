#include "gpui.h"

using namespace gpui;

enum TableMode : int {
    ModeWrap = 0,
    ModeAdaptive = 1,
    ModeNowrap = 2
};

struct MdApp {
    static El* Render(MdApp* self, Ctx* cx);
    int mode = ModeAdaptive;
    float scroll = 0;
    char source[16000];
};

static void OnWheel(MdApp* app, Ctx* cx, const WheelEvent* ev) {
    (void)cx;
    app->scroll -= ev->delta;
    if (app->scroll < 0) {
        app->scroll = 0;
    }
}

static void CycleMode(MdApp* app, Ctx* cx, const ClickEvent*) {
    app->mode = (app->mode + 1) % 3;
    Notify(cx);
}

static const char* ModeLabel(int mode) {
    if (mode == ModeWrap) {
        return "Table: wrap";
    }
    if (mode == ModeNowrap) {
        return "Table: scroll (nowrap)";
    }
    return "Table: scroll (adaptive)";
}

static float ModeColumnWidth(int mode) {
    if (mode == ModeNowrap) {
        return 180.f;
    }
    return mode == ModeAdaptive ? 140.f : 110.f;
}

El* MdApp::Render(MdApp* app, Ctx* cx) {
    Arena* frame = cx->a;

    const Theme& th = cx->theme();
    El* bar = Div(frame)
                  ->FlexRow()
                  ->Pad(8)
                  ->Gap(8)
                  ->BorderT(0, th.border)
                  ->Child(ButtonEl(frame, 1, Str(ModeLabel(app->mode)))
                              ->OnClick(Listen(cx, &CycleMode)));
    El* doc = component::TextView::New(cx, Str(app->source))
                  ->TableColumnWidth(ModeColumnWidth(app->mode))
                  ->IntoEl();
    El* body = Div(frame)
                   ->Grow()
                   ->ClipY()
                   ->ScrollY(app->scroll)
                   ->Child(Div(frame)->FlexCol()->Pad(16)->Child(doc));
    return Div(frame)
        ->FlexCol()
        ->SizeFull()
        ->Bg(th.background)
        ->Child(bar)
        ->Child(Div(frame)->H(1)->Bg(th.border))
        ->Child(body);
}

int GpuiMain(int argc, char** argv) {
    (void)argc;
    (void)argv;
    App* app = AppNew();
    ThemeSet(app, ThemeMode::Light);
    AssetsClear();
    AssetsAddDefaultRoots(StrL("markdown_table"));
    AssetsAddRoot(StrL("assets/markdown_table"));

    Entity<MdApp> view = EntityNew<MdApp>(app);
    MdApp* self = view.Get(app);
    TempStr md = AssetsLoadTextTemp(StrL("report.md"));
    if (md.s && md.len > 0) {
        int n = md.len < (int)sizeof(self->source) - 1
                    ? md.len
                    : (int)sizeof(self->source) - 1;
        memcpy(self->source, md.s, (size_t)n);
        self->source[n] = 0;
    } else {
        StrCopyZ(self->source, (int)sizeof(self->source),
                 "# Missing report.md");
    }

    Window* win = WindowOpenView(app, StrL("Markdown Table"), 900, 720, view.id,
                                 WinOpts{});
    WindowOnWheel(win, ListenTo(view, &OnWheel));
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
