#include "gpui.h"

using namespace gpui;

struct SelApp {
    LineInput in;
    char copied[2048];
    bool selecting;
    int selFrom;
    int selTo;
};

static const char* kMsgs[] = {
    "Hello! How can I help you today?",
    "I want to select text across multiple bubbles.",
    "Sure — drag from anywhere, even from the blank space between bubbles, "
    "then press Ctrl+C to copy everything.",
    "Nice, it also keeps the top-to-bottom order.",
};
static const int kNMsgs = 4;
static const bool kMine[] = {false, true, false, true};

static void OnInit(AppHost* host) {
    ThemeSet(ThemeMode::Light);
    auto* app = (SelApp*)host->user;
    strncpy_s(app->in.placeholder,
              "Type here (selection must NOT start from here)", _TRUNCATE);
    host->input = &app->in;
    app->copied[0] = 0;
    app->selFrom = -1;
    app->selTo = -1;
}

static void OnClick(AppHost* host, int id) {
    auto* app = (SelApp*)host->user;
    if (id == 1) {
        // button: must not start selection
        app->selFrom = -1;
        app->selTo = -1;
        app->in.focused = false;
    } else if (id == 2) {
        app->in.focused = true;
        app->selFrom = -1;
    } else if (id >= 10 && id < 10 + kNMsgs) {
        app->in.focused = false;
        int ix = id - 10;
        if (app->selFrom < 0) {
            app->selFrom = ix;
        }
        app->selTo = ix;
    }
}

static void OnKey(AppHost* host, int vk, bool down) {
    if (!down) {
        return;
    }
    auto* app = (SelApp*)host->user;
    if (vk == 'C' && (GetKeyState(VK_CONTROL) & 0x8000)) {
        StrBuilder b;
        int a = app->selFrom, c = app->selTo;
        if (a > c) {
            int t = a;
            a = c;
            c = t;
        }
        if (a >= 0) {
            for (int i = a; i <= c && i < kNMsgs; i++) {
                if (i > a) {
                    b.Append(StrL("\n"));
                }
                b.Append(Str(kMsgs[i]));
            }
        }
        Str s = b.TakeStr();
        if (s.s && OpenClipboard(host->hwnd)) {
            EmptyClipboard();
            HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)s.len + 1);
            if (h) {
                memcpy(GlobalLock(h), s.s, (size_t)s.len + 1);
                GlobalUnlock(h);
                SetClipboardData(CF_TEXT, h);
            }
            CloseClipboard();
            strncpy_s(app->copied, s.s, _TRUNCATE);
        }
        StrFree(s);
    }
}

static El* Bubble(Arena* a, int ix, const Theme& th) {
    bool mine = kMine[ix];
    El* inner =
        Div(a)
            ->MaxW(420)
            ->Pad(12)
            ->Radius(8)
            ->Bg(mine ? RgbaOpacity(th.primary, 0.1f) : th.muted)
            ->Click(10 + ix)
            ->Child(
                TextEl(a, Str(kMsgs[ix]))->Font(14)->Fg(th.foreground)->Wrap());
    El* row = Div(a)->FlexRow()->W(kFill);
    if (mine) {
        row->JustifyBetween();
        row->Child(Div(a)->Grow());
    }
    row->Child(inner);
    return row;
}

static El* OnRender(AppHost* host, Arena* frame, WinSize size) {
    (void)size;
    auto* app = (SelApp*)host->user;
    const Theme& th = ThemeNow();
    El* col =
        Div(frame)->FlexCol()->SizeFull()->Pad(16)->Gap(12)->Bg(th.background);
    for (int i = 0; i < kNMsgs; i++) {
        col->Child(Bubble(frame, i, th));
    }
    col->Child(Div(frame)->Grow());
    col->Child(
        ButtonEl(frame, 1, StrL("Clicking me must not start selection")));
    Str shown = app->in.len > 0 ? Str(app->in.buf, app->in.len)
                                : Str(app->in.placeholder);
    col->Child(Div(frame)
                   ->W(kFill)
                   ->H(36)
                   ->PadX(12)
                   ->ItemsCenter()
                   ->Radius(6)
                   ->Border(1, th.border)
                   ->Click(2)
                   ->Child(TextEl(frame, shown)
                               ->Font(14)
                               ->Fg(app->in.len ? th.foreground : th.mutedFg)));
    return col;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    static SelApp app;
    AppHooks hooks = {};
    hooks.onInit = OnInit;
    hooks.onRender = OnRender;
    hooks.onClick = OnClick;
    hooks.onKey = OnKey;
    return RunApp(L"Text Selection", 800, 600, hooks, &app);
}
