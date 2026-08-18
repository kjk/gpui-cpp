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

// ─── inline markdown ─────────────────────────────────────────────────────
//
// crates/ui/src/text/inline.rs builds a real inline flow; report.md only uses
// **bold** and *italic*, so this is that subset.

static bool MdHasEmphasis(Str s) {
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] == '*') {
            return true;
        }
    }
    return false;
}

// A run with no emphasis stays one TextEl, so the text engine keeps breaking
// the line on its own metrics. Only a run that changes weight part-way pays
// for the word-by-word wrapping row.
static El* MdInline(Arena* a, Str s, float font, Rgba color, bool baseBold) {
    if (!MdHasEmphasis(s)) {
        El* t = TextEl(a, s)->Font(font)->Fg(color)->Wrap();
        if (baseBold) {
            t->Semibold();
        }
        return t;
    }
    // No gap: a word carries its own trailing space, so punctuation that
    // follows an emphasis run sits tight against it ("**bold**:" not
    // "bold :") and a wrapped line keeps the paragraph's leading.
    El* row = Div(a)->FlexRow()->FlexWrap();
    bool bold = false;
    bool italic = false;
    char word[256];
    int n = 0;
    auto flush = [&]() {
        if (n <= 0) {
            return;
        }
        El* t = TextEl(a, StrDup(a, Str(word, n)))->Font(font)->Fg(color);
        if (bold || baseBold) {
            t->Semibold();
        }
        if (italic) {
            t->Italic();
        }
        row->Child(t);
        n = 0;
    };
    for (int i = 0; i <= s.len; i++) {
        char c = i < s.len ? s.s[i] : 0;
        if (c == '*') {
            flush();
            // ** toggles weight, a lone * toggles slant.
            if (i + 1 < s.len && s.s[i + 1] == '*') {
                bold = !bold;
                i++;
            } else {
                italic = !italic;
            }
            continue;
        }
        if (c == ' ' || c == 0) {
            if (c == ' ' && n < (int)sizeof(word) - 1) {
                word[n++] = ' ';
            }
            flush();
            continue;
        }
        if (n < (int)sizeof(word) - 1) {
            word[n++] = c;
        }
    }
    return row;
}

// "- item" and "1. item". The marker sits in a gutter and the text wraps
// beside it; Rust's depth-0 bullet is "•" (crates/ui/src/text/utils.rs).
static El* MdListRow(Arena* a, Str marker, Str text, const Theme& th) {
    return Div(a)
        ->FlexRow()
        ->W(kFill)
        ->Gap(8)
        ->Child(Div(a)->W(16)->Shrink0()->Child(
            TextEl(a, marker)->Font(14)->Fg(th.mutedFg)))
        ->Child(MdInline(a, text, 14, th.foreground, false)->Grow());
}

static bool MdBullet(const char* line, int llen, int* textStart) {
    if (llen >= 2 && (line[0] == '-' || line[0] == '*') && line[1] == ' ') {
        *textStart = 2;
        return true;
    }
    return false;
}

static bool MdOrdered(const char* line, int llen, int* textStart) {
    int i = 0;
    while (i < llen && line[i] >= '0' && line[i] <= '9') {
        i++;
    }
    if (i == 0 || i + 1 >= llen || line[i] != '.' || line[i + 1] != ' ') {
        return false;
    }
    *textStart = i + 2;
    return true;
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
            col->Child(MdInline(a, t, 18, th.foreground, true)->W(kFill));
            continue;
        }
        if (llen >= 2 && line[0] == '#' && line[1] == '#') {
            Str t(line + 2, llen - 2);
            while (t.s && t.len && t.s[0] == ' ') {
                t.s++;
                t.len--;
            }
            col->Child(MdInline(a, t, 20, th.foreground, true)->W(kFill));
            continue;
        }
        if (llen >= 1 && line[0] == '#') {
            Str t(line + 1, llen - 1);
            while (t.s && t.len && t.s[0] == ' ') {
                t.s++;
                t.len--;
            }
            col->Child(MdInline(a, t, 24, th.foreground, true)->W(kFill));
            continue;
        }
        if (llen >= 3 && line[0] == '-' && line[1] == '-' && line[2] == '-') {
            col->Child(Div(a)->H(1)->W(kFill)->Bg(th.border));
            continue;
        }
        int textStart = 0;
        if (MdBullet(line, llen, &textStart)) {
            col->Child(MdListRow(a, StrL("\xE2\x80\xA2"),
                                 Str((char*)line + textStart, llen - textStart),
                                 th));
            continue;
        }
        if (MdOrdered(line, llen, &textStart)) {
            col->Child(MdListRow(a, StrDup(a, Str((char*)line, textStart - 1)),
                                 Str((char*)line + textStart, llen - textStart),
                                 th));
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
                            MdInline(a, Str((char*)cell, cl), 12, th.foreground,
                                     false)
                                ->W(kFill)));
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
            col->Child(
                MdInline(a, Str((char*)line, llen), 14, th.foreground, false)
                    ->W(kFill));
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
