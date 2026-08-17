/* C++ GPUI subset used by system_monitor. Frame-rebuilt element tree. */

#pragma once

#include "Base.h"

struct ID2D1Factory;
struct ID2D1RenderTarget;
struct ID2D1DCRenderTarget;
struct ID2D1SolidColorBrush;
struct ID2D1PathGeometry;
struct IDWriteFactory;
struct IDWriteTextFormat;
struct IDWriteTextLayout;

// ─── color ────────────────────────────────────────────────────────────────

struct Rgba {
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
    u8 a = 255;
};

inline Rgba Rgb(u8 r, u8 g, u8 b) {
    return Rgba{r, g, b, 255};
}
inline Rgba Rgba8(u8 r, u8 g, u8 b, u8 a) {
    return Rgba{r, g, b, a};
}
inline Rgba RgbaHex(u32 hex) {
    // 0xRRGGBB or 0xAARRGGBB if top byte set
    if (hex > 0xFFFFFFu) {
        return Rgba{(u8)((hex >> 16) & 0xff), (u8)((hex >> 8) & 0xff),
                    (u8)(hex & 0xff), (u8)((hex >> 24) & 0xff)};
    }
    return Rgba{(u8)((hex >> 16) & 0xff), (u8)((hex >> 8) & 0xff),
                (u8)(hex & 0xff), 255};
}
Rgba RgbaOpacity(Rgba c, float a01);
Rgba RgbaMix(Rgba a, Rgba b, float t);

constexpr float kAuto = -1.f;
constexpr float kFill = -2.f;

// ─── theme (Default Dark) ─────────────────────────────────────────────────

struct Theme {
    Rgba background;
    Rgba foreground;
    Rgba border;
    Rgba mutedFg;
    Rgba titleBar;
    Rgba titleBarBorder;
    Rgba tabBar;
    Rgba tabActiveBg;
    Rgba tabActiveFg;
    Rgba tabFg;
    Rgba tableBg;
    Rgba tableHeadFg;
    Rgba tableRowBorder;
    Rgba tableEven;
    Rgba progress;
    Rgba red;
    Rgba green;
    Rgba blue;
    Rgba yellow;
    Rgba danger;
    Rgba dangerFg;
    Rgba secondaryHover;
    Rgba secondaryActive;
    Rgba secondaryFg;
    Rgba secondary;
    Rgba muted;
    Rgba accent;
    Rgba primary;
    Rgba primaryFg;
    Rgba sidebar;
    Rgba sidebarFg;
    Rgba sidebarPrimary;
    Rgba sidebarPrimaryFg;
    Rgba info;
    Rgba infoFg;
    Rgba success;
    Rgba successFg;
    Rgba warning;
    Rgba warningFg;
    Rgba skeleton;
    float radius;
};

enum class ThemeMode : u8 {
    Light,
    Dark
};

const Theme& ThemeDark();
const Theme& ThemeLight();
const Theme& ThemeNow();
void ThemeSet(ThemeMode mode);
ThemeMode ThemeGet();

// ─── style / element ──────────────────────────────────────────────────────

enum class ElKind : u8 {
    Div,
    Text,
    Chart,
    Progress,
    Icon
};

enum class FlexDir : u8 {
    Row,
    Col
};
enum class Align : u8 {
    Start,
    Center,
    End,
    Stretch
};
enum class Justify : u8 {
    Start,
    Center,
    End,
    SpaceBetween
};
enum class OverflowY : u8 {
    Visible,
    Hidden,
    Scroll
};

enum class IconName : u8 {
    None = 0,
    Inbox,
    Bot,
    Cpu,
    MemoryStick,
    HardDrive,
    Battery,
    BatteryCharging,
    BatteryMedium,
    BatteryFull,
    WindowMinimize,
    WindowMaximize,
    WindowRestore,
    WindowClose,
    LayoutDashboard,
    Calendar,
    Folder,
    Settings,
    GalleryVerticalEnd,
    CircleUser,
    PanelLeft,
    Info,
    X,
    CircleCheck,
    TriangleAlert,
    CircleX,
    Loader,
    ChevronDown,
    ChevronRight,
    ChevronUp,
    Check,
    Search,
    Minus,
    Plus,
    Copy,
};

struct PaintCtx;

struct ChartSeries {
    const float* ys = nullptr;
    int n = 0;
    int tickMargin = 15;
    Rgba stroke = {};
    Rgba fillTop = {};
    Rgba fillBot = {};
};

struct Style {
    FlexDir dir = FlexDir::Row;
    Align align = Align::Stretch;
    Justify justify = Justify::Start;
    OverflowY overflowY = OverflowY::Visible;
    float width = kAuto;
    float height = kAuto;
    float minW = 0;
    float minH = 0;
    float maxW = 1e9f;
    float maxH = 1e9f;
    float flexGrow = 0;
    float flexShrink = 1;
    float padL = 0, padT = 0, padR = 0, padB = 0;
    float gap = 0;
    float border = 0;
    float borderT = 0;
    float borderB = 0;
    float radius = 0;
    Rgba bg = {};
    Rgba borderColor = {};
    Rgba color = {};
    float fontSize = 0; // 0 = inherit
    bool truncate = false;
    bool wrap = false;
    bool hasBg = false;
    bool hasColor = false;
    bool fontBold = false;
    bool fontSemibold = false;
    bool borderDashed = false;
    bool absolute = false;
    bool fixed = false; // out-of-flow in window coords (Rust deferred overlay)
    bool anchorBelow = false; // absolute, just under the parent box
    float anchorGap = 0;
    float absTop = kAuto, absLeft = kAuto, absBottom = kAuto, absRight = kAuto;
    Rgba hoverBg = {};
    bool hasHoverBg = false;
    int focusId = 0;
    int trapId = 0;
    Str tooltip;
};

struct El {
    ElKind kind = ElKind::Div;
    Style style;
    Str id;
    Str text;
    IconName icon = IconName::None;
    Str iconPath;
    ChartSeries chart = {};
    float progress = 0; // 0..100
    int clickId = 0;
    Func0 onClick;
    void (*customPaint)(PaintCtx* ctx, El* e, void* user) = nullptr;
    void* customUser = nullptr;
    El* first = nullptr;
    El* last = nullptr;
    El* next = nullptr;
    float x = 0, y = 0, w = 0, h = 0;
    float scrollY = 0;
    int scrollId = 0;
    float contentW = 0;
    float contentH = 0;
    int selLo = -1; // UTF-8 offsets into text, -1 = none
    int selHi = -1;
    float laidFont = 0; // resolved font size from last LayoutEl
    float laidMaxW = 0; // MeasureText maxW used (0 = unconstrained)

    El* FlexRow();
    El* FlexCol();
    El* Grow(float g = 1);
    El* Shrink0();
    El* W(float v);
    El* H(float v);
    El* SizeFull();
    El* MinH(float v);
    El* MinW(float v);
    El* MaxW(float v);
    El* MaxH(float v);
    El* Gap(float v);
    El* Pad(float v);
    El* PadX(float v);
    El* PadY(float v);
    El* PadL(float v);
    El* PadR(float v);
    El* PadT(float v);
    El* PadB(float v);
    El* ItemsCenter();
    El* ItemsStart();
    El* JustifyBetween();
    El* JustifyCenter();
    El* JustifyEnd();
    El* JustifyStart();
    El* Bg(Rgba c);
    El* Border(float width, Rgba c);
    El* BorderT(float width, Rgba c);
    El* BorderB(float width, Rgba c);
    El* Radius(float r);
    El* Fg(Rgba c);
    El* Font(float px);
    El* Truncate();
    El* ClipY();
    El* ScrollY(float off);
    El* ScrollId(int id);
    El* Click(int id);
    El* OnClick(Func0 fn);
    El* Child(El* c);
    El* Bold();
    El* Semibold();
    El* Wrap();
    El* Dashed();
    El* Absolute();
    El* Fixed();
    El* AnchorBelow(float gap = 0);
    El* Top(float v);
    El* Left(float v);
    El* Bottom(float v);
    El* Right(float v);
    El* HoverBg(Rgba c);
    El* FocusId(int id);
    El* TrapId(int id);
    El* Tip(Str s);
    El* Id(Str s);
};

enum class BtnKind : u8 {
    Default,
    Primary,
    Outline
};

El* ButtonEl(Arena* a, int clickId, Str label, BtnKind kind = BtnKind::Default);
El* ButtonSmall(Arena* a, int clickId, Str label, BtnKind kind, bool selected);

El* Div(Arena* a);
El* TextEl(Arena* a, Str s);
El* IconEl(Arena* a, IconName name);
El* IconEl(Arena* a, IconName name, float size);
El* ProgressEl(Arena* a, float value01to100, float barW, float barH);
El* ChartEl(Arena* a, const float* ys, int n, Rgba stroke, Rgba fillTop,
            Rgba fillBot, int tickMargin);

// ─── paint / window ───────────────────────────────────────────────────────

struct HitRect {
    int id = 0;
    float x = 0, y = 0, w = 0, h = 0;
    Func0 onClick;
};

struct ScrollRect {
    int id = 0;
    float x = 0, y = 0, w = 0, h = 0;
    float contentH = 0;
};

// Two-generation shaped-text cache (see TextMeas* in Gpui.cpp). Opaque slots.
struct TextMeasCache {
    void* slots = nullptr;
    int cap = 0;
    int used = 0;
    u32 frame = 0;
};

struct PaintCtx {
    ID2D1Factory* d2d = nullptr;
    IDWriteFactory* dwrite = nullptr;
    ID2D1RenderTarget* rt = nullptr;
    ID2D1DCRenderTarget* dcRt = nullptr;
    ID2D1SolidColorBrush* brush = nullptr;
    IDWriteTextFormat* font16 = nullptr;
    IDWriteTextFormat* font14 = nullptr;
    IDWriteTextFormat* font12 = nullptr;
    IDWriteTextFormat* font20 = nullptr;
    IDWriteTextFormat* font24 = nullptr;
    IDWriteTextFormat* font16b = nullptr;
    float dpi = 96;
    float viewW = 0;
    float viewH = 0;
    int hoverId = 0;
    int focusId = 0;
    Vec<HitRect> hits;
    Vec<ScrollRect> scrolls;
    TextMeasCache textCache;

    PaintCtx() = default;
};

struct FocusRect {
    int id = 0;
    int trapId = 0;
    float x = 0, y = 0, w = 0, h = 0;
};

struct LineInput {
    char buf[512] = {};
    int len = 0;
    int cursor = 0;
    char placeholder[128] = {};
    bool focused = false;
};

struct Overlay {
    int kind = 0; // 0 none, 1 dialog, 2 sheet
    char title[128] = {};
    char body[2048] = {};
};

struct MenuState {
    bool open = false;
    float x = 0, y = 0;
    char items[8][32] = {};
    int nItems = 0;
    int clickBase = 0;
};

struct WinSize {
    float dipW = 0;
    float dipH = 0;
    int pxW = 0;
    int pxH = 0;
};

float PxToDip(PaintCtx* ctx, int px);
int DipToPx(PaintCtx* ctx, float dip);

void MeasureText(PaintCtx* ctx, Str s, float fontSize, float maxW, float* outW,
                 float* outH, bool wrap = false, bool bold = false);
void TextMeasBeginFrame(PaintCtx* ctx);
void TextMeasEndFrame(PaintCtx* ctx);
void TextMeasClear(PaintCtx* ctx);
int TextIndexAt(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                float relX, float relY);
void PaintTextRange(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                    float x, float y, int u8a, int u8b, Rgba color);
void LayoutEl(PaintCtx* ctx, El* e, float x, float y, float availW,
              float availH, float inheritFont, Rgba inheritFg);
void PaintEl(PaintCtx* ctx, El* e);
int HitTest(PaintCtx* ctx, float x, float y);
const HitRect* HitTestRect(PaintCtx* ctx, float x, float y);
const ScrollRect* HitScrollRect(PaintCtx* ctx, float x, float y);
int HashClickId(Str s);

// Reserved click ids for custom window chrome (WM_NCHITTEST).
// Widget click ids must not use these — 100/101/102/200 used to be
// hardcoded here and collided with the showcase overview grid.
enum {
    ClickWinMin = -1,
    ClickWinMax = -2,
    ClickWinClose = -3,
    ClickWinCaption = -4,
};

struct AppHost;

// Filled in by the example.
struct AppHooks {
    void (*onInit)(AppHost* host);
    void (*onTick)(AppHost* host);
    El* (*onRender)(AppHost* host, Arena* frame, WinSize size);
    void (*onClick)(AppHost* host, int clickId);
    void (*onWheel)(AppHost* host, float x, float y, float delta);
    void (*onKey)(AppHost* host, int vk, bool down);
    void (*onChar)(AppHost* host, u32 cp);
    void (*onMouseMove)(AppHost* host, float x, float y);
    void (*onMouseDown)(AppHost* host, float x, float y, int button);
    void (*onMouseUp)(AppHost* host, float x, float y, int button);
    void (*onShutdown)(AppHost* host);
};

struct AppWinOpts {
    bool borderless = false;
    bool anim = false;
    int timerMs = 500;
};

struct AppHost {
    HWND hwnd = nullptr;
    PaintCtx paint = {};
    Arena* frameArena = nullptr;
    AppHooks hooks = {};
    void* user = nullptr;
    int hoverId = 0;
    int focusId = 0;
    float mouseX = 0;
    float mouseY = 0;
    bool maximized = false;
    bool running = true;
    bool anim = false;
    bool mouseDown = false;
    bool eatReturn = false;
    LineInput* input = nullptr;
    Overlay overlay = {};
    MenuState menu = {};
    Vec<FocusRect> focusEls;
    AppWinOpts winOpts = {};

    AppHost() = default;
};

int RunApp(const wchar_t* title, int dipW, int dipH, AppHooks hooks,
           void* user);
int RunAppEx(const wchar_t* title, int dipW, int dipH, AppHooks hooks,
             void* user, AppWinOpts opts);
void AppSetTitle(AppHost* host, const wchar_t* title);
void AppRequestAnim(AppHost* host, bool on);

// Collect focusable click targets from last paint for Tab cycling.
void FocusCollect(AppHost* host, El* root);
int FocusNext(AppHost* host, int trapId, bool backward);
void AppQuit(AppHost* host);
void AppInvalidate(AppHost* host);
void AppMinimize(AppHost* host);
void AppToggleMaximize(AppHost* host);
void AppClose(AppHost* host);
void AppDrag(AppHost* host);
bool AppIsMaximized(AppHost* host);
