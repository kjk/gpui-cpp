#include "Showcase.h"

#include <stdarg.h>
#include <stdio.h>

static ShowcaseRenderFn gRender[CompCount] = {};
static ShowcaseClickFn gClick[CompCount] = {};

void ShowcaseRegister(int comp, ShowcaseRenderFn render,
                      ShowcaseClickFn click) {
    if (comp < 0 || comp >= CompCount) {
        return;
    }
    gRender[comp] = render;
    gClick[comp] = click;
}

El* ShowcaseRenderRegistered(ShowcaseApp* app, Arena* a, WinSize size) {
    int c = app->component;
    if (c >= 0 && c < CompCount && gRender[c]) {
        return gRender[c](app, a, size);
    }
    if (c == CompOverview) {
        return ShowcaseOverview(app, a);
    }
    return ScComingSoon(a, CompSlug(c));
}

void ShowcaseClickRegistered(ShowcaseApp* app, int id) {
    int c = app->component;
    if (c >= 0 && c < CompCount && gClick[c]) {
        gClick[c](app, id);
    }
}

static const char* kSlugs[CompCount] = {
    "accordion",  "alert-dialog", "avatar",       "button",
    "calendar",   "checkbox",     "collapsible",  "color-picker",
    "combobox",   "date-picker",  "dialog",       "editor",
    "hover-card", "input",        "link",         "number-input",
    "otp-input",  "pagination",   "popover",      "popup",
    "progress",   "radio",        "radio-group",  "resizable",
    "scrollbar",  "select",       "sheet",        "slider",
    "switch",     "table",        "tabs",         "text-selection",
    "textarea",   "toast",        "toggle",       "toggle-group",
    "tooltip",    "tree",         "virtual-list",
};

const char* CompSlug(int i) {
    if (i < 0 || i >= CompCount) {
        return "overview";
    }
    return kSlugs[i];
}

int CompFromSlug(const char* slug) {
    if (!slug || !slug[0] || StrEqI(Str(slug), StrL("overview"))) {
        return CompOverview;
    }
    for (int i = 0; i < CompCount; i++) {
        if (StrEqI(Str(slug), Str(kSlugs[i]))) {
            return i;
        }
    }
    return CompOverview;
}

Str DupA(Arena* a, const char* s) {
    return StrDup(a, Str(s));
}

Str DupFmt(Arena* a, const char* f, ...) {
    char buf[512];
    va_list args;
    va_start(args, f);
    _vsnprintf_s(buf, _TRUNCATE, f, args);
    va_end(args);
    return StrDup(a, Str(buf));
}

El* ScTxt(Arena* a, Str s, float px, Rgba c) {
    return TextEl(a, s)->Font(px)->Fg(c);
}

El* ScBtnInk(Arena* a, int id, Str label) {
    return Div(a)
        ->H(28)
        ->PadX(12)
        ->ItemsCenter()
        ->JustifyCenter()
        ->Bg(ScInk())
        ->HoverBg(Rgb(0x40, 0x40, 0x40))
        ->Click(id)
        ->FocusId(id)
        ->Child(ScTxt(a, label, 12, ScWhite()));
}

El* ScBtnGhost(Arena* a, int id, Str label) {
    return Div(a)
        ->H(28)
        ->PadX(8)
        ->ItemsCenter()
        ->JustifyCenter()
        ->Border(1, ScInk())
        ->Bg(ScWhite())
        ->HoverBg(ScHover())
        ->Click(id)
        ->FocusId(id)
        ->Child(ScTxt(a, label, 12, ScInk()));
}

El* ScBtnLine(Arena* a, int id, Str label) {
    return Div(a)
        ->H(28)
        ->PadX(12)
        ->ItemsCenter()
        ->JustifyCenter()
        ->Border(1, ScBorder())
        ->Bg(ScWhite())
        ->HoverBg(ScHover())
        ->Click(id)
        ->FocusId(id)
        ->Child(ScTxt(a, label, 12, ScInk()));
}

El* ScField(Arena* a, LineInput* in, int clickId, float w, bool valid) {
    Str shown = in->len > 0 ? Str(in->buf, in->len) : Str(in->placeholder);
    Rgba fg = in->len > 0 ? ScInk() : ScMutedC();
    Rgba border = ScBorder();
    if (!valid) {
        border = ScMutedC();
    } else if (in->focused) {
        border = ScInk();
    }
    return Div(a)
        ->W(w)
        ->H(28)
        ->PadX(8)
        ->ItemsCenter()
        ->Border(1, border)
        ->Bg(ScWhite())
        ->Click(clickId)
        ->FocusId(clickId)
        ->Child(ScTxt(a, shown, 12, fg));
}

El* ScComingSoon(Arena* a, const char* name) {
    return Div(a)
        ->FlexCol()
        ->Gap(8)
        ->W(280)
        ->Child(ScTxt(a, DupA(a, name), 16, ScInk())->Semibold())
        ->Child(ScTxt(a, StrL("This component page is not ported yet."), 12,
                      ScMutedC()));
}

El* ShowcaseOverview(ShowcaseApp* app, Arena* a) {
    (void)app;
    El* col = Div(a)->FlexCol()->Gap(16)->W(720)->MaxW(720);
    col->Child(
        Div(a)
            ->FlexCol()
            ->Gap(4)
            ->Child(ScTxt(a, StrL("GPUI Base"), 18, ScInk())->Semibold())
            ->Child(ScTxt(
                a, StrL("Choose a component to open its interactive example."),
                12, ScMutedC())));

    El* grid = Div(a)->FlexCol()->Gap(4)->W(kFill);
    for (int row = 0; row < CompCount; row += 3) {
        El* r = Div(a)->FlexRow()->Gap(4)->W(kFill);
        for (int c = 0; c < 3; c++) {
            int i = row + c;
            if (i >= CompCount) {
                r->Child(Div(a)->Grow());
                continue;
            }
            r->Child(Div(a)
                         ->Grow()
                         ->H(36)
                         ->PadX(12)
                         ->ItemsCenter()
                         ->JustifyStart()
                         ->Border(1, ScBorder())
                         ->Bg(ScWhite())
                         ->HoverBg(ScHover())
                         ->Click(ClickOverview + i)
                         ->FocusId(ClickOverview + i)
                         ->Child(ScTxt(a, Str(kSlugs[i]), 12, ScInk())));
        }
        grid->Child(r);
    }
    col->Child(grid);
    return col;
}

static El* RenderComp(ShowcaseApp* app, Arena* a, WinSize size) {
    return ShowcaseRenderRegistered(app, a, size);
}

static void BindInput(ShowcaseApp* app, AppHost* host) {
    host->input = nullptr;
    app->input.focused =
        app->input.focused &&
        (app->component == CompInput || app->component == CompNumberInput ||
         (app->component == CompDialog && app->dialogOpen));
    app->comboQuery.focused = app->comboQuery.focused &&
                              app->component == CompCombobox &&
                              app->comboboxOpen;
    app->hexIn.focused = app->hexIn.focused &&
                         app->component == CompColorPicker && app->colorOpen;
    if (app->comboQuery.focused) {
        host->input = &app->comboQuery;
    } else if (app->hexIn.focused) {
        host->input = &app->hexIn;
    } else if (app->input.focused) {
        host->input = &app->input;
    }
}

static El* OnRender(AppHost* host, Arena* frame, WinSize size) {
    auto* app = (ShowcaseApp*)host->user;
    app->hoverId = host->hoverId;
    BindInput(app, host);
    bool showBack = app->navigationEnabled && app->component != CompOverview;

    El* root = Div(frame)->FlexCol()->SizeFull()->Bg(ScWhite());
    if (showBack) {
        root->Child(
            Div(frame)
                ->H(40)
                ->PadX(12)
                ->ItemsCenter()
                ->Shrink0()
                ->BorderB(1, ScLine())
                ->Child(ScBtnGhost(frame, ClickBack, StrL("All components"))));
    }

    El* content = RenderComp(app, frame, size);
    El* scroller = Div(frame)
                       ->Grow()
                       ->ClipY()
                       ->ScrollY(app->scrollY)
                       ->W(kFill)
                       ->Child(Div(frame)
                                   ->FlexCol()
                                   ->W(kFill)
                                   ->MinH(size.dipH - (showBack ? 40.f : 0.f))
                                   ->Pad(16)
                                   ->ItemsCenter()
                                   ->JustifyCenter()
                                   ->Child(content));
    root->Child(scroller);
    return root;
}

void ShowcaseClick(ShowcaseApp* app, AppHost* host, int id) {
    (void)host;
    if (id == ClickBack) {
        app->component = CompOverview;
        app->scrollY = 0;
        return;
    }
    if (id >= ClickOverview && id < ClickOverview + CompCount) {
        app->component = id - ClickOverview;
        app->scrollY = 0;
        return;
    }
    if (id == 0) {
        app->colorOpen = false;
        app->comboboxOpen = false;
        app->selectOpen = false;
        app->dateOpen = false;
        app->popupOpen = false;
        app->popoverOpen = false;
        app->hexIn.focused = false;
        app->comboQuery.focused = false;
        return;
    }
    ShowcaseClickRegistered(app, id);
}

static void InsertBuf(char* buf, int* len, int cap, uint32_t cp) {
    if (cp != '\n' && (cp < 32 || cp > 126)) {
        return;
    }
    if (*len >= cap - 1) {
        return;
    }
    buf[(*len)++] = (char)cp;
    buf[*len] = 0;
}

static void InsertAt(char* buf, int* len, int cap, int* cur, uint32_t cp) {
    if (cp != '\n' && (cp < 32 || cp > 126)) {
        return;
    }
    if (*len >= cap - 1) {
        return;
    }
    int c = *cur;
    if (c < 0) {
        c = 0;
    }
    if (c > *len) {
        c = *len;
    }
    memmove(buf + c + 1, buf + c, (size_t)(*len - c + 1));
    buf[c] = (char)cp;
    (*len)++;
    *cur = c + 1;
}

static void BackspaceBuf(char* buf, int* len) {
    if (*len > 0) {
        buf[--(*len)] = 0;
    }
}

static void BackspaceAt(char* buf, int* len, int* cur) {
    if (*cur <= 0 || *len <= 0) {
        return;
    }
    memmove(buf + *cur - 1, buf + *cur, (size_t)(*len - *cur + 1));
    (*len)--;
    (*cur)--;
}

static int LineStartAt(const char* s, int cur) {
    while (cur > 0 && s[cur - 1] != '\n') {
        cur--;
    }
    return cur;
}

static int LineEndAt(const char* s, int len, int cur) {
    while (cur < len && s[cur] != '\n') {
        cur++;
    }
    return cur;
}

static void ParseHexIn(ShowcaseApp* app) {
    const char* s = app->hexIn.buf;
    if (s[0] == '#') {
        s++;
    }
    unsigned v = 0;
    if (sscanf_s(s, "%x", &v) == 1) {
        app->colorHex = v & 0xffffff;
    }
}

void ShowcaseChar(ShowcaseApp* app, AppHost* host, uint32_t cp) {
    (void)host;
    if (app->component == CompOtpInput && app->otpOn) {
        if (cp >= '0' && cp <= '9' && app->otpLen < 6) {
            app->otp[app->otpLen++] = (char)cp;
            app->otp[app->otpLen] = 0;
        }
        return;
    }
    if (app->component == CompTextarea && app->textareaOn) {
        InsertBuf(app->textarea, &app->textareaLen, (int)sizeof(app->textarea),
                  cp);
        return;
    }
    if (app->component == CompEditor && app->editorOn) {
        InsertAt(app->editor, &app->editorLen, (int)sizeof(app->editor),
                 &app->editorCursor, cp);
        return;
    }
    if (app->component == CompColorPicker && app->hexIn.focused) {
        ParseHexIn(app);
    }
}

void ShowcaseKey(ShowcaseApp* app, AppHost* host, int vk, bool down) {
    if (!down) {
        return;
    }
    if (vk == VK_BACK) {
        if (app->component == CompOtpInput && app->otpOn) {
            BackspaceBuf(app->otp, &app->otpLen);
        } else if (app->component == CompTextarea && app->textareaOn) {
            BackspaceBuf(app->textarea, &app->textareaLen);
        } else if (app->component == CompEditor && app->editorOn) {
            BackspaceAt(app->editor, &app->editorLen, &app->editorCursor);
        }
        return;
    }
    if (app->component == CompEditor && app->editorOn) {
        int cur = app->editorCursor;
        int len = app->editorLen;
        if (vk == VK_LEFT && cur > 0) {
            app->editorCursor = cur - 1;
            return;
        }
        if (vk == VK_RIGHT && cur < len) {
            app->editorCursor = cur + 1;
            return;
        }
        if (vk == VK_HOME) {
            app->editorCursor = LineStartAt(app->editor, cur);
            return;
        }
        if (vk == VK_END) {
            app->editorCursor = LineEndAt(app->editor, len, cur);
            return;
        }
        if (vk == VK_UP) {
            int start = LineStartAt(app->editor, cur);
            int col = cur - start;
            if (start > 0) {
                int prevEnd = start - 1;
                int prevStart = LineStartAt(app->editor, prevEnd);
                app->editorCursor = prevStart + col;
                if (app->editorCursor > prevEnd) {
                    app->editorCursor = prevEnd;
                }
            }
            return;
        }
        if (vk == VK_DOWN) {
            int start = LineStartAt(app->editor, cur);
            int col = cur - start;
            int end = LineEndAt(app->editor, len, cur);
            if (end < len) {
                int nextStart = end + 1;
                int nextEnd = LineEndAt(app->editor, len, nextStart);
                app->editorCursor = nextStart + col;
                if (app->editorCursor > nextEnd) {
                    app->editorCursor = nextEnd;
                }
            }
            return;
        }
        if (vk == VK_DELETE && cur < len) {
            app->editorCursor = cur + 1;
            BackspaceAt(app->editor, &app->editorLen, &app->editorCursor);
            return;
        }
    }
    if (vk == VK_ESCAPE) {
        app->colorOpen = false;
        app->comboboxOpen = false;
        app->selectOpen = false;
        app->dateOpen = false;
        app->popupOpen = false;
        app->popoverOpen = false;
        app->hexIn.focused = false;
        app->comboQuery.focused = false;
        return;
    }
    if (vk == VK_RETURN) {
        if (app->component == CompColorPicker && app->colorOpen) {
            ParseHexIn(app);
            app->colorOpen = false;
            app->hexIn.focused = false;
            return;
        }
        if (app->component == CompTextarea && app->textareaOn) {
            InsertBuf(app->textarea, &app->textareaLen,
                      (int)sizeof(app->textarea), '\n');
            host->eatReturn = true;
        } else if (app->component == CompEditor && app->editorOn) {
            InsertAt(app->editor, &app->editorLen, (int)sizeof(app->editor),
                     &app->editorCursor, '\n');
            host->eatReturn = true;
        }
    }
}

void ShowcaseWheel(ShowcaseApp* app, float x, float y, float delta) {
    (void)x;
    (void)y;
    if (app->component == CompScrollbar) {
        app->exampleScroll -= delta;
        if (app->exampleScroll < 0) {
            app->exampleScroll = 0;
        }
        if (app->exampleScroll > 368) {
            app->exampleScroll = 368;
        }
        return;
    }
    if (app->component == CompVirtualList) {
        app->virtualScroll -= delta * 2;
        if (app->virtualScroll < 0) {
            app->virtualScroll = 0;
        }
        return;
    }
    app->scrollY -= delta;
    if (app->scrollY < 0) {
        app->scrollY = 0;
    }
    if (app->scrollY > 4000) {
        app->scrollY = 4000;
    }
}

static int TextSelOffsetAt(AppHost* host, float x, float y, bool nearest) {
    static const char* paras[] = {
        "Text selection across renderers",
        "Selection should feel like a natural part of reading a product brief. "
        "Start in this paragraph, continue into the next renderer, and GPUI "
        "preserves the document order while every frame supplies fresh "
        "geometry for the same stable selection handle.",
        "This second paragraph is deliberately long enough to wrap in the "
        "showcase. Drag across the boundary to see one continuous highlight, "
        "then use the platform copy shortcut to confirm that the copied result "
        "follows the visible reading order rather than renderer ownership.",
        "International text should remain predictable when a line mixes café, "
        "déjà vu, Kraków, naïve, and résumé. Resize the window or drag across "
        "several wrapped lines; UTF-8 byte ranges still map back to the "
        "correct glyphs without splitting a character.",
    };
    const HitRect* best = nullptr;
    float bestDist = 1e9f;
    for (int i = host->paint.hits.len - 1; i >= 0; i--) {
        const HitRect& h = host->paint.hits[i];
        if (h.id < 531 || h.id >= 535) {
            continue;
        }
        if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) {
            best = &h;
            break;
        }
        if (!nearest) {
            continue;
        }
        float cy = y;
        if (y < h.y) {
            cy = h.y;
        } else if (y > h.y + h.h) {
            cy = h.y + h.h;
        }
        float d = y - cy;
        if (d < 0) {
            d = -d;
        }
        if (d < bestDist) {
            bestDist = d;
            best = &h;
        }
    }
    if (!best) {
        return -1;
    }
    int para = best->id - 531;
    float font = para == 0 ? 18.f : 14.f;
    int local = TextIndexAt(&host->paint, Str(paras[para]), font,
                            best->w > 0 ? best->w : 560.f, true, x - best->x,
                            y - best->y);
    int off = 0;
    for (int i = 0; i < para; i++) {
        off += (int)strlen(paras[i]) + 1;
    }
    int plen = (int)strlen(paras[para]);
    if (local < 0) {
        local = 0;
    }
    if (local > plen) {
        local = plen;
    }
    return off + local;
}

void ShowcaseMouseMove(ShowcaseApp* app, AppHost* host, float x, float y) {
    if (app->component == CompSlider) {
        ShowcaseSliderDrag(app, host, x, y);
    } else if (app->component == CompResizable) {
        ShowcaseResizeDrag(app, host, x, y);
    } else if (app->component == CompTextSelection && host->mouseDown) {
        int off = TextSelOffsetAt(host, x, y, true);
        if (off >= 0) {
            app->selB = off;
        }
    }
}

void ShowcaseMouseDown(ShowcaseApp* app, AppHost* host, float x, float y,
                       int button) {
    (void)button;
    if (app->component == CompSlider) {
        ShowcaseSliderDrag(app, host, x, y);
    } else if (app->component == CompResizable) {
        ShowcaseResizeDrag(app, host, x, y);
    } else if (app->component == CompTextSelection) {
        int off = TextSelOffsetAt(host, x, y, false);
        if (off >= 0) {
            app->selA = off;
            app->selB = off;
        }
    }
}

void ShowcaseMouseUp(ShowcaseApp* app, AppHost* host, float x, float y,
                     int button) {
    (void)host;
    (void)x;
    (void)y;
    (void)button;
    app->draggingSlider = false;
    app->draggingResize = false;
}

static void OnInit(AppHost* host) {
    ThemeSet(ThemeMode::Light);
    auto* app = (ShowcaseApp*)host->user;
    strncpy_s(app->input.placeholder, "Type something…", _TRUNCATE);
    if (app->component == CompNumberInput) {
        strncpy_s(app->input.buf, "12", _TRUNCATE);
    } else {
        strncpy_s(app->input.buf, "Hello GPUI", _TRUNCATE);
    }
    app->input.len = (int)strlen(app->input.buf);
    if (app->component == CompInput || app->component == CompNumberInput) {
        app->input.focused = true;
    } else if (app->component == CompEditor) {
        app->editorOn = true;
    } else if (app->component == CompOtpInput) {
        app->otpOn = true;
    }
    strncpy_s(app->comboQuery.placeholder, "Search frameworks…", _TRUNCATE);
    strncpy_s(app->hexIn.placeholder, "#2563EB", _TRUNCATE);
    strncpy_s(app->hexIn.buf, "#2563EB", _TRUNCATE);
    app->hexIn.len = 7;
    app->textareaLen = (int)strlen(app->textarea);
}

static void OnClick(AppHost* host, int id) {
    ShowcaseClick((ShowcaseApp*)host->user, host, id);
}

static void OnChar(AppHost* host, uint32_t cp) {
    ShowcaseChar((ShowcaseApp*)host->user, host, cp);
}

static void OnKey(AppHost* host, int vk, bool down) {
    ShowcaseKey((ShowcaseApp*)host->user, host, vk, down);
}

static void OnWheel(AppHost* host, float x, float y, float delta) {
    ShowcaseWheel((ShowcaseApp*)host->user, x, y, delta);
}

static void OnMove(AppHost* host, float x, float y) {
    ShowcaseMouseMove((ShowcaseApp*)host->user, host, x, y);
}

static void OnDown(AppHost* host, float x, float y, int button) {
    ShowcaseMouseDown((ShowcaseApp*)host->user, host, x, y, button);
}

static void OnUp(AppHost* host, float x, float y, int button) {
    ShowcaseMouseUp((ShowcaseApp*)host->user, host, x, y, button);
}

static void ParseSlug(PWSTR cmd, char* out, int cap) {
    out[0] = 0;
    if (!cmd) {
        return;
    }
    while (*cmd == L' ' || *cmd == L'\t') {
        cmd++;
    }
    if (*cmd == L'"') {
        cmd++;
    }
    int n = 0;
    while (*cmd && *cmd != L' ' && *cmd != L'"' && n < cap - 1) {
        wchar_t c = *cmd++;
        out[n++] = (c < 128) ? (char)c : '?';
    }
    out[n] = 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR cmd, int) {
    static ShowcaseApp app;
    char slug[64] = {};
    ParseSlug(cmd, slug, 64);
    app.component = CompFromSlug(slug);
    app.navigationEnabled = (app.component == CompOverview);

    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    hooks.onClick = OnClick;
    hooks.onChar = OnChar;
    hooks.onKey = OnKey;
    hooks.onWheel = OnWheel;
    hooks.onMouseMove = OnMove;
    hooks.onMouseDown = OnDown;
    hooks.onMouseUp = OnUp;
    return RunApp(L"GPUI Base", 840, 640, hooks, &app);
}
