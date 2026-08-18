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
    float x = ev->x;
    float y = ev->y;
    float delta = ev->delta;
    (void)x;
    (void)y;
    app->scroll -= delta;
    if (app->scroll < 0) {
        app->scroll = 0;
    }
}

static bool IsTableLine(const char* s) {
    return s && s[0] == '|';
}

static void CycleMode(MdApp* app, Ctx* cx, const ClickEvent*) {
    app->mode = (app->mode + 1) % 3;
    Notify(cx);
}

static El* RenderMd(Arena* a, const char* src, int mode, const Theme& th) {
    El* col = Div(a)->FlexCol()->Gap(10)->Pad(16);
    const char* p = src;
    while (*p) {
        const char* line = p;
        while (*p && *p != '\n') {
            p++;
        }
        int llen = (int)(p - line);
        if (*p == '\n') {
            p++;
        }
        if (llen >= 3 && line[0] == '#' && line[1] == '#' && line[2] == '#') {
            Str t(line + 3, llen - 3);
            while (t.s && t.len && t.s[0] == ' ') {
                t.s++;
                t.len--;
            }
            col->Child(TextEl(a, t)->Font(18)->Semibold()->Fg(th.foreground));
            continue;
        }
        if (llen >= 2 && line[0] == '#' && line[1] == '#') {
            Str t(line + 2, llen - 2);
            while (t.s && t.len && t.s[0] == ' ') {
                t.s++;
                t.len--;
            }
            col->Child(TextEl(a, t)->Font(20)->Semibold()->Fg(th.foreground));
            continue;
        }
        if (llen >= 1 && line[0] == '#') {
            Str t(line + 1, llen - 1);
            while (t.s && t.len && t.s[0] == ' ') {
                t.s++;
                t.len--;
            }
            col->Child(TextEl(a, t)->Font(24)->Semibold()->Fg(th.foreground));
            continue;
        }
        if (llen >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
            col->Child(Div(a)->H(1)->W(kFill)->Bg(th.border));
            continue;
        }
        if (IsTableLine(line)) {
            El* table = Div(a)->FlexCol()->Border(1, th.border);
            int rows = 0;
            const char* q = line;
            const char* save = p;
            while (q && IsTableLine(q)) {
                const char* e = q;
                while (*e && *e != '\n') {
                    e++;
                }
                // skip separator |---|
                bool sep = true;
                for (const char* c = q; c < e; c++) {
                    if (*c != '|' && *c != '-' && *c != ':' && *c != ' ') {
                        sep = false;
                        break;
                    }
                }
                if (!sep) {
                    El* row = Div(a)->FlexRow();
                    if (rows == 0) {
                        row->Bg(th.muted);
                    } else if (rows % 2 == 0) {
                        row->Bg(th.tableEven);
                    }
                    const char* cell = q + 1;
                    int cols = 0;
                    while (cell < e) {
                        const char* bar = cell;
                        while (bar < e && *bar != '|') {
                            bar++;
                        }
                        int cl = (int)(bar - cell);
                        while (cl > 0 && cell[0] == ' ') {
                            cell++;
                            cl--;
                        }
                        while (cl > 0 && cell[cl - 1] == ' ') {
                            cl--;
                        }
                        float cw = mode == ModeNowrap
                                       ? 180.f
                                       : (mode == ModeAdaptive ? 140.f : 110.f);
                        row->Child(Div(a)->W(cw)->PadX(8)->PadY(6)->Child(
                            TextEl(a, Str((char*)cell, cl))
                                ->Font(12)
                                ->Fg(th.foreground)
                                ->Wrap()));
                        cols++;
                        if (bar >= e) {
                            break;
                        }
                        cell = bar + 1;
                    }
                    table->Child(row);
                    rows++;
                }
                if (*e == '\n') {
                    e++;
                }
                q = e;
                p = q;
                if (!*q) {
                    break;
                }
            }
            (void)save;
            col->Child(table);
            continue;
        }
        if (llen > 0) {
            col->Child(TextEl(a, Str((char*)line, llen))
                           ->Font(14)
                           ->Fg(th.foreground)
                           ->Wrap());
        }
    }
    return col;
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

El* MdApp::Render(MdApp* app, Ctx* cx) {
    Arena* frame = cx->a;
    Window* win = cx->win;

    WinSize size = WindowSize(cx->win);

    const Theme& th = cx->theme();
    El* bar = Div(frame)
                  ->FlexRow()
                  ->Pad(8)
                  ->Gap(8)
                  ->BorderT(0, th.border)
                  ->Child(ButtonEl(frame, 1, Str(ModeLabel(app->mode)))
                              ->OnClick(Listen(cx, &CycleMode)));
    El* body = Div(frame)
                   ->Grow()
                   ->ClipY()
                   ->ScrollY(app->scroll)
                   ->Child(RenderMd(frame, app->source, app->mode, th));
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
    Entity<MdApp> view = EntityNew<MdApp>(app);
    MdApp* self = view.Get(app);
    (void)self;
    ThemeSet(app, ThemeMode::Light);
    AssetsClear();
    AssetsAddDefaultRoots(StrL("markdown_table"));
    AssetsAddRoot(StrL("assets/markdown_table"));
    self->mode = ModeAdaptive;
    self->source[0] = 0;
    Vec<uint8_t> buf;
    if (AssetsLoad(StrL("report.md"), &buf) && buf.len > 0) {
        int n = buf.len < 15999 ? buf.len : 15999;
        memcpy(self->source, buf.els, (size_t)n);
        self->source[n] = 0;
    } else {
        StrCopyZ(self->source, (int)sizeof(self->source),
                 "# Missing report.md");
    }
    WinOpts opts = {};
    Window* win =
        WindowOpenView(app, StrL("Markdown Table"), 900, 700, view.id, opts);
    WindowOnWheel(win, ListenTo(view, &OnWheel));
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
