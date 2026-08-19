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
// gpui::hsla. h/s/l/a are 0..1 and are clamped, so a lightness computed from
// scene coordinates cannot wrap around into a different hue.
Rgba RgbaHsla(float h, float s, float l, float a01);

constexpr float kAuto = -1.f;
constexpr float kFill = -2.f;
constexpr float kPi = 3.14159265358979f;

// ─── theme (Default Dark) ─────────────────────────────────────────────────

struct App;

struct Theme {
    Rgba background;
    Rgba foreground;
    Rgba border;
    Rgba mutedFg;
    // input.border, and theme.input_background(): the surface an input paints
    // itself on — the window background in light, the input border at 70% in
    // dark, the way Rust mixes it toward transparent.
    Rgba inputBorder;
    Rgba inputBg;
    // ring: the focus ring color. caret: the text cursor.
    Rgba ring;
    Rgba caret;
    Rgba titleBar;
    Rgba titleBarBorder;
    Rgba tabBar;
    Rgba tabActiveBg;
    Rgba tabActiveFg;
    Rgba tabFg;
    Rgba tableBg;
    Rgba tableHead;
    Rgba tableHeadFg;
    Rgba tableRowBorder;
    Rgba tableEven;
    Rgba progress;
    Rgba red;
    Rgba green;
    Rgba blue;
    Rgba yellow;
    Rgba cyan;
    Rgba magenta;
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
    // theme.overlay: what a dialog backdrop tints the page with. 5% black in
    // light, 20% in dark (default-theme.json).
    Rgba overlay;
    // group_box.background / group_box.foreground: the surface a filled
    // GroupBox puts its content on.
    Rgba groupBox;
    Rgba groupBoxFg;
    // description_list_label: the label cell of a DescriptionList.
    Rgba descListLabel;
    Rgba descListLabelFg;
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
// The theme belongs to App, the way Rust keeps it as a Global; read it with
// cx->theme(). ThemeNow() is the paint-time fallback for code below Ctx.
const Theme& ThemeNow();
void ThemeSet(App* app, ThemeMode mode);
ThemeMode ThemeGet();

// ─── entities ─────────────────────────────────────────────────────────────
//
// GPUI keeps view state in `App` and hands out `Entity<T>` handles; a view
// implements `Render` and mutates itself through `Context<T>`. The same shape
// here, minus the refcounting: `App` owns the state, `Entity<T>` is a POD
// generational handle, and `Ctx` is the one context type (GPUI splits it into
// `&mut App` / `&mut Window` / `&mut Context<T>` only to satisfy the borrow
// checker).

struct Window;
struct Ctx;
struct El;
// The shaped run a text element measured to; Paint.h owns the type.
struct TextLayout;

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
    // The box that was hit, so a handler can place the click inside it — what
    // a slider needs to turn a press on its track into a value.
    float elX = 0;
    float elY = 0;
    float elW = 0;
    float elH = 0;
    // How many presses this one is in an unbroken run: 1, 2, 3… GPUI's
    // MouseDownEvent::click_count, and what Rust's on_double_click tests —
    // `on_click(|ev, ..| if ev.click_count() == 2 { .. })`. A handler that
    // does not look at it sees every press, as it did before.
    int clickCount = 1;
};

// Portable key codes. The values are the Win32 VK_* ones, so the Windows
// window passes wParam straight through and the X11 window maps keysyms onto
// them. Only the keys the widgets react to are named.
enum {
    KeyBack = 8,
    KeyTab = 9,
    KeyReturn = 13,
    KeyShift = 16,
    KeyControl = 17,
    KeyMenu = 18,
    KeyEscape = 27,
    KeySpace = 32,
    KeyPageUp = 33,
    KeyPageDown = 34,
    KeyEnd = 35,
    KeyHome = 36,
    KeyLeft = 37,
    KeyUp = 38,
    KeyRight = 39,
    KeyDown = 40,
    KeyDelete = 46,
    // Letters and digits are their ASCII uppercase / digit codes.
    KeyA = 65,
    KeyC = 67,
    KeyV = 86,
    KeyX = 88
};

struct KeyEvent {
    int vk = 0;      // a Key* code, 0 for a typed character
    uint32_t ch = 0; // typed codepoint, 0 for key down/up
    bool down = false;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
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
    // The press count, as on ClickEvent. 1 for a move or a release.
    int clickCount = 1;
};

// The pointer shape the window asks the OS for. GPUI spells this
// CursorStyle and has a dozen; these are the two the element tree can tell
// apart today.
enum class CursorKind : uint8_t {
    Arrow,
    IBeam
};

struct WheelEvent {
    float x = 0;
    float y = 0;
    float delta = 0;
};

// Fired by a window timer; GPUI does this with cx.spawn + Timer::after.
struct TickEvent {
    int ms = 0;
};

// cx.listener(...): a handler plus the entity it runs against. Dispatch looks
// the entity up and drops the event if the handle went stale.
//
// `arg` is what the Rust closure would have captured — the tab index in
// `cx.listener(move |this, _, _, cx| this.tab = ix)`. Without it a view has to
// hand out element ids and decode them again in one big switch.
using ListenerFn = void (*)(void* self, Ctx* cx, const void* ev);
using ListenerArgFn = void (*)(void* self, Ctx* cx, const void* ev,
                               intptr_t arg);

struct Listener {
    void* fn = nullptr;
    EntityId view = {};
    intptr_t arg = 0;
    bool hasArg = false;

    bool IsValid() const { return fn != nullptr; }
};

// One armed timer. GPUI has no timer list: it spawns a task per timer and the
// Task handle cancels on drop. Here the window keeps them, and dispatch drops
// one whose view went stale — which is the same lifetime, spelled differently.
struct TimerSub {
    int id = 0; // what WindowCancelTimer takes
    int ms = 0;
    double dueAt = 0; // TimeNow() deadline
    bool repeat = false;
    Listener l;
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
    LoaderCircle,
    Ellipsis,
    ChevronsUpDown,
    SquareTerminal,
    BookOpen,
    Settings2,
    Frame,
    ChartPie,
    File,
    FolderOpen,
    ChevronDown,
    ChevronLeft,
    ChevronRight,
    ChevronUp,
    Check,
    Search,
    Minus,
    Plus,
    Copy,
    Bell,
    Star,
    StarFill,
    Eye,
    Heart,
    ArrowLeft,
    Building2,
    Asterisk,
    Sun,
    Maximize,
    Minimize,
    Map,
    Globe,
    Github,
    HeartOff,
};

struct PaintCtx;

struct ChartSeries {
    const float* ys = nullptr;
    int n = 0;
    int tickMargin = 15;
    // The x-axis labels, one per point; without them the index is drawn.
    const char* const* labels = nullptr;
    // A second series drawn over the first, as a stacked area chart does.
    bool overlay = false;
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
    // w_1_2 / w_2_3 / …: a fraction of the parent's content box, which GPUI
    // has as first-class widths. 0 = unset.
    float widthFrac = 0;
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
    float borderL = 0;
    float borderR = 0;
    float radius = 0;
    Rgba bg = {};
    Rgba borderColor = {};
    Rgba color = {};
    float fontSize = 0; // 0 = inherit
    // line_height as a multiple of the font size. 0 = GPUI's default, phi.
    float lineHeight = 0;
    bool truncate = false;
    bool wrap = false;
    // flex_wrap on a row: children that do not fit start a new line.
    bool flexWrap = false;
    bool hasBg = false;
    bool hasColor = false;
    bool fontBold = false;
    bool fontSemibold = false;
    bool fontMedium = false; // font_medium(): DWrite weight 500
    bool fontMono = false;   // font_family("Consolas")
    bool underline = false;  // text_decoration_1()
    bool italic = false;     // *emphasis*
    bool borderDashed = false;
    // Dash on/off lengths for a dashed border, in stroke widths. GPUI's
    // border_dashed draws 2 on, 1 off; a dashed Separator paints its own path
    // with 4 on, 2 off.
    float dashOn = 2;
    float dashOff = 1;
    bool absolute = false;
    bool fixed = false; // out-of-flow in window coords (Rust deferred overlay)
    // Laid out where it sits, painted after everything else — GPUI's
    // deferred(): a popup anchored to its trigger still draws over the page
    // below it, and hit-tests before it.
    bool deferred = false;
    bool anchorBelow = false;   // absolute, just under the parent box
    bool anchorAbove = false;   // absolute, just over it
    bool anchorCenterX = false; // absolute, centered on the parent box
    float anchorGap = 0;
    float absTop = kAuto, absLeft = kAuto, absBottom = kAuto, absRight = kAuto;
    Rgba hoverBg = {};
    bool hasHoverBg = false;
    // hover(|style| style.text_color(..)): what the subtree under a hovered
    // element paints with, for the descendants that set no color of their own.
    Rgba hoverFg = {};
    bool hasHoverFg = false;
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
    // The shaped run LayoutEl measured, borrowed from the text cache so the
    // paint pass can draw it without looking it up a second time. Owned by
    // the cache, which cannot drop it before the frame ends; null when the
    // element has no text or the run could not be cached.
    TextLayout* laidLayout = nullptr;
    // Layout memo. LayoutEl runs a subtree up to three times per parent pass
    // (measure, shrink-wrap, clamp) and LayoutChildren used to re-run one just
    // to move it, which multiplies out to an exponential in tree depth. Layout
    // is a pure function of (availW, availH, inherited font, inherited color)
    // for a given frame, so the second call with the same inputs replays the
    // recorded result and translates the subtree to its new origin instead.
    float memoAvailW = 0;
    float memoAvailH = 0;
    float memoFont = 0;
    Rgba memoFg = {};
    float memoW = 0;
    float memoH = 0;
    float memoContentW = 0;
    float memoContentH = 0;
    bool memoValid = false;

    El* FlexRow();
    El* FlexCol();
    El* FlexWrap();
    El* Grow(float g = 1);
    El* Shrink0();
    El* W(float v);
    El* WFrac(float f);
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
    El* ItemsEnd();
    El* JustifyBetween();
    El* JustifyCenter();
    El* JustifyEnd();
    El* JustifyStart();
    El* Bg(Rgba c);
    El* Border(float width, Rgba c);
    El* BorderT(float width, Rgba c);
    El* BorderB(float width, Rgba c);
    El* BorderL(float width, Rgba c);
    El* BorderR(float width, Rgba c);
    El* Radius(float r);
    El* Fg(Rgba c);
    El* Font(float px);
    El* LineHeight(float mult);
    El* Truncate();
    El* ClipY();
    El* ScrollY(float off);
    El* ScrollId(int v);
    El* Click(int v);
    El* OnClick(Func0 fn);
    El* OnClick(Listener l);
    El* Child(El* c);
    El* Bold();
    El* Semibold();
    El* Medium();
    El* Mono();
    El* Underline();
    El* Italic();
    El* Selectable();
    El* Wrap();
    El* Dashed();
    El* DashArray(float on, float off);
    El* Absolute();
    El* Fixed();
    El* Deferred();
    El* AnchorBelow(float gap = 0);
    El* AnchorAbove(float gap = 0);
    El* AnchorCenterX();
    El* Top(float v);
    El* Left(float v);
    El* Bottom(float v);
    El* Right(float v);
    El* HoverBg(Rgba c);
    El* HoverFg(Rgba c);
    El* FocusId(int v);
    El* TrapId(int v);
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

// The 2D backend. Direct2D + DirectWrite on Windows, cairo + Pango on Linux;
// both are opaque here and only Paint_win.cpp / Paint_linux.cpp look inside.
// `PaintApp` is the process-wide half (factories, fonts), `PaintTarget` the
// per-window drawing surface.
struct PaintApp;
struct PaintTarget;

struct PaintCtx {
    PaintApp* pa = nullptr;
    PaintTarget* rt = nullptr;
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

// gpui_component::input::InputEvent. Change is the only variant raised so
// far; PressEnter / Focus / Blur have no subscriber here yet.
enum class InputEventKind : uint8_t {
    Change
};

struct InputEvent {
    InputEventKind kind = InputEventKind::Change;
};

// GPUI's InputState: the text a themed Input is bound to. `onChange` is what
// Rust spells cx.subscribe(&input_state, |ev: &InputEvent| ...) — the window
// fires it after an edit.
struct LineInput {
    char buf[512] = {};
    int len = 0;
    int cursor = 0;
    char placeholder[128] = {};
    bool focused = false;
    Listener onChange = {};
    // This field's caret clock, the counterpart of InputState::blink_cursor.
    // Created on first use, so a LineInput stays a plain value.
    EntityId blink = {};
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
                 float* outH, bool wrap = false, int weight = 0,
                 float lineH = 0);
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
// crates/base/src/text_boundary.rs. The byte range of the word around `off`
// — a run of word characters, or the run of spaces when the offset is in
// one, or the single character otherwise. False when there is nothing there.
bool TextWordRangeAt(Str s, int off, int* outA, int* outB);
// The same for the line: back to the previous newline, on to the next.
void TextLineRangeAt(Str s, int off, int* outA, int* outB);
// points_for_multi_click: the document range a press of `clickCount` selects
// under (x, y) — 2 takes the word, 3 or more the whole run — in the same
// offsets TextHitOffsetAt and CopyTextHits speak. False for a single click,
// or when no selectable text is there.
bool TextMultiClickRange(PaintCtx* ctx, float x, float y, int clickCount,
                         int* outA, int* outB);
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

struct WinOpts {
    bool borderless = false;
    // The view draws the title bar. Cocoa keeps its traffic-light controls
    // above a transparent full-size content view; Windows drops the caption
    // but keeps the rest of the frame, and X11 drops the frame outright. On
    // all three the view supplies the title-bar background, its drag region
    // and — off macOS — the minimize / maximize / close controls, which is
    // what component::TitleBar builds.
    bool clientTitleBar = false;
    bool anim = false;
    int timerMs = 500;
};

// gpui::FrameTiming. One drawn frame, as measured by the window itself, so the
// FPS HUD reports what the runtime actually spent rather than an approximation
// taken from the outside. GPUI gates recording behind
// `set_frame_trace_enabled`; here it is two QPC reads per frame and always on.
struct FrameTiming {
    float drawSecs = 0;
};

enum {
    kFrameTraceCap = 256
};

// Process-wide state: the Direct2D / DirectWrite factories, the shared font
// cache, the entity store and the open windows. GPUI's `App`.
struct App {
    PaintApp* paint = nullptr;
    ThemeMode themeMode = ThemeMode::Light;
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
// The OS window: an HWND wrapper on Windows, an X11 Window on Linux.
struct PlatWindow;

struct Window {
    App* app = nullptr;
    PlatWindow* plat = nullptr;
    PaintCtx paint = {};
    Arena* frameArena = nullptr;
    // The view this window renders. GPUI's Window holds a root view too.
    EntityId root = {};
    int hoverId = 0;
    int focusId = 0;
    float mouseX = 0;
    float mouseY = 0;
    // What the pointer looks like right now; the OS is only told on a change.
    CursorKind cursor = CursorKind::Arrow;
    bool maximized = false;
    bool running = true;
    bool anim = false;
    bool mouseDown = false;
    // The multi-click run in progress: when the last press landed, where, and
    // with which button, so WindowClickCount can tell the next press apart
    // from a second click. GPUI keeps the same three in its platform layer.
    double lastDownAt = 0;
    float lastDownX = 0;
    float lastDownY = 0;
    int lastDownButton = 0;
    int clickRun = 0;
    bool eatReturn = false;
    LineInput* input = nullptr;
    Overlay overlay = {};
    MenuState menu = {};
    Vec<FocusRect> focusEls;
    Vec<KeyedSlot> keyed;
    WinOpts opts = {};
    // Window-level subscriptions bound to view entities.
    Listener onKey = {};
    Listener onWheel = {};
    Listener onClick = {};
    Listener onMouse = {};
    // Armed timers, any number of them.
    Vec<TimerSub> timers;
    int nextTimerId = 1;
    // Which LineInput had focus last frame, so the runtime can start and stop
    // its caret without every app wiring that up.
    LineInput* prevInput = nullptr;
    // Ring of the last kFrameTraceCap draw times; frameSeq counts every frame
    // ever drawn and is what a collector cursors on.
    FrameTiming frameTrace[kFrameTraceCap] = {};
    uint64_t frameSeq = 0;

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

    // cx.theme() — the colors every widget reads.
    const Theme& theme() const;
    ThemeMode themeMode() const;
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
    l.fn = (void*)fn;
    l.view = cx->self;
    return l;
}

// cx.listener(move |this, ...| ... ix ...): same, carrying a captured value.
template <typename T, typename E>
Listener Listen(Ctx* cx, void (*fn)(T*, Ctx*, const E*, intptr_t),
                intptr_t arg) {
    Listener l;
    l.fn = (void*)fn;
    l.view = cx->self;
    l.arg = arg;
    l.hasArg = true;
    return l;
}

// A handler that takes a value the component supplies: which day of the
// calendar, which combobox row. The component fills it with ListenerArg.
template <typename T, typename E>
Listener Listen(Ctx* cx, void (*fn)(T*, Ctx*, const E*, intptr_t)) {
    Listener l;
    l.fn = (void*)fn;
    l.view = cx->self;
    l.hasArg = true;
    return l;
}

// Bind the value a component hands its caller. This is what a Rust closure
// gets as its event payload: `.on_click(cx.listener(|this, day, _, cx| ...))`.
inline Listener ListenerArg(Listener l, intptr_t arg) {
    if (l.IsValid()) {
        l.arg = arg;
        l.hasArg = true;
    }
    return l;
}

// Same, but bound to another entity instead of the one that is rendering.
template <typename T, typename E>
Listener ListenTo(Entity<T> e, void (*fn)(T*, Ctx*, const E*)) {
    Listener l;
    l.fn = (void*)fn;
    l.view = e.id;
    return l;
}

template <typename T, typename E>
Listener ListenTo(Entity<T> e, void (*fn)(T*, Ctx*, const E*, intptr_t),
                  intptr_t arg) {
    Listener l;
    l.fn = (void*)fn;
    l.view = e.id;
    l.arg = arg;
    l.hasArg = true;
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
// Fires for a click no element handled — the outside click that dismisses an
// overlay. Elements carry their own listener; this is not a dispatch table.
void WindowOnUnhandledClick(Window* win, Listener l);
void WindowOnMouse(Window* win, Listener l);
// Repeating timer; GPUI's system_monitor does the same with a spawned task
// that sleeps and calls cx.notify(). Returns a handle, or 0. Any number may
// be armed at once.
int WindowSetInterval(Window* win, int ms, Listener l);
// Fires once, then forgets itself. GPUI's Timer::after.
int WindowSetTimeout(Window* win, int ms, Listener l);
void WindowCancelTimer(Window* win, int id);

// ─── caret ────────────────────────────────────────────────────────────────
//
// Port of crates/base/src/input/base/blink_cursor.rs. A blinking caret is
// state, not a function of the clock: something flips it on a 500 ms timer
// and every repaint in between shows what the last flip decided. Sampling the
// clock at paint time instead makes the caret invisible whenever nothing
// happens to repaint during the lit half.
//
// One per text field, the way Rust gives every InputState its own
// Entity<BlinkCursor>. `handle` is an EntityId the owner keeps; the first
// Start creates the entity behind it.

struct BlinkCursor {
    bool visible = false;
    bool paused = false; // solid, because the user is typing
    // The armed timer. Cancelling it is what Rust's epoch counter does.
    int timer = 0;

    static void OnFlip(BlinkCursor* self, Ctx* cx, const TickEvent* ev);
    static void OnResume(BlinkCursor* self, Ctx* cx, const TickEvent* ev);
};

// Idempotent. Rust calls these from on_focus / on_blur.
void BlinkStart(App* app, Window* win, EntityId* handle);
void BlinkStop(App* app, Window* win, EntityId* handle);
// Keep it solid, then resume blinking shortly after — Rust's
// pause_blink_cursor, called from every edit and cursor movement.
void BlinkPause(App* app, Window* win, EntityId* handle);
// What a text widget asks before drawing its caret. Rust:
// blink_cursor.read(cx).visible().
bool BlinkVisible(App* app, EntityId handle);

// The same, when a Ctx is already in hand — which it is inside any Render.
inline void BlinkStart(Ctx* cx, EntityId* handle) {
    BlinkStart(cx->app, cx->win, handle);
}
inline void BlinkStop(Ctx* cx, EntityId* handle) {
    BlinkStop(cx->app, cx->win, handle);
}
inline void BlinkPause(Ctx* cx, EntityId* handle) {
    BlinkPause(cx->app, cx->win, handle);
}
inline bool BlinkVisible(Ctx* cx, EntityId handle) {
    return BlinkVisible(cx->app, handle);
}

// Open a window whose root is a view entity, the WindowOpen + cx.new pair.
Window* WindowOpenView(App* app, Str title, int dipW, int dipH, EntityId root,
                       WinOpts opts);
int AppRunView(Str title, int dipW, int dipH, EntityId root, App* app,
               WinOpts opts);

// The view a window renders, typed.
template <typename T>
T* WindowRoot(Window* win) {
    return win ? (T*)EntityGet(win->app, win->root) : nullptr;
}

// Client size in DIPs; what onRender used to receive as WinSize.
WinSize WindowSize(Window* win);

// FrameTimingCollector::collect_unseen: copy the frames drawn since *cursor
// into `out` and advance the cursor. Frames dropped from the ring while the
// caller was away are skipped. Returns how many were written.
int WindowCollectFrames(Window* win, uint64_t* cursor, FrameTiming* out,
                        int max);

// Monotonic seconds since the first call. GPUI's `Instant`, which the FPS
// readouts need at a finer resolution than GetTickCount64's ~16 ms.
double TimeNow();

App* AppNew();
void AppFree(App* app);

// Put UTF-8 text on the system clipboard.
void ClipboardSetText(Window* win, Str text);
int AppRun(App* app);
Window* WindowOpen(App* app, Str title, int dipW, int dipH, WinOpts opts);
void AppSetTitle(Window* win, Str title);
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

// The entry point every example implements. The platform half of the runtime
// provides wWinMain / main and calls this, so no example spells out either.
// Global scope, so an example that says `using namespace gpui;` can define it
// without qualifying the name.
int GpuiMain(int argc, char** argv);
