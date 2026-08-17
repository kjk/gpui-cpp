#include "gpui.h"

enum TableMode : int {
    ModeWrap = 0,
    ModeAdaptive = 1,
    ModeNowrap = 2
};

struct MdApp {
    int mode = ModeAdaptive;
    float scroll = 0;
    char source[16000];
};

static void OnInit(AppHost* host) {
    ThemeSet(ThemeMode::Light);
    AssetsClear();
    AssetsAddDefaultRoots(StrL("markdown_table"));
    AssetsAddRoot(StrL("assets/markdown_table"));
    auto* app = (MdApp*)host->user;
    app->mode = ModeAdaptive;
    app->source[0] = 0;
    Vec<u8> buf;
    if (AssetsLoad(StrL("report.md"), &buf) && buf.len > 0) {
        int n = buf.len < 15999 ? buf.len : 15999;
        memcpy(app->source, buf.els, (size_t)n);
        app->source[n] = 0;
    } else {
        strncpy_s(app->source, "# Missing report.md", _TRUNCATE);
    }
}

static void OnClick(AppHost* host, int id) {
    auto* app = (MdApp*)host->user;
    if (id == 1) {
        app->mode = (app->mode + 1) % 3;
    }
}

static void OnWheel(AppHost* host, float x, float y, float delta) {
    (void)x;
    (void)y;
    auto* app = (MdApp*)host->user;
    app->scroll -= delta;
    if (app->scroll < 0) {
        app->scroll = 0;
    }
}

static bool IsTableLine(const char* s) {
    return s && s[0] == '|';
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

static El* OnRender(AppHost* host, Arena* frame, WinSize size) {
    (void)size;
    auto* app = (MdApp*)host->user;
    const Theme& th = ThemeNow();
    El* bar = Div(frame)
                  ->FlexRow()
                  ->Pad(8)
                  ->Gap(8)
                  ->BorderT(0, th.border)
                  ->Child(ButtonEl(frame, 1, Str(ModeLabel(app->mode))));
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

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    static MdApp app;
    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    hooks.onClick = OnClick;
    hooks.onWheel = OnWheel;
    return RunApp(L"Markdown Table", 900, 700, hooks, &app);
}
