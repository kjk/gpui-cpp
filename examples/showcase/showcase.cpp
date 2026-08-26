#include "Showcase.h"

#include <stdarg.h>
#include <stdio.h>

static ShowcaseRenderFn gRender[CompCount] = {};

void ShowcaseRegister(int comp, ShowcaseRenderFn render) {
    if (comp < 0 || comp >= CompCount) {
        return;
    }
    gRender[comp] = render;
}

El* ShowcaseRenderRegistered(ShowcaseApp* app, Ctx* cx, WinSize size) {
    int c = app->component;
    if (c >= 0 && c < CompCount && gRender[c]) {
        return gRender[c](app, cx, size);
    }
    if (c == CompOverview) {
        return ShowcaseOverview(app, cx);
    }
    return ScComingSoon(cx, CompSlug(c));
}

static const char* kSlugs[CompCount] = {
    "accordion",      "alert-dialog", "avatar",      "button",
    "calendar",       "checkbox",     "collapsible", "color-picker",
    "combobox",       "date-picker",  "dialog",      "dock",
    "editor",         "hover-card",   "input",       "link",
    "number-input",   "otp-input",    "pagination",  "popover",
    "popup",          "progress",     "radio",       "radio-group",
    "resizable",      "scrollbar",    "select",      "sheet",
    "slider",         "switch",       "table",       "tabs",
    "text-selection", "textarea",     "toast",       "toggle",
    "toggle-group",   "tooltip",      "tree",        "virtual-list",
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

Str DupA(Ctx* cx, const char* s) {
    Arena* a = cx->a;
    return StrDup(a, Str(s));
}

Str DupFmt(Ctx* cx, const char* f, ...) {
    Arena* a = cx->a;
    char buf[512];
    va_list args;
    va_start(args, f);
    vsnprintf(buf, sizeof(buf), f, args);
    va_end(args);
    return StrDup(a, Str(buf));
}

El* ScTxt(Ctx* cx, Str s, float px, Rgba c) {
    Arena* a = cx->a;
    return TextEl(a, s)->Font(px)->Fg(c);
}

El* ScBtnGhost(Ctx* cx, int id, Listener onClick, Str label) {
    Arena* a = cx->a;
    return Div(a)
        ->H(28)
        ->PadX(8)
        ->ItemsCenter()
        ->JustifyCenter()
        ->Border(1, ScInk())
        ->Bg(ScWhite())
        ->HoverBg(ScHover())
        ->OnClick(onClick)
        ->FocusId(id)
        ->Child(ScTxt(cx, label, 12, ScInk()));
}

El* ScComingSoon(Ctx* cx, const char* name) {
    Arena* a = cx->a;
    return Div(a)
        ->FlexCol()
        ->Gap(8)
        ->W(280)
        ->Child(ScTxt(cx, DupA(cx, name), 16, ScInk())->Semibold())
        ->Child(ScTxt(cx, StrL("This component page is not ported yet."), 12,
                      ScMutedC()));
}

// Gallery navigation: the tile knows which component it opens.
static void OpenComp(ShowcaseApp* app, Ctx* cx, const ClickEvent*,
                     intptr_t comp) {
    app->component = (int)comp;
    app->scrollY = 0;
    Notify(cx);
}

static void BackToOverview(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->component = CompOverview;
    app->scrollY = 0;
    Notify(cx);
}

El* ShowcaseOverview(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    (void)app;
    El* col = Div(a)->FlexCol()->Gap(16)->W(720)->MaxW(720);
    col->Child(
        Div(a)
            ->FlexCol()
            ->Gap(4)
            ->Child(ScTxt(cx, StrL("GPUI Base"), 18, ScInk())->Semibold())
            ->Child(ScTxt(
                cx, StrL("Choose a component to open its interactive example."),
                12, ScMutedC())));

    El* grid = Div(a)->FlexCol()->Gap(4)->W(kFill);
    for (int row = 0; row < CompCount; row += 3) {
        El* r = Div(a)->FlexRow()->Gap(4)->W(kFill);
        for (int c = 0; c < 3; c++) {
            int i = row + c;
            if (i >= CompCount) {
                r->Child(Div(a)->Flex1());
                continue;
            }
            r->Child(
                Div(a)
                    ->Flex1()
                    ->H(36)
                    ->PadX(12)
                    ->ItemsCenter()
                    ->JustifyStart()
                    ->Border(1, ScBorder())
                    ->Bg(ScWhite())
                    ->HoverBg(ScHover())
                    ->OnClick(Listen(cx, &OpenComp, i))
                    ->FocusId(HashClickId(DupFmt(cx, "overview-item-%d", i)))
                    ->Child(ScTxt(cx, Str(kSlugs[i]), 12, ScInk())));
        }
        grid->Child(r);
    }
    col->Child(grid);
    return col;
}

static El* RenderComp(ShowcaseApp* app, Ctx* cx, WinSize size) {
    return ShowcaseRenderRegistered(app, cx, size);
}

static void BindInput(ShowcaseApp* app, Window* win) {
    win->input = nullptr;
    app->input.focused =
        app->input.focused &&
        (app->component == CompInput || app->component == CompNumberInput ||
         (app->component == CompDialog && app->dialogOpen));
    app->comboQuery.focused = app->comboQuery.focused &&
                              app->component == CompCombobox &&
                              app->comboboxOpen;
    app->hexIn.focused = app->hexIn.focused &&
                         app->component == CompColorPicker && app->colorOpen;
    app->textareaOn = app->textareaOn && app->component == CompTextarea;
    app->editorOn = app->editorOn && app->component == CompEditor;
    app->textarea.focused = app->textareaOn;
    app->editor.focused = app->editorOn;
    if (app->comboQuery.focused) {
        win->input = &app->comboQuery;
    } else if (app->hexIn.focused) {
        win->input = &app->hexIn;
    } else if (app->input.focused) {
        win->input = &app->input;
    } else if (app->textareaOn) {
        win->input = &app->textarea;
    } else if (app->editorOn) {
        win->input = &app->editor;
    }
}

El* ShowcaseApp::Render(ShowcaseApp* app, Ctx* cx) {
    Arena* frame = cx->a;
    Window* win = cx->win;
    WinSize size = WindowSize(win);
    BindInput(app, win);
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
                ->Child(ScBtnGhost(cx, HashClickId(StrL("back-to-overview")),
                                   Listen(cx, &BackToOverview),
                                   StrL("All components"))));
    }

    El* content = RenderComp(app, cx, size);
    // Surfaces rather than parts: a few pages take the whole viewport.
    // Centring one inside a `flex_none` box leaves a percentage size with
    // nothing to resolve against, and it collapses.
    bool fillsViewport = app->component == CompDock;
    El* page = Div(frame)
                   ->W(kFill)
                   ->MinH(size.dipH - (showBack ? 40.f : 0.f))
                   ->Pad(16);
    El* wrap = Div(frame)->FlexCol();
    if (fillsViewport) {
        // Rust's row, whose height is its content's: `size_full` resolves
        // against a parent with no definite height, so what is left is the
        // 420 floor the page names.
        page->FlexRow();
        wrap->Flex1()->SizeFull()->MinH(420);
    } else {
        page->FlexCol()->ItemsCenter()->JustifyCenter();
        wrap->Shrink0();
    }
    El* scroller = Div(frame)
                       ->Flex1()
                       ->ClipY()
                       ->ScrollY(app->scrollY)
                       ->W(kFill)
                       ->Child(page->Child(wrap->Child(content)));
    root->Child(scroller);
    return root;
}

// The OTP page is a row of single-character cells, not a text field, so it
// still keeps a plain buffer.
static void OtpPush(char* buf, int* len, uint32_t cp) {
    if (cp < '0' || cp > '9' || *len >= 6) {
        return;
    }
    buf[(*len)++] = (char)cp;
    buf[*len] = 0;
}

static void OtpPop(char* buf, int* len) {
    if (*len > 0) {
        buf[--(*len)] = 0;
    }
}

static void ParseHexIn(ShowcaseApp* app) {
    const char* s = InputCStr(&app->hexIn);
    if (s[0] == '#') {
        s++;
    }
    unsigned v = 0;
    if (sscanf(s, "%x", &v) == 1) {
        app->colorHex = v & 0xffffff;
    }
}

void ShowcaseChar(ShowcaseApp* app, Window* win, uint32_t cp) {
    (void)win;
    // The window routes a typed character into whichever InputState has focus;
    // only the OTP cells and the hex readout need anything on top of that.
    if (app->component == CompOtpInput && app->otpOn) {
        OtpPush(app->otp, &app->otpLen, cp);
        return;
    }
    if (app->component == CompColorPicker && app->hexIn.focused) {
        ParseHexIn(app);
    }
}

void ShowcaseKey(ShowcaseApp* app, Window* win, int vk, bool down) {
    (void)win;
    if (!down) {
        return;
    }
    if (vk == KeyBack) {
        if (app->component == CompOtpInput && app->otpOn) {
            OtpPop(app->otp, &app->otpLen);
        }
        return;
    }
    if (vk == KeyEscape) {
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
    if (vk == KeyReturn) {
        if (app->component == CompColorPicker && app->colorOpen) {
            ParseHexIn(app);
            app->colorOpen = false;
            app->hexIn.focused = false;
        }
    }
}

void ShowcaseWheel(ShowcaseApp* app, float x, float y, float delta) {
    (void)x;
    (void)y;
    app->scrollY -= delta;
    if (app->scrollY < 0) {
        app->scrollY = 0;
    }
    if (app->scrollY > 4000) {
        app->scrollY = 4000;
    }
}

// The page's four paragraphs, in document order: the offsets a selection is
// kept in are byte offsets into them, joined by one newline each.
static const char* kSelParas[] = {
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

// The paragraph under the point, and the byte offset inside it. -1 when the
// point is nowhere near the page's text.
static int TextSelParaAt(Window* win, float x, float y, bool nearest,
                         int* outLocal) {
    const HitRect* best = nullptr;
    float bestDist = 1e9f;
    for (int i = win->paint.hits.len - 1; i >= 0; i--) {
        const HitRect& h = win->paint.hits[i];
        if (h.id < 531 || h.id >= 535) {
            continue;
        }
        if (h.bounds.Contains({x, y})) {
            best = &h;
            break;
        }
        if (!nearest) {
            continue;
        }
        float cy = y;
        if (y < h.bounds.y) {
            cy = h.bounds.y;
        } else if (y > h.bounds.Bottom()) {
            cy = h.bounds.Bottom();
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
    int local = TextIndexAt(&win->paint, Str(kSelParas[para]), font,
                            best->bounds.w > 0 ? best->bounds.w : 560.f, true,
                            x - best->bounds.x, y - best->bounds.y);
    int plen = (int)strlen(kSelParas[para]);
    if (local < 0) {
        local = 0;
    }
    if (local > plen) {
        local = plen;
    }
    *outLocal = local;
    return para;
}

// Where a paragraph starts in the document the four of them make up.
static int TextSelParaBase(int para) {
    int off = 0;
    for (int i = 0; i < para; i++) {
        off += (int)strlen(kSelParas[i]) + 1;
    }
    return off;
}

static int TextSelOffsetAt(Window* win, float x, float y, bool nearest) {
    int local = 0;
    int para = TextSelParaAt(win, x, y, nearest, &local);
    if (para < 0) {
        return -1;
    }
    return TextSelParaBase(para) + local;
}

// The word (two clicks) or the paragraph (three or more) under the point, in
// document offsets — points_for_multi_click, in text_selection.rs.
static bool TextSelRangeAt(Window* win, float x, float y, int clickCount,
                           int* outA, int* outB) {
    if (clickCount < 2) {
        return false;
    }
    int local = 0;
    int para = TextSelParaAt(win, x, y, false, &local);
    if (para < 0) {
        return false;
    }
    Str text = Str(kSelParas[para]);
    int a = 0;
    int b = 0;
    if (clickCount == 2) {
        if (!TextWordRangeAt(text, local, &a, &b)) {
            return false;
        }
    } else {
        TextLineRangeAt(text, local, &a, &b);
    }
    if (a >= b) {
        return false;
    }
    int base = TextSelParaBase(para);
    *outA = base + a;
    *outB = base + b;
    return true;
}

void ShowcaseMouseMove(ShowcaseApp* app, Window* win,
                       const MouseMoveEvent* ev) {
    float x = ev->x;
    float y = ev->y;
    if (app->component == CompTextSelection && win->mouseDown) {
        int off = TextSelOffsetAt(win, x, y, true);
        if (off >= 0) {
            app->selB = off;
        }
    }
}

void ShowcaseMouseDown(ShowcaseApp* app, Window* win,
                       const MouseDownEvent* ev) {
    if (ev->button != MouseButton::Left) {
        return;
    }
    float x = ev->x;
    float y = ev->y;
    if (app->component == CompTextSelection) {
        int wordA = 0;
        int wordB = 0;
        if (TextSelRangeAt(win, x, y, ev->clickCount, &wordA, &wordB)) {
            app->selA = wordA;
            app->selB = wordB;
            return;
        }
        int off = TextSelOffsetAt(win, x, y, false);
        if (off >= 0) {
            app->selA = off;
            app->selB = off;
        }
    }
}

void ShowcaseMouseUp(ShowcaseApp* app, Window* win, const MouseUpEvent* ev) {
    (void)app;
    (void)win;
    (void)ev;
}

// A click no element claimed dismisses whatever is open, the way GPUI closes
// an overlay on an outside click.
static void OnUnhandledClick(ShowcaseApp* app, Ctx* cx, const ClickEvent*) {
    app->colorOpen = false;
    app->comboboxOpen = false;
    app->selectOpen = false;
    app->dateOpen = false;
    app->popupOpen = false;
    app->popoverOpen = false;
    app->hexIn.focused = false;
    app->comboQuery.focused = false;
    Notify(cx);
}

static void OnKey(ShowcaseApp* app, Ctx* cx, const KeyEvent* ev) {
    if (ev->ch != 0) {
        ShowcaseChar(app, cx->win, ev->ch);
        return;
    }
    ShowcaseKey(app, cx->win, ev->vk, ev->down);
}

static void OnWheel(ShowcaseApp* app, Ctx* cx, const ScrollWheelEvent* ev) {
    (void)cx;
    ShowcaseWheel(app, ev->x, ev->y, ev->deltaY);
}

static void OnMouseDown(ShowcaseApp* app, Ctx* cx, const MouseDownEvent* ev) {
    ShowcaseMouseDown(app, cx->win, ev);
}

static void OnMouseUp(ShowcaseApp* app, Ctx* cx, const MouseUpEvent* ev) {
    ShowcaseMouseUp(app, cx->win, ev);
}

static void OnMouseMove(ShowcaseApp* app, Ctx* cx, const MouseMoveEvent* ev) {
    ShowcaseMouseMove(app, cx->win, ev);
}

// The page to open, if one was named on the command line.
static void ParseSlug(int argc, char** argv, char* out, int cap) {
    out[0] = 0;
    if (argc < 2 || !argv[1]) {
        return;
    }
    StrCopyZ(out, cap, argv[1]);
}

int GpuiMain(int argc, char** argv) {
    App* app = AppNew();
    component::Init(app);
    ThemeSet(app, ThemeMode::Light);

    Entity<ShowcaseApp> view = EntityNew<ShowcaseApp>(app);
    ShowcaseApp* self = view.Get(app);
    char slug[64] = {};
    ParseSlug(argc, argv, slug, 64);
    self->component = CompFromSlug(slug);
    // 0..100 with the thumb where the page has always shown it.
    SliderSetValue(&self->slider, SliderSingle(64.f));
    self->navigationEnabled = (self->component == CompOverview);

    InputSetPlaceholder(&self->input, StrL("Type something…"));
    InputSetValue(&self->input, self->component == CompNumberInput
                                    ? StrL("12")
                                    : StrL("Hello GPUI"));
    if (self->component == CompInput || self->component == CompNumberInput) {
        self->input.focused = true;
    } else if (self->component == CompEditor) {
        self->editorOn = true;
    } else if (self->component == CompOtpInput) {
        self->otpOn = true;
    }
    InputSetPlaceholder(&self->comboQuery, StrL("Search frameworks…"));
    InputSetPlaceholder(&self->hexIn, StrL("#2563EB"));
    InputSetValue(&self->hexIn, StrL("#2563EB"));
    self->textarea.kind = InputKind::Textarea;
    InputSetValue(&self->textarea,
                  StrL("Build focused interfaces.\nKeep behavior composable."));
    self->editor.kind = InputKind::Editor;

    Window* win = WindowOpenView(app, StrL("GPUI Base C++"), 840, 640, view.id,
                                 WinOpts{});
    WindowOnUnhandledClick(win, ListenTo(view, &OnUnhandledClick));
    WindowOnKey(win, ListenTo(view, &OnKey));
    WindowOnScrollWheel(win, ListenTo(view, &OnWheel));
    WindowOnMouseDown(win, ListenTo(view, &OnMouseDown));
    WindowOnMouseUp(win, ListenTo(view, &OnMouseUp));
    WindowOnMouseMove(win, ListenTo(view, &OnMouseMove));
    int rc = AppRun(app);
    AppFree(app);
    return rc;
}
