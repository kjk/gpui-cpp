/* C++ GPUI subset used by system_monitor. Frame-rebuilt element tree. */

#include "Base.h"

// ─── color ────────────────────────────────────────────────────────────────

namespace gpui {

struct Rgba {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

inline Rgba Rgb(uint8_t r, uint8_t g, uint8_t b) {
    return Rgba{r, g, b, 255};
}
inline Rgba Rgba8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return Rgba{r, g, b, a};
}
inline Rgba RgbaHex(uint32_t hex) {
    // 0xRRGGBB or 0xAARRGGBB if top byte set
    if (hex > 0xFFFFFFu) {
        return Rgba{(uint8_t)((hex >> 16) & 0xff), (uint8_t)((hex >> 8) & 0xff),
                    (uint8_t)(hex & 0xff), (uint8_t)((hex >> 24) & 0xff)};
    }
    return Rgba{(uint8_t)((hex >> 16) & 0xff), (uint8_t)((hex >> 8) & 0xff),
                (uint8_t)(hex & 0xff), 255};
}
Rgba RgbaOpacity(Rgba c, float a01);
Rgba RgbaMix(Rgba a, Rgba b, float t);
inline D2D1_COLOR_F RgbaToD2D(Rgba c) {
    return D2D1::ColorF(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
}

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
    Rgba scrollbarThumb;
    Rgba info;
    Rgba infoFg;
    Rgba success;
    Rgba successFg;
    Rgba warning;
    Rgba warningFg;
    Rgba skeleton;
    // crates/ui/src/theme/mod.rs: radius 6, radius_lg 8 (Dialog, Notification).
    float radius;
    float radiusLg;
};

enum class ThemeMode : uint8_t {
    Light,
    Dark
};

const Theme& ThemeDark();
const Theme& ThemeLight();
const Theme& ThemeNow();
void ThemeSet(ThemeMode mode);
ThemeMode ThemeGet();

// ─── entities ─────────────────────────────────────────────────────────────
//
// GPUI keeps view state in `App` and hands out `Entity<T>` handles; a view
// implements `Render` and mutates itself through `Context<T>`. The same shape
// here, minus the refcounting: `App` owns the state, `Entity<T>` is a POD
// generational handle, and `Ctx` is the one context type (GPUI splits it into
// `&mut App` / `&mut Window` / `&mut Context<T>` only to satisfy the borrow
// checker).

struct App;
struct Window;
struct Ctx;
struct El;

struct EntityId {
    int32_t index = -1;
    uint32_t gen = 0; // 0 == null handle

    bool IsValid() const { return index >= 0 && gen != 0; }
};

inline bool operator==(EntityId a, EntityId b) {
    return a.index == b.index && a.gen == b.gen;
}
inline bool operator!=(EntityId a, EntityId b) {
    return !(a == b);
}

using RenderFn = El* (*)(void* self, Ctx* cx);
using DropFn = void (*)(void* self);

struct EntitySlot {
    void* ptr = nullptr;
    uint32_t gen = 0;
    RenderFn render = nullptr;
    DropFn drop = nullptr;
};

// window.use_keyed_state: per-window state owned by a RenderOnce element that
// has nowhere else to keep it.
struct KeyedSlot {
    uint32_t key = 0;
    void* ptr = nullptr;
    DropFn drop = nullptr;
};

struct ClickEvent {
    float x = 0;
    float y = 0;
    int button = 1;
    // The element's click id, when it has one. Lets one handler serve a list.
    int id = 0;
};

struct KeyEvent {
    int vk = 0;
    uint32_t ch = 0; // WM_CHAR codepoint, 0 for key down/up
    bool down = false;
};

enum class MouseKind : uint8_t {
    Down,
    Up,
    Move
};

struct MouseEvent {
    MouseKind kind = MouseKind::Down;
    float x = 0;
    float y = 0;
    int button = 1; // 1 left, 2 right, 0 for moves
    int id = 0;     // click id under the cursor, when there is one
};

struct WheelEvent {
    float x = 0;
    float y = 0;
    float delta = 0;
};

// Fired by the window timer; GPUI does this with cx.spawn + Timer::after.
struct TickEvent {
    int ms = 0;
};

// cx.listener(...): a handler plus the entity it runs against. Dispatch looks
// the entity up and drops the event if the handle went stale.
using ListenerFn = void (*)(void* self, Ctx* cx, const void* ev);

struct Listener {
    ListenerFn fn = nullptr;
    EntityId view = {};

    bool IsValid() const { return fn != nullptr; }
};

// ─── style / element ──────────────────────────────────────────────────────

enum class ElKind : uint8_t {
    Div,
    Text,
    Chart,
    Progress,
    Icon
};

enum class FlexDir : uint8_t {
    Row,
    Col
};
enum class Align : uint8_t {
    Start,
    Center,
    End,
    Stretch
};
enum class Justify : uint8_t {
    Start,
    Center,
    End,
    SpaceBetween
};
enum class OverflowY : uint8_t {
    Visible,
    Hidden,
    Scroll
};

enum class IconName : uint8_t {
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
    User,
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
    Bell,
    Star,
    Eye,
    Heart,
    ArrowLeft,
    Building2,
    Asterisk,
    Sun,
    Maximize,
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
    Listener listener;
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
    bool selectable = false;
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
    El* OnClick(Listener l);
    El* Child(El* c);
    El* Bold();
    El* Semibold();
    El* Selectable();
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

enum class BtnKind : uint8_t {
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
    Listener listener;
};

struct ScrollRect {
    int id = 0;
    float x = 0, y = 0, w = 0, h = 0;
    float contentH = 0;
};

struct TextHit {
    float x = 0, y = 0, w = 0, h = 0;
    Str text;
    float font = 14;
    float maxW = 0;
    bool wrap = false;
    int docOff = 0;
};

// Two-generation shaped-text cache (see TextMeas* in Gpui.cpp). Opaque slots.
struct TextMeasCache {
    void* slots = nullptr;
    int cap = 0;
    int used = 0;
    uint32_t frame = 0;
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
    Vec<TextHit> texts;
    int textDocLen = 0;
    int selA = -1;
    int selB = -1;
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
                 float* outH, bool wrap = false, int weight = 0);
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
int TextHitOffsetAt(PaintCtx* ctx, float x, float y, bool nearest);
int CopyTextHits(PaintCtx* ctx, int selA, int selB, char* out, int cap);
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

struct App;
struct Window;

struct AppWinOpts {
    bool borderless = false;
    bool anim = false;
    int timerMs = 500;
};

// Process-wide state: the Direct2D / DirectWrite factories, the shared font
// cache, the entity store and the open windows. GPUI's `App`.
struct App {
    ID2D1Factory* d2d = nullptr;
    IDWriteFactory* dwrite = nullptr;
    IDWriteTextFormat* font16 = nullptr;
    IDWriteTextFormat* font14 = nullptr;
    IDWriteTextFormat* font12 = nullptr;
    IDWriteTextFormat* font20 = nullptr;
    IDWriteTextFormat* font24 = nullptr;
    IDWriteTextFormat* font16b = nullptr;
    Vec<Window*> windows;
    // Entity store; see Entity.h. Slots are recycled, so a handle carries a
    // generation and goes stale instead of dangling.
    Vec<EntitySlot> entities;
    Vec<int32_t> freeSlots;
    int exitCode = 0;

    App() = default;
};

// One platform window: its render target, frame arena, hover / focus state
// and the view it renders. GPUI's `Window`.
struct Window {
    App* app = nullptr;
    HWND hwnd = nullptr;
    PaintCtx paint = {};
    Arena* frameArena = nullptr;
    // The view this window renders. GPUI's Window holds a root view too.
    EntityId root = {};
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
    Vec<KeyedSlot> keyed;
    AppWinOpts winOpts = {};
    // Window-level subscriptions bound to view entities.
    Listener onKey = {};
    Listener onWheel = {};
    Listener onTick = {};
    Listener onClick = {};
    Listener onMouse = {};
    int tickMs = 0;

    Window() = default;
};

// ─── context ──────────────────────────────────────────────────────────────

// GPUI's Context<T>. `win` is null outside a window callback, `a` is the frame
// arena during render, `self` is the entity currently rendering or updating.
struct Ctx {
    App* app = nullptr;
    Window* win = nullptr;
    Arena* a = nullptr;
    EntityId self = {};
};

EntityId EntityNewRaw(App* app, void* ptr, RenderFn render, DropFn drop);
void* EntityGet(App* app, EntityId id);
void EntityDrop(App* app, EntityId id);
void EntityDropAll(App* app);

// A typed handle. Stale handles read back as null instead of dangling.
template <typename T>
struct Entity {
    EntityId id = {};

    bool IsValid() const { return id.IsValid(); }
    T* Get(App* app) const { return (T*)EntityGet(app, id); }
    T* Get(Ctx* cx) const { return (T*)EntityGet(cx->app, id); }
};

template <typename T>
void EntityDropT(void* p) {
    delete (T*)p;
}

// cx.new(|cx| T::default()). T must expose `static El* Render(T*, Ctx*)`.
template <typename T>
Entity<T> EntityNew(App* app) {
    Entity<T> e;
    e.id = EntityNewRaw(app, new T(), (RenderFn)&T::Render, &EntityDropT<T>);
    return e;
}

template <typename T>
Entity<T> EntityNew(Ctx* cx) {
    return EntityNew<T>(cx->app);
}

// State with no Render, e.g. a model the views read.
template <typename T>
Entity<T> EntityNewState(App* app) {
    Entity<T> e;
    e.id = EntityNewRaw(app, new T(), nullptr, &EntityDropT<T>);
    return e;
}

// cx.listener(|this, ev, window, cx| ...). The cast mirrors MkFunc0/MkFunc1.
// E is whichever event struct the handler takes: ClickEvent, KeyEvent, ...
template <typename T, typename E>
Listener Listen(Ctx* cx, void (*fn)(T*, Ctx*, const E*)) {
    Listener l;
    l.fn = (ListenerFn)fn;
    l.view = cx->self;
    return l;
}

// Same, but bound to another entity instead of the one that is rendering.
template <typename T, typename E>
Listener ListenTo(Entity<T> e, void (*fn)(T*, Ctx*, const E*)) {
    Listener l;
    l.fn = (ListenerFn)fn;
    l.view = e.id;
    return l;
}

// cx.notify(): the frame tree is rebuilt from scratch, so this just schedules
// a repaint of every window. GPUI tracks which views observe the entity.
void Notify(Ctx* cx);
void NotifyApp(App* app);
void ListenerCall(App* app, Window* win, const Listener& l, const void* ev);

// Render an entity into `a`, building the Ctx for it.
El* EntityRender(App* app, Window* win, Arena* a, EntityId id);

// window.use_keyed_state(key, cx, init)
void* WindowKeyedState(Window* win, uint32_t key, int size, DropFn drop);
void WindowKeyedFree(Window* win);

template <typename T>
T* KeyedState(Ctx* cx, uint32_t key) {
    void* p = WindowKeyedState(cx->win, key, (int)sizeof(T), &EntityDropT<T>);
    return (T*)p;
}

// Window-level subscriptions. GPUI spells these window.on_key_down and
// cx.spawn + Timer::after; here each one is a Listener bound to a view.
void WindowOnKey(Window* win, Listener l);
void WindowOnWheel(Window* win, Listener l);
// View-level click dispatch: fires with ClickEvent::id for any hit that has a
// click id but no listener of its own. Elements should carry their own
// listener; this is the bridge for code still written against click ids.
void WindowOnClick(Window* win, Listener l);
void WindowOnMouse(Window* win, Listener l);
// Repeating timer. ms <= 0 stops it. GPUI's system_monitor does the same with
// a spawned task that sleeps and calls cx.notify().
void WindowSetInterval(Window* win, int ms, Listener l);

// Open a window whose root is a view entity, the WindowOpen + cx.new pair.
Window* WindowOpenView(App* app, const wchar_t* title, int dipW, int dipH,
                       EntityId root, AppWinOpts opts);
int AppRunView(const wchar_t* title, int dipW, int dipH, EntityId root,
               App* app, AppWinOpts opts);

// The view a window renders, typed.
template <typename T>
T* WindowRoot(Window* win) {
    return win ? (T*)EntityGet(win->app, win->root) : nullptr;
}

// Client size in DIPs; what onRender used to receive as WinSize.
WinSize WindowSize(Window* win);

App* AppNew();
void AppFree(App* app);
int AppRun(App* app);
Window* WindowOpen(App* app, const wchar_t* title, int dipW, int dipH,
                   AppWinOpts opts);
void AppSetTitle(Window* win, const wchar_t* title);
void AppRequestAnim(Window* win, bool on);

// Collect focusable click targets from last paint for Tab cycling.
void FocusCollect(Window* win, El* root);
int FocusNext(Window* win, int trapId, bool backward);
void AppQuit(Window* win);
void AppInvalidate(Window* win);
void AppMinimize(Window* win);
void AppToggleMaximize(Window* win);
void AppClose(Window* win);
void AppDrag(Window* win);
bool AppIsMaximized(Window* win);
} // namespace gpui
