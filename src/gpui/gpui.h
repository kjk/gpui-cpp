/* C++ GPUI subset used by system_monitor. Frame-rebuilt element tree. */

#include "base.h"

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
// The other direction. h/s/l come back 0..1, the way gpui::Hsla holds them.
void RgbaToHsla(Rgba c, float* h, float* s, float* l);
// Colorize::hue: the same color turned to a new hue, keeping its saturation,
// lightness and alpha.
Rgba RgbaWithHue(Rgba c, float h01);

// The text-field engine, in the input section below. El and HitRect name one
// before it is defined, the way they name SliderState.
struct InputState;

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
    // list.active.background / list.active.border, and the table pair that
    // falls back to them (theme/schema.rs). What ListSettings::active_highlight
    // picks instead of plain `accent` for a selected row: a translucent tint
    // with a solid rule around it, rather than a filled block.
    Rgba listActive;
    Rgba listActiveBorder;
    Rgba tableActive;
    Rgba tableActiveBorder;
    Rgba progress;
    Rgba red;
    Rgba green;
    Rgba blue;
    Rgba yellow;
    Rgba cyan;
    Rgba magenta;
    // chart_1..chart_5, and the pair a candlestick closes on. Both themes
    // give them the same five blues (default-theme.json).
    Rgba chart1;
    Rgba chart2;
    Rgba chart3;
    Rgba chart4;
    Rgba chart5;
    Rgba chartBullish;
    Rgba chartBearish;
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
    Rgba sidebarAccent;
    Rgba sidebarAccentFg;
    Rgba sidebarBorder;
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
// Theme::font_size and Theme::radius, which the story's Appearance menu
// writes the way Rust writes `Theme::global_mut(cx).font_size`. The themes
// here are shared statics rather than a per-app Global, so a change is the
// process', not one window's. `radius_lg` follows Rust's rule: two more than
// the radius, or nothing when the radius is nothing.
void ThemeSetRadius(float radius);
// The root font size every element inherits from, and what an explicit size
// is measured against: a `Font(12)` is twelve at the default 16, and grows
// with it, which is what Rust gets for free by spelling its sizes in rems.
float ThemeFontSize();
void ThemeSetFontSize(float px);
// The theme belongs to App, the way Rust keeps it as a Global; read it with
// cx->theme(). ThemeNow() is the paint-time fallback for code below Ctx.
// Theme::focus_ring. The ring is painted outside the element's border, so an
// ancestor that clips its content cuts it off; an application whose layout
// clips heavily turns it off here and keeps the tinted border, which takes no
// room. Like the scrollbar mode and the font size, it is one process-wide
// setting rather than one window's Global.
bool ThemeFocusRing();
void ThemeSetFocusRing(bool on);
const Theme& ThemeNow();
void ThemeSet(App* app, ThemeMode mode);
ThemeMode ThemeGet();

// ─── geometry ───────────────────────────────────────────────────────────
//
// crates/gpui/src/geometry.rs. Rust spells these `Point<T>`, `Size<T>`,
// `Bounds<T>` and `Edges<T>`, where `T` is not an element type but a *unit* —
// `Pixels`, `ScaledPixels`, `DevicePixels`, `Rems`, `Length` — so the compiler
// refuses to add device pixels to logical ones. Everything above Paint.h here
// is DIPs and always has been, which leaves that generic with one instantiation
// (`Point<Pixels>` is 170 of the 185 `Point<T>` in gpui-component), so these
// are plain float structs and the arithmetic is written out once. The units
// that are not DIPs get their own named struct instead of a parameter:
// `WinSize` carries both the DIP and the device-pixel size of a window, and the
// backends scale on the way to Direct2D / cairo / Core Graphics.
//
// They are values: aggregates, no constructors, copied by the byte. Code that
// reads or writes one component at a time — the layout pass over `El`, a mouse
// event's position — keeps its flat fields; these are for what is produced,
// returned or passed as a unit.

// gpui::Axis, from the same file. `Along` is the field it picks out.
enum class Axis : uint8_t {
    Horizontal,
    Vertical
};

// crates/base/src/geometry.rs: which edge of the window something hangs off.
enum class Side : uint8_t {
    Left,
    Right
};

inline bool SideIsLeft(Side s) {
    return s == Side::Left;
}

inline bool AxisIsHorizontal(Axis a) {
    return a == Axis::Horizontal;
}

struct Point {
    float x = 0;
    float y = 0;
};

struct Size {
    float w = 0;
    float h = 0;
};

// Edges<Pixels>: the four insets of a box, in Rust's field order. Ahead of
// Bounds, which insets by one.
struct Edges {
    float top = 0;
    float right = 0;
    float bottom = 0;
    float left = 0;

    float Horizontal() const { return left + right; }
    float Vertical() const { return top + bottom; }
};

// Bounds<Pixels>. Rust composes it from an origin and a size; here the four
// floats are the struct, so there is no `.origin` or `.size` to reach for.
struct Bounds {
    float x = 0, y = 0, w = 0, h = 0;

    float Right() const { return x + w; }
    float Bottom() const { return y + h; }
    float CenterX() const { return x + w * 0.5f; }
    float CenterY() const { return y + h * 0.5f; }
    // Bounds::contains: the top and left edges are inside, the bottom and
    // right ones are not, so abutting boxes never both claim a point.
    bool Contains(Point p) const {
        return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
    }
    // Bounds::inset, which is Bounds::dilate with the amount negated, so a
    // positive amount shrinks the box.
    Bounds Inset(float d) const { return {x + d, y + d, w - d - d, h - d - d}; }
    // Bounds::extend, negated the same way: the content box inside padding.
    Bounds Inset(Edges e) const {
        return {x + e.left, y + e.top, w - e.Horizontal(), h - e.Vertical()};
    }
};

// Bounds::new(origin, size), for the callers that hold the two apart.
inline Bounds BoundsAt(Point origin, Size size) {
    return {origin.x, origin.y, size.w, size.h};
}

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
struct SliderState;
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

// Where one transition has got to, kept per window and per id. Separate from
// KeyedSlot because the lifetime is GPUI's element state rather than Rust's
// keyed state: a slot nothing asked for while the frame was built is dropped,
// so a dialog that closes and opens again animates its way in a second time.
struct MotionSlotRec {
    uint32_t key = 0;
    // The frame that last asked for it. `frameSeq` at the time, so the sweep
    // is a comparison rather than a flag to clear.
    uint64_t frame = 0;
    void* ptr = nullptr;
};

// window.use_keyed_state: per-window state owned by a RenderOnce element that
// has nowhere else to keep it.
struct KeyedSlot {
    uint32_t key = 0;
    void* ptr = nullptr;
    DropFn drop = nullptr;
    // Set when the slot was taken through KeyedEntity: the app owns the
    // memory then, and the window only remembers which entity the key means.
    EntityId entity = {};
};

// ─── mouse input ──────────────────────────────────────────────────────────
// crates/gpui/src/interactive.rs, field for field, minus the four things a
// C++ struct cannot say the same way:
//   * `MouseButton::Navigate(NavigationDirection)` carries its direction; a
//     C++ enumerator cannot, so the two directions are their own constants.
//   * `Option<MouseButton>` on a move is a `pressed` flag plus the button.
//   * `ScrollDelta::Pixels | Lines` is a delta plus `precise`. Rust defers the
//     multiply to `pixel_delta(line_height)`; the three windows here turn a
//     notch into DIPs at the seam, so nothing downstream needs a line height.
//   * `Point<Pixels>` is `x` and `y`. There is a `Point` above, but an
//     event's position is read a component at a time, and flattening it
//     spares every handler a `.position`.
// What is missing outright: touch, pinch and pressure, which none of these
// three windows report.

enum class MouseButton : uint8_t {
    Left,
    Right,
    Middle,
    NavigateBack,
    NavigateForward
};

// GPUI's Modifiers. `platform` is the Windows key, X11's Super and macOS's
// Command; `function` is Fn, which only macOS reports.
struct Modifiers {
    bool control = false;
    bool alt = false;
    bool shift = false;
    bool platform = false;
    bool function = false;

    bool Modified() const {
        return control || alt || shift || platform || function;
    }
    // The semantically secondary modifier: Command on macOS, Control on the
    // other two — Modifiers::secondary().
    bool Secondary() const {
#if GPUI_OS_MAC
        return platform;
#else
        return control;
#endif
    }
    int Count() const {
        return (int)control + (int)alt + (int)shift + (int)platform +
               (int)function;
    }
};

// The phase of a scroll gesture. A wheel notch is one Moved; a trackpad on
// macOS runs Started -> Moved -> Ended.
enum class TouchPhase : uint8_t {
    Started,
    Moved,
    Ended,
    Cancelled
};

struct MouseDownEvent {
    MouseButton button = MouseButton::Left;
    float x = 0;
    float y = 0;
    // The box of the element the press landed on, when it has an identity —
    // the same thing ClickEvent::el carries, and what a handler needs to
    // place something where the press was inside it. A Rust hitbox has the
    // bounds too; the event does not, because the closure already has them.
    Bounds el = {};
    Modifiers modifiers = {};
    // How many presses this one is in an unbroken run: 1, 2, 3… What Rust's
    // on_double_click tests — `on_click(|ev, ..| ev.click_count() == 2)`.
    int clickCount = 1;
    // The press that also activated the window. Windows knows from
    // WM_MOUSEACTIVATE; X11 has no such notion and a Cocoa view does not
    // accept the first mouse, so it is false on those two.
    bool firstMouse = false;

    // MouseDownEvent::is_focusing.
    bool IsFocusing() const { return button == MouseButton::Left; }
};

struct MouseUpEvent {
    MouseButton button = MouseButton::Left;
    float x = 0;
    float y = 0;
    Modifiers modifiers = {};
    int clickCount = 1;

    bool IsFocusing() const { return button == MouseButton::Left; }
};

struct MouseMoveEvent {
    float x = 0;
    float y = 0;
    // Rust's Option<MouseButton>: `pressed` is the Some, `pressedButton` its
    // value. With no button down, pressedButton means nothing.
    bool pressed = false;
    MouseButton pressedButton = MouseButton::Left;
    Modifiers modifiers = {};

    // MouseMoveEvent::dragging.
    bool Dragging() const {
        return pressed && pressedButton == MouseButton::Left;
    }
};

// Two strings with the same bytes. `Str` is a pointer and a length, so a
// kind that names a drag is compared by what it says, not by where it lives.
inline bool StrSame(Str a, Str b) {
    return a.len == b.len &&
           (a.len == 0 || memcmp(a.s, b.s, (size_t)a.len) == 0);
}

// What a drag carries. Rust's `on_drag(payload, ..)` takes a value of any
// type and `DragMoveEvent<T>` only reaches the handlers that named that type;
// there is no type to match on here, so the payload says what kind of thing
// is being dragged by name and which one of that kind by index.
struct DragPayload {
    Str kind = {};
    int ix = 0;
    void* data = nullptr;

    bool IsValid() const { return kind.s != nullptr; }
};

// DragMoveEvent<T>: what is being dragged, and the move that carried it.
struct DragMoveEvent {
    DragPayload drag = {};
    MouseMoveEvent event = {};
    // The dragged element's box, which is `bounds` on the entity the drag
    // names in Rust. It is the box the last frame laid out, so a handler that
    // moves the element reads its own answer back on the next move.
    Bounds el = {};
};

// on_drop::<T>: a drag that let go over this element. Rust matches the drop
// handler by the payload's type; here the element says which `kind` it takes,
// and a drag carrying anything else passes over it as if it were not there.
struct DropEvent {
    DragPayload drag = {};
    // Where the button came up, in window coordinates.
    float x = 0;
    float y = 0;
    // The box of the element that took the drop, so a handler can work out
    // where inside itself the drop landed.
    Bounds el = {};
};

// The pointer left the window. GPUI's MouseExitEvent is a MouseMoveEvent in
// all but name — it derefs to one — so it carries the same three things.
struct MouseExitEvent {
    float x = 0;
    float y = 0;
    bool pressed = false;
    MouseButton pressedButton = MouseButton::Left;
    Modifiers modifiers = {};
};

struct ScrollWheelEvent {
    float x = 0;
    float y = 0;
    // DIPs, already scaled: one wheel notch is 48. Positive scrolls the view
    // up and left, which is the sign WM_MOUSEWHEEL reports.
    float deltaX = 0;
    float deltaY = 0;
    // ScrollDelta::Pixels rather than ::Lines — a trackpad or another device
    // that measures the gesture itself, not a wheel with detents.
    bool precise = false;
    Modifiers modifiers = {};
    TouchPhase phase = TouchPhase::Moved;
};

// GPUI's PlatformInput: what a platform window hands to the window layer.
// Rust's enum carries its payload; here a kind and a union of the same structs
// do. Only the mouse variants exist — keys still arrive through WindowKeyDown
// and WindowChar, whose KeyEvent is a merged key-and-character event rather
// than GPUI's Keystroke, and nothing here produces a file drop or a gesture.
enum class PlatformInputKind : uint8_t {
    MouseDown,
    MouseUp,
    MouseMove,
    MouseExited,
    ScrollWheel
};

struct PlatformInput {
    PlatformInputKind kind = PlatformInputKind::MouseMove;
    union {
        MouseDownEvent mouseDown = {};
        MouseUpEvent mouseUp;
        MouseMoveEvent mouseMove;
        MouseExitEvent mouseExited;
        ScrollWheelEvent scrollWheel;
    };
};

// GPUI's ClickEvent is the down and up pair (ClickEvent::Mouse) or the Enter
// or Space that activated a focused element (ClickEvent::Keyboard). This one
// is flat: it fires from the release, like Rust's, and carries the position
// the release landed at with the count and the modifiers the press had. It
// also carries what our hit rect knows and a Rust hitbox does not have to —
// which element this was, and where it is.
struct ClickEvent {
    float x = 0;
    float y = 0;
    MouseButton button = MouseButton::Left;
    // The element's click id, when it has one. Lets one handler serve a list.
    int id = 0;
    // The box that was hit, so a handler can place the click inside it — what
    // a slider needs to turn a press on its track into a value. This is also
    // KeyboardClickEvent::bounds, the only position a keyboard click has.
    Bounds el = {};
    int clickCount = 1;
    Modifiers modifiers = {};
    // ClickEvent::Keyboard: Space or Enter on the focused element, with no
    // pointer anywhere near it. ClickEvent::is_keyboard.
    bool keyboard = false;
    // KeyboardClickEvent::button: which of the two activated it, KeyReturn or
    // KeySpace. 0 when the pointer made this click.
    int keyboardKey = 0;
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
    KeyE = 69,
    KeyV = 86,
    KeyX = 88,
    KeyY = 89,
    KeyZ = 90
};

struct KeyEvent {
    int vk = 0;      // a Key* code, 0 for a typed character
    uint32_t ch = 0; // typed codepoint, 0 for key down/up
    bool down = false;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

// The pointer shape the window asks the OS for. GPUI spells this
// CursorStyle and has a dozen; these are the two the element tree can tell
// apart today.
enum class CursorKind : uint8_t {
    Arrow,
    IBeam,
    // cursor_pointer: the hand, which says a thing is there to be clicked.
    // GPUI's own default for a div is the arrow, so this is opt-in; Rust asks
    // for it on links and on the button variants that look like one.
    Pointer,
    // cursor_col_resize, which a table's column edge asks for.
    ColResize,
    // cursor_row_resize: the handle between two panels stacked one over the
    // other.
    RowResize
};

// Fired by a window timer; GPUI does this with cx.spawn + Timer::after.
struct TickEvent {
    int ms = 0;
};

// What GPUI's `on_hover` hands its closure: a `&bool` saying whether the
// pointer just entered the element or just left it. It fires on the change,
// not on every move inside.
struct HoverEvent {
    bool hovered = false;
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
    // The handler takes an argument at all.
    bool hasArg = false;
    // The caller already said what it is. Rust hands a closure both what it
    // captured and what the widget produced; there is one slot here, so a
    // value the caller bound wins and ListenerFill leaves it alone.
    bool argBound = false;

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
    Icon,
    // gpui's img(..): a decoded bitmap, sized by its own pixels unless the
    // caller says otherwise. An image whose source cannot be decoded — a
    // remote URL, a format the platform does not read — paints its `text`
    // instead, which is the alt text a document gave it.
    Image
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
// gpui's Overflow, per axis: `overflow_hidden` clips and
// `overflow_x_scroll` / `overflow_y_scroll` scroll.
enum class Overflow : uint8_t {
    Visible,
    Hidden,
    Scroll
};

// ScrollbarMode, crates/base/src/scrollbar.rs. Rust's default is Scrolling —
// shown while scrolling, fading out after two idle seconds — which needs an
// animation clock per area; it is not ported, and a caller that asks for it
// gets the bar whenever the pointer is over the area.
enum class ScrollbarMode : uint8_t {
    Always,
    Hover
};

// The default Theme::scrollbar_mode. An element that names its own wins, the
// way `Scrollbar::new().mode(..)` overrides the theme's.
ScrollbarMode ScrollbarModeNow();
void ScrollbarModeSet(ScrollbarMode m);

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
    PanelLeftOpen,
    PanelLeftClose,
    PanelRightOpen,
    PanelRightClose,
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
    Palette,
    Copy,
    Bell,
    Star,
    StarFill,
    Eye,
    EyeOff,
    Heart,
    ArrowLeft,
    ArrowRight,
    Building2,
    Asterisk,
    Sun,
    Moon,
    Play,
    Maximize,
    Minimize,
    Map,
    Globe,
    Github,
    ExternalLink,
    HeartOff,
};

struct PaintCtx;

// A run of a text element painted differently from the rest of it: GPUI's
// HighlightStyle over a range, which is what a syntax highlighter's captures
// and an editor's TextDecorations both come to. The runs are UTF-8 offsets
// into the element's own text and must not overlap; whatever they leave over
// paints in the element's own colour.
//
// Weight and slant are not here. A run drawn in another face would shape to
// other widths, and every run of an element shares one shaped layout — the
// colour, the wash behind it and the rule under it are what can change
// without re-shaping.
struct TextSpan {
    int lo = 0;
    int hi = 0;
    Rgba color = {};
    // The wash behind the run, painted before the glyphs. Alpha 0 is none,
    // which is what a run that only recolours its glyphs wants — and the
    // reason it is spelled out: an Rgba defaults to opaque.
    Rgba bg = {0, 0, 0, 0};
    // UnderlineStyle: a rule under the run in its own colour.
    bool underline = false;
};

// Which of crates/ui/src/chart's charts this series is. They share the axis,
// the grid and the labels; what differs is the shape drawn over them.
enum class ChartKind : uint8_t {
    Area,
    Line,
    Bar,
    Candlestick,
    Radar
};

// plot::StrokeStyle. How a run of points is joined: the Catmull-Rom curve
// GPUI draws by default, straight segments, or a stair that steps after each
// point.
enum class ChartStroke : uint8_t {
    Natural,
    Linear,
    StepAfter
};

// BarAlignment: which edge of the plot a bar grows from. Bottom is the usual
// column; Left and Right lay the bands down the side and make it a row chart.
enum class BarAlign : uint8_t {
    Bottom,
    Top,
    Left,
    Right
};

struct ChartSeries {
    ChartKind kind = ChartKind::Area;
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
    // The value domain the y axis is scaled to. Both zero takes it from the
    // data, which is what a ScaleLinear over the data's own extent does; the
    // system monitor's charts say 0..100 instead.
    float domainMin = 0;
    float domainMax = 0;
    // Candlestick: the other three values per point, and the two colors a
    // candle takes depending on which way it closed.
    const float* opens = nullptr;
    const float* highs = nullptr;
    const float* lows = nullptr;
    Rgba up = {};
    Rgba down = {};
    // Bar: ScaleBand's inner padding, and how round the top of a bar is.
    float bandPadding = 0.2f;
    float barRadius = 4;
    BarAlign barAlign = BarAlign::Bottom;
    // Stack: where each bar starts, so a series drawn over another one sits
    // on top of it rather than in front of it. Null starts every bar at zero.
    const float* bases = nullptr;
    // BarChart::label: the value written at the bar's growing end.
    bool barLabels = false;
    // BarChart::fill(|d, ..|): a colour per bar rather than one for the lot.
    const Rgba* barFills = nullptr;
    // BarChart::fill_gradient: the two stops a bar is filled between. Run
    // across the chart's own range by default, or down each bar on its own
    // when barGradientPerBar is set.
    bool barGradient = false;
    bool barGradientPerBar = false;
    // fill(|_, bar, chart, _|): one ramp across the whole plot, on its
    // bottom-left to top-right diagonal, with every bar filled by the slice
    // of it that falls under its own footprint. The stops then differ from
    // bar to bar, which the other two modes' single pair cannot express.
    bool barGradientDiagonal = false;
    Rgba barFillFrom = {};
    Rgba barFillTo = {};
    // StrokeStyle and LineChart::dot, both of which the area chart shares.
    ChartStroke strokeStyle = ChartStroke::Natural;
    bool dot = false;
    // CandlestickChart::body_width_ratio: how much of a band the body takes.
    float bodyWidthRatio = 0.8f;
    // RadarChart::outer_radius / grid_levels, and its own dot flag.
    float radarRadius = 0;
    int gridLevels = 4;
    // AreaChart::id in Rust: a chart with one takes the pointer, and shows a
    // crosshair and a tooltip for whatever it is over.
    bool tooltip = false;
    // The name the tooltip's row goes by.
    Str name = {};
};

struct Style {
    FlexDir dir = FlexDir::Row;
    Align align = Align::Stretch;
    Justify justify = Justify::Start;
    Overflow overflowY = Overflow::Visible;
    Overflow overflowX = Overflow::Visible;
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
    Edges pad = {};
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
    // Transformation::rotate: turns clockwise about the element's own centre,
    // where 1 is a whole one. Only an icon reads it — a rotated box would want
    // the whole element tree in on it, and nothing in the port asks for one.
    float rotate = 0;
    // Style::opacity. 1 is untouched; anything less fades this element and
    // everything under it.
    float opacity = 1;
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
    // text_decoration_line_through(): a ~~del~~ run, an HTML <s> or <del>.
    bool strike = false;
    bool italic = false; // *emphasis*
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
    // left(relative(f)) / right(relative(f)): the offset is that fraction of
    // the parent's width, added to the pixel one. A stepper's connector needs
    // it to reach from the middle of one step to the middle of the next.
    float absLeftRel = 0, absRightRel = 0;
    Rgba hoverBg = {};
    bool hasHoverBg = false;
    // hover(|style| style.text_color(..)): what the subtree under a hovered
    // element paints with, for the descendants that set no color of their own.
    Rgba hoverFg = {};
    bool hasHoverFg = false;
    int focusId = 0;
    // FocusHandle::tab_index / tab_stop. The index groups the traversal: Tab
    // visits every element of the lowest index in the order they were painted,
    // then the next index, and so on, which is how a control can be reached
    // before one that is laid out above it. A handle that is not a tab stop
    // keeps its focus and its ring and is simply skipped by the traversal —
    // an input's clear button, or a dock tab bar's tools, which the keyboard
    // reaches through the thing they belong to rather than one at a time.
    int tabIndex = 0;
    bool tabStop = true;
    // div().key_context(".."): the name a keystroke is resolved against while
    // focus is anywhere in this subtree. Hashed, since that is all an id is.
    uint32_t keyContext = 0;
    // FocusableExt::focus_ring: whether a focused element shows the focus
    // appearance at all. Rust gates the whole `focus_ring_style` call on it,
    // so turning it off drops the tinted border along with the ring — a
    // control that draws its own focus some other way wants neither.
    bool focusRing = true;
    int trapId = 0;
    Str tooltip;
};

// One `on_action` handler. The tree is frame-arena, so a handful of these
// chained off an element costs a pointer each and dies with the frame.
struct ActionSlot {
    uint32_t action = 0;
    Listener fn = {};
    ActionSlot* next = nullptr;
};

// What an action handler is called with. Rust hands over the action itself,
// which carries whatever fields it was declared with; an action here is a
// name, so this is the name and the one thing a handler answers back.
struct ActionEvent {
    uint32_t action = 0;
    // cx.propagate(): the handler looked and did not want it, so the search
    // carries on outwards. Not setting it is Rust's default, which stops.
    bool propagate = false;
};

struct El {
    ElKind kind = ElKind::Div;
    // The frame arena this was built on, so a builder that has to allocate —
    // an action handler's slot — has one without being handed it again.
    Arena* arena = nullptr;
    Style style;
    Str id;
    Str text;
    IconName icon = IconName::None;
    Str iconPath;
    // ElKind::Image: what the document called the image. gpui/image.h says
    // what that may name.
    Str imgSrc;
    ChartSeries chart = {};
    float progress = 0; // 0..100
    int clickId = 0;
    Func0 onClick;
    Listener listener;
    // `div().on_hover(..)`. Fires with a HoverEvent when the pointer enters
    // the element and again when it leaves, never in between.
    Listener onHover;
    Listener onScroll;
    ActionSlot* actions = nullptr;
    // window.on_mouse_event::<MouseDownEvent> bound to one element, which is
    // what `div().on_mouse_down(..)` is. A press runs this before the click
    // listener above; unlike the click, it carries the full MouseDownEvent.
    Listener onMouseDown;
    Listener onMouseUp;
    // on_drag_move. GPUI carries a drag entity so the move can name what is
    // being dragged; here the element that took the press keeps the moves
    // until the button comes back up, which is the same thing without the
    // entity. Needs a Click(id): the tree is rebuilt every frame, so the id
    // is what finds the element again.
    Listener onDragMove;
    // on_drag: what a press on this element picks up. The payload rides along
    // on every DragMoveEvent the drag produces.
    DragPayload drag = {};
    // div().cursor_col_resize() and friends: the shape the pointer takes over
    // this element. Rust hangs it off the style; it needs a Click(id) here for
    // the same reason a hover does, since the hit rect is what the move
    // consults.
    CursorKind cursor = CursorKind::Arrow;
    // on_mouse_up_out: a release that landed anywhere but on this element.
    // Rust hears it wherever the pointer is, whether or not the press started
    // here, and so does this.
    Listener onMouseUpOut;
    // on_drop::<T>(..) and drag_over::<T>(..): the kind of drag this element
    // takes, and what to do when one lets go over it. `WindowDragOverId` says
    // which element the drag is over right now, which is what a caller styles
    // on — GPUI applies `drag_over` itself because the style is part of the
    // element; an element here is rebuilt every frame and reads the answer
    // back instead.
    Str dropKind = {};
    Listener onDrop;
    // Where this element ended up, written at paint. GPUI's DockArea keeps
    // its own `bounds` the same way, through an element whose only job is to
    // report the box layout gave it; a caller that has to answer "what is
    // under the pointer" needs last frame's boxes to do it.
    gpui::Bounds* boundsOut = nullptr;
    // BindSlider: this element is a slider's track, and a press or a drag on
    // it moves that state. GPUI's slider elements capture the state entity in
    // their own closures; there are no closures on an element here, so the
    // element names the state and the window does what those closures do.
    SliderState* slider = nullptr;
    Axis sliderAxis = Axis::Horizontal;
    // BindInput: this element is a text field's editor box, so a press in it
    // places the caret and a drag extends the selection. The same trick as
    // BindSlider — Rust's InputElement captures the state entity in the
    // closures it installs, and an element here names the state instead.
    InputState* input = nullptr;
    // BindSliderBounds: this element is the rail a value maps against, and
    // hands its box to the state once it has one — SliderIndicator's
    // `on_prepaint(|bounds| state.set_bounds(bounds))`. The box that catches
    // the press is the taller track, so the two are not the same element.
    SliderState* sliderBounds = nullptr;
    void (*customPaint)(PaintCtx* ctx, El* e, void* user) = nullptr;
    void* customUser = nullptr;
    El* first = nullptr;
    El* last = nullptr;
    El* next = nullptr;
    float x = 0, y = 0, w = 0, h = 0;
    // The laid-out box as one value. The fields stay flat because the layout
    // pass writes them a component at a time. The return type is qualified
    // because this member hides `Bounds` inside El.
    gpui::Bounds Bounds() const { return {x, y, w, h}; }
    float scrollY = 0;
    // overflow_x_scroll: how far the content is slid to the left. Positive
    // means the view has moved right over it, as scrollY is positive-down.
    float scrollX = 0;
    // The bar this box shows, and whether the caller named it. Unnamed, it
    // is the theme's — Rust's Scrollbar reads `cx.theme().scrollbar_mode`
    // unless the caller passed one.
    ScrollbarMode scrollMode = ScrollbarMode::Always;
    bool scrollModeSet = false;
    // A box that scrolls without showing a bar. In Rust the scrolling
    // container and the Scrollbar are two elements, so a container with no
    // Scrollbar beside it simply has none; the box paints its own bar here,
    // and this is how it says not to — a tab bar scrolls, and shows nothing.
    bool noScrollbar = false;
    int scrollId = 0;
    float contentW = 0;
    float contentH = 0;
    int selLo = -1; // UTF-8 offsets into text, -1 = none
    int selHi = -1;
    // The highlighted runs inside this text, in order. The array is the
    // caller's — the frame arena, in practice — and outlives the frame the
    // element was built in.
    const TextSpan* spans = nullptr;
    int nSpans = 0;
    Rgba selColor = Rgba8(0x6b, 0xb3, 0xf0, 90);
    // The input method's provisional run, underlined the way Rust gives the
    // marked range its own UnderlineStyle. Same offsets, same -1 for none.
    int markLo = -1;
    int markHi = -1;
    bool selectable = false;
    // The caret this run draws, as a UTF-8 offset into it; -1 for none. Rust's
    // InputElement measures cursor_bounds in prepaint and paints a quad there,
    // which is what this is — putting the bar between two text runs instead
    // would shift the glyphs by its own width every time it appeared.
    int caretOff = -1;
    Rgba caretColor = {};
    float caretW = 2;
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
    // percentage(delta) turns clockwise, which is what a spinner is made of.
    El* Rotate(float turns);
    // Scroll without a bar: the box still takes the wheel and still clips.
    El* HideScrollbar();
    // opacity(f): this element and everything under it, faded together.
    // Nested opacities multiply, as GPUI's do.
    El* Opacity(float f);
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
    El* ScrollX(float off);
    El* ClipX();
    El* ScrollMode(ScrollbarMode m);
    El* ScrollId(int v);
    El* Click(int v);
    El* OnClick(Func0 fn);
    El* OnClick(Listener l);
    // The scrollbar's own handler. Rust's scrollbar writes straight into the
    // shared ScrollHandle; here the view owns the offset, so dragging the
    // thumb or pressing the track reports one for it to store.
    El* OnScroll(Listener l);
    El* OnHover(Listener l);
    El* OnMouseDown(Listener l);
    El* OnMouseUp(Listener l);
    El* OnDragMove(Listener l);
    El* OnDrag(Str dragKind, int ix = 0, void* data = nullptr);
    El* OnMouseUpOut(Listener l);
    El* OnDrop(Str acceptKind, Listener l);
    El* BoundsOut(gpui::Bounds* out);
    El* Cursor(CursorKind c);
    El* BindSlider(SliderState* s, Axis axis = Axis::Horizontal);
    El* BindSliderBounds(SliderState* s);
    El* BindInput(InputState* s);
    // The selection quad and the caret an input's text run paints over itself.
    El* SelRange(int lo, int hi, Rgba color);
    El* Spans(const TextSpan* runs, int n);
    // The marked range, which is drawn underlined in the text's own colour.
    El* MarkRange(int lo, int hi);
    El* Caret(int off, Rgba color, float width = 2);
    El* Child(El* c);
    El* Bold();
    El* Semibold();
    El* Medium();
    El* Mono();
    El* Underline();
    El* Strikethrough();
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
    El* LeftRel(float frac);
    El* RightRel(float frac);
    El* HoverBg(Rgba c);
    El* HoverFg(Rgba c);
    El* FocusId(int v);
    El* KeyContext(Str name);
    // on_action::<A>(..). The listener is called with an ActionEvent; setting
    // its `propagate` passes the action on outwards, which is cx.propagate().
    El* OnAction(uint32_t action, Listener fn);
    El* TabIndex(int v);
    El* TabStop(bool v);
    El* FocusRing(bool v);
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
// gpui's img(src): `alt` is what paints when the source will not decode.
// The size is the image's own unless W() / H() overrides it, and it is
// scaled down to fit the width it is given — object_fit(Contain) with
// max_w(relative(1.)), which is how node.rs renders a markdown image.
El* ImageEl(Arena* a, Str src, Str alt = {});
El* ProgressEl(Arena* a, float value01to100, float barW, float barH);
El* ChartEl(Arena* a, const float* ys, int n, Rgba stroke, Rgba fillTop,
            Rgba fillBot, int tickMargin);

// ─── paint / window ───────────────────────────────────────────────────────

struct HitRect {
    int id = 0;
    Bounds bounds = {};
    Func0 onClick;
    Listener listener;
    Listener onHover;
    Listener onMouseDown;
    Listener onMouseUp;
    Listener onDragMove;
    DragPayload drag = {};
    Listener onMouseUpOut;
    Str dropKind = {};
    Listener onDrop;
    CursorKind cursor = CursorKind::Arrow;
    // El::Tip. The overlay reads it when the pointer arrives, so it has to
    // survive the hit test rather than only the paint that drew it.
    Str tooltip = {};
    SliderState* slider = nullptr;
    Axis sliderAxis = Axis::Horizontal;
    InputState* input = nullptr;
};

// A scrolled box the frame painted, and the scrollbar drawn down its right
// edge. Rust's scrollbar reaches its ScrollHandle directly; the offset here
// belongs to whichever view passed it in through El::ScrollY, so a press on
// the bar reports the offset it computed and the view stores it.
struct ScrollRect {
    int id = 0;
    Bounds bounds = {};
    float contentH = 0;
    float scrollY = 0;
    float contentW = 0;
    float scrollX = 0;
    ScrollbarMode mode = ScrollbarMode::Always;
    Listener onScroll;
};

// The scrollbar as it is drawn: a 6-DIP thumb inset from the right edge. The
// band a press counts in is the pair together, so the target is wide enough to
// aim at without reaching into the content.
const float kScrollbarThumbW = 6.f;
const float kScrollbarThumbMargin = 4.f;
const float kScrollbarBandW = kScrollbarThumbW + kScrollbarThumbMargin * 2.f;

// What El::OnScroll hands its handler: the box that was scrolled and where it
// should now be. Positive-down, as El::ScrollY takes it.
struct ScrollEvent {
    int id = 0;
    float offsetY = 0;
    // The horizontal offset, for a box that scrolls both ways. A handler that
    // only scrolls down can ignore it; it is whatever it already was.
    float offsetX = 0;
};

struct TextHit {
    Bounds bounds = {};
    Str text;
    float font = 14;
    float maxW = 0;
    bool wrap = false;
    int docOff = 0;
    // TextSelectionScopeId: the focus trap this run sits inside, 0 for the
    // page itself. A selection belongs to one scope, so a drag that started
    // in a dialog does not run on into the page behind it.
    int scope = 0;
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

// What the inspector is looking at. GPUI's Inspector picks an element out of
// the window and shows where it came from; an element here has no source
// location — nothing takes `#[track_caller]` — so what it can say for itself
// is its id, the box layout gave it, and the style it was built with.
struct InspectorPick {
    int id = 0;
    Str elId = {};
    Bounds bounds = {};
    // The kind of element, as El::kind reads it.
    int kind = 0;
    int depth = 0;
    bool hasBg = false;
    Rgba bg = {};
    float pad = 0;
    float gap = 0;
    float radius = 0;
    float border = 0;
    bool row = true;
    float font = 0;
    // The text a Text element holds, so a picked label says which one it is.
    Str text = {};
};

// window.toggle_inspector / Inspector::is_picking. The panel is the caller's
// to render — `component::Inspector` is the one this tree ships — and this is
// the state it reads.
struct InspectorState {
    bool on = false;
    bool picking = false;
    bool hasPick = false;
    InspectorPick pick = {};
    // A press while picking names the element it landed on, but the frame
    // that painted last was aimed at wherever the pointer was then. The pick
    // is settled on the next frame instead, against the press itself.
    bool pending = false;
    float pendingX = 0;
    float pendingY = 0;
};

struct PaintCtx {
    PaintApp* pa = nullptr;
    PaintTarget* rt = nullptr;
    // Window::element_opacity: the Style::opacity of everything this element
    // is inside, multiplied together. Every colour painted is faded by it, so
    // a subtree fades as one thing rather than each of its boxes separately.
    float opacity = 1;
    float dpi = 96;
    float viewW = 0;
    float viewH = 0;
    int hoverId = 0;
    int focusId = 0;
    // window.focus_generation: bumped every time the focus moves, so a
    // keystroke can tell that it stayed put without holding onto the element.
    int focusGen = 0;
    // Where the pointer is, which is what a Hover-mode scrollbar consults.
    float mouseX = -1;
    float mouseY = -1;
    // The inspector picking an element: every box under the pointer overwrites
    // this as it paints, so the deepest one wins — which is the one a click
    // would land on.
    bool picking = false;
    bool pickHit = false;
    InspectorPick pick = {};
    Vec<HitRect> hits;
    Vec<ScrollRect> scrolls;
    Vec<TextHit> texts;
    // The fields this frame painted, outermost box first, so a press can find
    // the one it landed in. A hit rect would shadow the click on the box
    // around it; these are a list of their own for the same reason GPUI
    // installs the editor's mouse handlers beside the container's, not
    // instead of them.
    Vec<InputState*> inputs;
    int textDocLen = 0;
    int selA = -1;
    int selB = -1;
    // Which scope the range above belongs to; -1 paints it wherever it
    // falls, which is what a caller that knows of no scopes wants.
    int selScope = -1;
    TextMeasCache textCache;

    PaintCtx() = default;
};

struct FocusRect {
    int id = 0;
    int trapId = 0;
    int tabIndex = 0;
    bool tabStop = true;
    // Where this element sits in the frame's dispatch list. Rust walks the
    // real tree to find what is above a focused handle; the tree here is gone
    // by the time a key arrives, so the walk is recorded while it is still
    // there — see DispatchNode.
    int dispatchIx = 0;
    Bounds bounds = {};
};

// One key context or one action handler, recorded in tree order. `subtreeEnd`
// is one past the last node of the element's whole subtree, so a node is
// above position `p` exactly when it was written before `p` and its subtree
// still has not closed: that is an ancestor test that does not care whether
// the elements in between contributed nodes of their own. Walking backwards
// from `p` visits them innermost first, which is the order Rust reads a
// dispatch path in.
struct DispatchNode {
    int subtreeEnd = 0;
    uint32_t context = 0;
    uint32_t action = 0;
    Listener fn = {};
};

// ─── UTF-8 scanning ───────────────────────────────────────────────────────
//
// What text_boundary.rs and the input engine both walk the text with. A byte
// that is not valid UTF-8 counts as one character of its own value: every
// caller only asks which class it lands in, and every stray byte lands in the
// same one.

// crates/base/src/text_boundary.rs CharacterKind.
enum class CharKind : uint8_t {
    Word,
    Whitespace,
    Newline,
    Other
};

CharKind CharKindOf(uint32_t c);
// The codepoint at byte `i` and how many bytes it took.
int Utf8At(Str s, int i, uint32_t* out);
// Where the character before `i` starts.
int Utf8Prev(Str s, int i);
// clip_offset_left: into the string, then back to a character boundary.
int Utf8ClipLeft(Str s, int off);

// ─── rope ─────────────────────────────────────────────────────────────────
//
// crates/base/src/input/base/rope_ext.rs. Rust's input holds its document in a
// `ropey::Rope` and reaches it through the `RopeExt` trait; here the document
// is a flat UTF-8 buffer and the trait's methods are these functions over a
// `Str`. An input holds a form field or a page of code, not a file, so the
// piece table a rope buys is machinery with nothing to do.

// sum_tree::Bias: which side of a character an offset that lands inside one
// is pulled to.
enum class Bias : uint8_t {
    Left,
    Right
};

// rope_ext.rs Point — a row and a byte column inside it, not a position on
// screen. Named the way Rust names it; gpui::Point is the geometry one.
struct RopePoint {
    int row = 0;
    int column = 0;
};

// Into the text, then to a character boundary on the named side.
int RopeClipOffset(Str text, int offset, Bias bias);
// char_at: the codepoint at `offset`, and how many bytes it took. 0 when the
// offset is past the end — Rust's `None`.
int RopeCharAt(Str text, int offset, uint32_t* out);
int RopeLinesLen(Str text);
int RopeLineStartOffset(Str text, int row);
int RopeLineEndOffset(Str text, int row);
// slice_line: the row without its newline.
Str RopeSliceLine(Str text, int row);
int RopeLineLen(Str text, int row);
RopePoint RopeOffsetToPoint(Str text, int offset);
int RopePointToOffset(Str text, RopePoint point);
int RopeOffsetUtf16ToOffset(Str text, int offsetUtf16);
int RopeOffsetToOffsetUtf16(Str text, int offset);
int RopeCharIndexToOffset(Str text, int charIndex);
int RopeOffsetToCharIndex(Str text, int offset);

// ─── input ────────────────────────────────────────────────────────────────
//
// crates/base/src/input/base. Rust's engine is one `InputBaseState<M>`
// parameterized by a compile-time mode marker (`InputMode`, `TextareaMode`,
// `EditorMode`) so a method that only makes sense for one of them does not
// exist on the others. There are no traits to bound here, so the marker is a
// runtime `InputKind` and the methods that would not compile in Rust return
// early instead — `InputMoveVertical` on a single-line field, say.

// gpui_component::input::InputEvent.
enum class InputEventKind : uint8_t {
    Change,
    PressEnter,
    Focus,
    Blur
};

struct InputEvent {
    InputEventKind kind = InputEventKind::Change;
    // PressEnter { secondary, shift }.
    bool secondary = false;
    bool shift = false;
};

// cursor.rs Selection: a byte range into the text. Empty means the caret sits
// at `start`, with nothing selected.
struct Selection {
    int start = 0;
    int end = 0;

    int Len() const { return end > start ? end - start : 0; }
    bool IsEmpty() const { return start == end; }
    bool Contains(int offset) const { return offset >= start && offset < end; }
};

inline Selection SelectionAt(int offset) {
    return Selection{offset, offset};
}

// undo_manager.rs EditIntent. What kind of edit produced a change, which is
// what decides whether two of them coalesce into one undo step.
enum class EditIntent : uint8_t {
    Typing,
    Backspace,
    DeleteForward,
    Atomic
};

// change.rs Change. Rust's owns two `String`s; these are heap `Str`s the
// transaction that holds them frees.
struct Change {
    Selection oldRange = {};
    Str oldText = {};
    Selection newRange = {};
    Str newText = {};
    Selection selBefore = {};
    Selection selAfter = {};
};

// One undo step. Rust's holds a `Vec<Change>`; a `Vec<T>` here is memcpy-only
// and this lives inside another `Vec`, so the change list is a plain owned
// array the manager grows and frees by hand.
struct UndoTransaction {
    EditIntent intent = EditIntent::Atomic;
    Change* changes = nullptr;
    int len = 0;
    int cap = 0;
};

// undo_manager.rs UndoManager. Every edit makes a transaction; adjacent
// compatible ones coalesce until something breaks the run — a cursor move, a
// paste, a blur. IME composition brackets its callbacks with Begin/Commit.
struct UndoManager {
    Vec<UndoTransaction> undos;
    Vec<UndoTransaction> redos;
    bool ignoring = false;
    bool transactionOpen = false;
    bool hasPending = false;
    Change pending = {};
    // pending_intent: what the next replace should record itself as. Taken by
    // the edit that follows.
    bool hasPendingIntent = false;
    EditIntent pendingIntent = EditIntent::Atomic;
    bool coalescingBoundary = false;

    ~UndoManager();
};

void UndoRecordTransaction(UndoManager* m, Change change, EditIntent intent);
void UndoBeginTransaction(UndoManager* m);
void UndoCommitTransaction(UndoManager* m);
void UndoBreakCoalescing(UndoManager* m);
void UndoSetIgnoring(UndoManager* m, bool ignoring);
bool UndoIsIgnoring(const UndoManager* m);
void UndoClear(UndoManager* m);
// undo() / redo(). Rust clones the change list out; the transaction lives on
// the other stack either way, so these hand back the one that moved and the
// caller walks it — backwards for an undo, forwards for a redo. Null when the
// stack is empty.
const UndoTransaction* UndoPopUndo(UndoManager* m);
const UndoTransaction* UndoPopRedo(UndoManager* m);

// mask_pattern.rs MaskToken. `Sep` carries its character, which the pattern
// string holds, so this is the tag alone.
enum class MaskToken : uint8_t {
    Digit,         // 9  — [0-9]
    Letter,        // A  — [a-zA-Z]
    LetterOrDigit, // # — [a-zA-Z0-9]
    Any,           // *  — any character
    Sep            // anything else, matching only itself
};

enum class MaskKind : uint8_t {
    None,
    Pattern,
    Number
};

// mask_pattern.rs MaskPattern. Rust keeps the parsed `Vec<MaskToken>` beside
// the pattern; a token is a pure function of its character, so the pattern
// string is the whole state and `MaskTokenAt` reads it.
struct MaskPattern {
    MaskKind kind = MaskKind::None;
    // Pattern: owned, freed by MaskPatternFree.
    Str pattern = {};
    // Number: the group separator, 0 for None.
    uint32_t separator = 0;
    // Number: how many fraction digits to keep, -1 for None.
    int fraction = -1;
};

// `(999)999-9999`, `AAAA-99-####`, `*999*`.
MaskPattern MaskPatternNew(Str pattern);
MaskPattern MaskPatternNumber(uint32_t separator);
void MaskPatternFree(MaskPattern* p);
// The token at character index `pos`, and its separator character. False when
// the pattern has no token there.
bool MaskTokenAt(const MaskPattern& p, int pos, MaskToken* out, uint32_t* sep);
bool MaskIsNone(const MaskPattern& p);
bool MaskIsValid(const MaskPattern& p, Str maskText);
bool MaskIsValidAt(const MaskPattern& p, uint32_t ch, int pos);
// mask(): 123456789 through `(999)999-999` is `(123)456-789`.
Str MaskApply(Arena* a, const MaskPattern& p, Str text);
// unmask(): the original text back out of a masked one.
Str MaskUnapply(Arena* a, const MaskPattern& p, Str maskText);
// The cue a pattern shows when empty: `(___) ___-____`. Empty for the other
// two kinds, which is Rust's `None`.
Str MaskPlaceholder(Arena* a, const MaskPattern& p);
// normalize_number_input: full-width and CJK number characters folded to
// their ASCII equivalents, so `123。5` reaches the parser as `123.5`.
Str NormalizeNumberInput(Arena* a, Str text);

// kind.rs. Which of the three states this is. Rust fixes it at the type level;
// `InputIsMultiLine` is its `MULTI_LINE` associated constant.
enum class InputKind : uint8_t {
    Input,
    Textarea,
    Editor
};

// mode.rs LayoutMode. How the input lays its text out — rows and growth. Not
// the same question as `InputKind`: an auto-growing textarea capped at one row
// is still multi-line, which is the bug Rust split these two apart to fix.
enum class LayoutModeKind : uint8_t {
    PlainText,
    AutoGrow,
    CodeEditor
};

struct LayoutMode {
    LayoutModeKind kind = LayoutModeKind::PlainText;
    int rows = 1;
    int minRows = 1;
    int maxRows = 0; // 0 = usize::MAX
    int tabSize = 4;
    bool lineNumber = false;
};

void LayoutModeSetRows(LayoutMode* m, int rows);
int LayoutModeRows(const LayoutMode& m);
int LayoutModeMinRows(const LayoutMode& m);

// state.rs InputBaseState. The text a themed Input is bound to, everything
// that edits it, and the undo history behind it. `onChange` is what Rust
// spells cx.subscribe(&input_state, |ev: &InputEvent| ...).
struct InputState {
    InputKind kind = InputKind::Input;
    LayoutMode mode = {};
    // Rust's `Rope`. NUL-terminated past `len` so a `const char*` reader still
    // works; the terminator is not counted.
    Vec<char> text;
    Selection selectedRange = {};
    bool selectionReversed = false;
    // selected_word_range: what a double click took, kept so dragging out of
    // it cannot shrink back inside the word.
    bool hasSelectedWordRange = false;
    Selection selectedWordRange = {};
    UndoManager undo;
    MaskPattern maskPattern = {};
    bool maskPatternSet = false;
    Str placeholder = {}; // owned
    bool focused = false;
    bool disabled = false;
    bool readonly = false;
    bool loading = false;
    // A masked field draws one bullet per character. InputMode only.
    bool masked = false;
    bool cleanOnEscape = false;
    bool submitOnEnter = false;
    bool softWrap = true;
    // text_align: 0 left, 1 center, 2 right.
    int align = 0;
    // A press is down and every move until the release extends the selection.
    bool selecting = false;
    // This field's caret clock, InputState::blink_cursor. Created on first
    // use, so an InputState stays a plain value.
    EntityId blink = {};
    Listener onChange = {};
    // validate: `Fn(&str, &mut App) -> bool`. A plain function pointer plus
    // its captured value, the way Listener carries one.
    bool (*validate)(Str text, intptr_t arg) = nullptr;
    intptr_t validateArg = 0;
    // The text run the element last painted, so a press can be turned into an
    // offset. Rust keeps `last_bounds` + `last_layout` for the same reason;
    // in a multi-line field this is the *first* row, and the ones under it are
    // found by stepping `lastLineH` down from it — unless soft wrap made them
    // different heights, which is what `rowBoxes` is for.
    Bounds lastBounds = {};
    float lastFont = 0;
    float lastLineH = 0;
    // Whether those rows were drawn in the monospace family, so a press is
    // measured against the same advances they were laid out with.
    bool lastMono = false;
    // display_map.rs: the box each logical line was last laid out in. Soft
    // wrap makes them uneven — a line that wrapped is two of those boxes tall
    // or more — so a press cannot be turned into a row by arithmetic, and
    // neither can the caret's y. Empty when nothing wrapped, where the
    // arithmetic is right and cheaper. Sized before the rows are built, so
    // the pointers the elements are handed stay put for the frame.
    Vec<Bounds> rowBoxes;
    // The box the rows were laid out in as a whole, which is the scrolled
    // height once soft wrap has had its say.
    Bounds contentBox = {};
    // input_bounds: the whole field, what a press outside the run maps against.
    Bounds inputBounds = {};
    // scroll_handle: how far the field has scrolled under its own box, and
    // the box it scrolls inside. Positive-down, as El::ScrollY takes it; Rust
    // keeps the same pair, negative, on a ScrollHandle.
    float scrollX = 0;
    float scrollY = 0;
    float viewW = 0;
    float viewH = 0;
    // The caret's x inside the run, measured when it was last painted, and
    // the whole scrolled height. `last_layout` is what Rust reads them off.
    float caretX = 0;
    float contentW = 0;
    float contentH = 0;
    // Suppressed while set_value writes the text, so a programmatic write is
    // not reported as the user having typed.
    bool emitEvents = true;
    // ime_marked_range: the text the input method has put in provisionally,
    // which is the document's until the composition commits or is abandoned.
    // Rust keeps an Option; `imeMarking` is the Some.
    Selection imeMarked = {};
    bool imeMarking = false;
    // preferred_column: the column a vertical move aims for, so walking down
    // past a short line and back up returns to where it started. -1 for none.
    int preferredColumn = -1;
    // The x Rust remembers beside it, which is what a walk over *display*
    // rows aims at — a column means nothing halfway through a wrapped line.
    // -1 for none; cleared by every move that is not part of the walk.
    float preferredX = -1;

    ~InputState();
};

// Which way a move went, which decides whether scroll_to may pull the view
// back the other way. Rust's MoveDirection.
enum class InputMoveDir : uint8_t {
    None,
    Up,
    Down
};

// scroll_to: the offset that brings the caret into view, from where the field
// is now. `caretY` is the top of the caret's line and `caretX` its position
// across the run; the answer keeps the caret a line's clearance from either
// edge and never scrolls past the content. A move that went up will not be
// answered with a downward scroll, and the other way about — Rust clamps the
// same way, so a vertical walk does not fight itself.
void InputScrollToCaret(InputState* s, float caretX, float caretY,
                        InputMoveDir dir);
// The same, for wherever the caret is now: the row it is on and the x the
// last paint measured.
void InputScrollToCursor(InputState* s, InputMoveDir dir);

// value() / the NUL-terminated view of it. Neither allocates.
Str InputValue(const InputState* s);
const char* InputCStr(const InputState* s);
// unmask_value(): the text with the mask's separators taken back out.
Str InputUnmaskValue(Arena* a, const InputState* s);
// selected_text().
Str InputSelectedValue(const InputState* s);
bool InputIsMultiLine(const InputState* s);
bool InputIsSingleLine(const InputState* s);
bool InputIsEditable(const InputState* s);
// cursor(): the caret offset, which end of the selection depends on which way
// it was dragged.
int InputCursor(const InputState* s);
// cursor_position(): the row and column the caret is on.
RopePoint InputCursorPosition(const InputState* s);

// set_value(): replaces the text, resets the selection to the end, and clears
// the undo history — the programmatic write, not an edit.
void InputSetValue(InputState* s, Str value);
// replace_all(): the same replacement, but recorded so it can be undone.
void InputReplaceAll(InputState* s, App* app, Window* win, Str value);
void InputSetPlaceholder(InputState* s, Str value);
void InputSetMaskPattern(InputState* s, MaskPattern pattern);
// clean(): empties the field.
void InputClean(InputState* s, App* app, Window* win);
// insert() / replace(): a programmatic edit, recorded as one atomic step.
void InputInsert(InputState* s, App* app, Window* win, Str value);

// previous_boundary / next_boundary: one character either way.
int InputPreviousBoundary(const InputState* s, int offset);
int InputNextBoundary(const InputState* s, int offset);
// start_of_line / end_of_line, and the two word boundaries movement.rs asks
// for. A single-line field answers 0 and len for the first pair, as Rust does.
int InputStartOfLine(const InputState* s);
int InputEndOfLine(const InputState* s);
int InputPreviousStartOfWord(const InputState* s);
int InputNextEndOfWord(const InputState* s);

// move_to(): drops the selection and puts the caret at `offset`.
void InputMoveTo(InputState* s, App* app, Window* win, int offset);
// select_to(): drags the live end of the selection to `offset`.
void InputSelectTo(InputState* s, App* app, Window* win, int offset);
void InputSelectAll(InputState* s, App* app, Window* win);
void InputUnselect(InputState* s, App* app, Window* win);
void InputSetSelectedRange(InputState* s, App* app, Window* win, int a, int b);
// selection.rs: what a double and a triple click take.
void InputSelectWord(InputState* s, App* app, Window* win, int offset);
void InputSelectLine(InputState* s, App* app, Window* win, int offset);

// The actions state.rs binds, one per `impl` method there. The window turns a
// key chord into one of these with InputActionForKey and hands it over — which
// is what GPUI's action dispatch does for the focused element.
enum class InputAction : uint8_t {
    None,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    MoveHome,
    MoveEnd,
    MoveToStart,
    MoveToEnd,
    MoveToPreviousWord,
    MoveToNextWord,
    MovePageUp,
    MovePageDown,
    SelectLeft,
    SelectRight,
    SelectUp,
    SelectDown,
    SelectAll,
    SelectToStart,
    SelectToEnd,
    SelectToStartOfLine,
    SelectToEndOfLine,
    SelectToPreviousWordStart,
    SelectToNextWordEnd,
    Backspace,
    Delete,
    DeleteToBeginningOfLine,
    DeleteToEndOfLine,
    DeleteToPreviousWordStart,
    DeleteToNextWordEnd,
    Enter,
    Escape,
    Copy,
    Cut,
    Paste,
    Undo,
    Redo
};

InputAction InputActionForKey(const InputState* s, int vk, bool shift,
                              bool ctrl, bool alt);
// True when the input consumed it, so the window does not also treat Enter as
// a click on the focused element. `shift` is Enter's modifier, which decides
// whether a submit-on-enter textarea inserts a newline.
bool InputPerform(InputState* s, App* app, Window* win, InputAction action,
                  bool shift);
// replace_text_in_range: the one path every edit goes through. A null range is
// the current selection. Returns false when the edit was rejected — readonly,
// or a mask or validator that said no.
bool InputReplaceTextInRange(InputState* s, App* app, Window* win,
                             const Selection* range, Str newText);
// Input methods count in UTF-16 on both platforms that have one to talk to;
// a field counts in bytes. These are the two directions across. An offset
// past the end clamps to it, which is what a platform handing over a stale
// range needs.
int Utf8OffsetToUtf16(Str s, int u8);
int Utf16OffsetToUtf8(Str s, int u16);
// marked_text_range(): what the input method is still deciding, if anything.
bool InputMarkedRange(const InputState* s, Selection* out);
// replace_and_mark_text_in_range(): the input method's provisional insert.
// A null `range` means "over the mark, or the selection if there is none",
// which is what makes each keystroke of a composition replace the last one.
// `sel` is where the caret should sit inside the new text, in bytes from its
// start; null puts it at the end. Empty text ends the composition.
void InputReplaceAndMarkText(InputState* s, App* app, Window* win,
                             const Selection* range, Str newText,
                             const Selection* sel);
// unmark_text(): the composition is over and what it left stands. Commits the
// undo transaction the composition opened, so the whole of it undoes at once.
void InputUnmarkText(InputState* s, App* app, Window* win);
// The typed character, once the platform has decoded it.
void InputTypeChar(InputState* s, App* app, Window* win, uint32_t ch);

// on_focus / on_blur. Points win->input at this field, starts its caret, and
// emits the event; blurring commits the typing session, which is what makes a
// later undo stop at the right place.
void InputFocus(InputState* s, App* app, Window* win);
void InputBlur(InputState* s, App* app, Window* win);

// index_for_mouse_position: the offset a press at (x, y) lands on, against the
// run the element last painted.
int InputIndexForPosition(const InputState* s, PaintCtx* ctx, float x, float y);
// The field a press at (x, y) landed in, or null.
InputState* InputAtPosition(PaintCtx* ctx, float x, float y);

// crates/base/src/slider.rs. SliderValue is Rust's enum — `Single(f32)` or
// `Range(f32, f32)` — flattened into the pair plus the flag that says which
// variant this is. `hi` is `end()`, the one a single-value slider uses.
struct SliderValue {
    float lo = 0;
    float hi = 0;
    bool range = false;

    float Start() const { return range ? lo : hi; }
    float End() const { return hi; }
};

inline SliderValue SliderSingle(float v) {
    return {0, v, false};
}
inline SliderValue SliderRange(float lo, float hi) {
    return {lo, hi, true};
}
// SliderValue::clamp.
SliderValue SliderValueClamp(SliderValue v, float min, float max);
// SliderValue::set_start / set_end: a range keeps its ends in order.
void SliderValueSetStart(SliderValue* v, float value);
void SliderValueSetEnd(SliderValue* v, float value);

// SliderScale. Logarithmic gives finer control near the low end, which is what
// a volume or a playback speed wants.
enum class SliderScale : uint8_t {
    Linear,
    Logarithmic
};

// SliderEvent. Change comes on every press and every move that shifts the
// value; Release comes once, when the button goes up after one of those.
enum class SliderEventKind : uint8_t {
    Change,
    Release
};

struct SliderEvent {
    SliderEventKind kind = SliderEventKind::Change;
    SliderValue value = {};
};

// SliderState: what a slider is between frames, the way InputState is what an
// input is. Rust keeps it in an `Entity<SliderState>` and the element closures
// capture that handle; an element here names its state with `El::BindSlider`
// and the window applies the same behavior, which is how InputState works.
// `onChange` is `cx.subscribe(&state, |ev: &SliderEvent| ...)`.
struct SliderState {
    float min = 0;
    float max = 100;
    float step = 1;
    SliderValue value = {};
    // percentage: Range<f32>. A single-value slider only uses `hi`, with `lo`
    // pinned at 0, which is what Rust's `0.0..percentage` says.
    float pctLo = 0;
    float pctHi = 0;
    // The box the value maps against, recorded when the slider is pressed.
    Bounds bounds = {};
    SliderScale scale = SliderScale::Linear;
    // Set by a press, cleared by the release, so a release with no press
    // behind it emits nothing.
    bool dragging = false;
    // Which end of a range the press took, for the moves that follow. Rust
    // carries it in the DragThumb payload, which a drag here has no room for.
    bool dragStart = false;
    Listener onChange = {};
};

// SliderState::new().min(..).max(..).step(..).scale(..).default_value(..),
// which is a builder chain in Rust and one call here, so a view can write its
// slider as a field initializer.
SliderState SliderStateNew(float min, float max, SliderValue value,
                           float step = 1,
                           SliderScale scale = SliderScale::Linear);

// `.min()` / `.max()`, which re-derive the thumb position. Rust panics when a
// logarithmic slider is given a min <= 0 or a max <= min; there are no
// exceptions here, so the limits are pushed to the nearest usable pair
// instead — a widget that draws itself wrong is better than one that exits.
void SliderSetLimits(SliderState* s, float min, float max);
// `.step()`, the quantum a value snaps to.
void SliderSetStep(SliderState* s, float step);
// `.scale()`.
void SliderSetScale(SliderState* s, SliderScale scale);
// `.default_value()` / `set_value()`.
void SliderSetValue(SliderState* s, SliderValue v);
// set_bounds, the box a position maps against.
inline void SliderSetBounds(SliderState* s, Bounds b) {
    s->bounds = b;
}

// percentage_to_value / value_to_percentage.
float SliderPctToValue(const SliderState* s, float pct);
float SliderValueToPct(const SliderState* s, float value);
// update_thumb_pos: the percentages that follow from the value.
void SliderUpdateThumbPos(SliderState* s);

// update_value_by_position. `isStart` moves the low end of a range. Rust ends
// this with `cx.emit(SliderEvent::Change)`; the window here raises the event
// instead, so this returns whether the value actually moved.
bool SliderUpdateByPosition(SliderState* s, Axis axis, Point pos, bool isStart);
// Which end of a range a press at `pos` takes, by the midpoint between the two
// thumbs — the `is_start` arm of SliderTrack's mouse-down handler.
bool SliderIsStartAt(const SliderState* s, Axis axis, Point pos);
// handle_release: true when a Release event is due.
bool SliderHandleRelease(SliderState* s);

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

Size MeasureText(PaintCtx* ctx, Str s, float fontSize, float maxW,
                 bool wrap = false, int weight = 0, float lineH = 0);
void TextMeasBeginFrame(PaintCtx* ctx);
void TextMeasEndFrame(PaintCtx* ctx);
void TextMeasClear(PaintCtx* ctx);
// The inverse of TextIndexAt: where a UTF-8 offset sits in a run once it has
// been laid out and wrapped. `outY` is the top of the visual row the offset
// is on and `outH` that row's height. False when there was nothing to
// measure against.
//
// `mono` and `lineHeight` have to be the ones the run was painted with:
// Consolas advances differently from the proportional face, so measuring a
// code editor's row without them answers for a line of text that was never
// drawn.
bool TextPointAt(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                 int off, float* outX, float* outY, float* outH,
                 bool mono = false, float lineHeight = 0);
int TextIndexAt(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                float relX, float relY, bool mono = false,
                float lineHeight = 0);
void PaintTextRange(PaintCtx* ctx, Str s, float fontSize, float maxW, bool wrap,
                    float x, float y, int u8a, int u8b, Rgba color);
void PaintTextUnderline(PaintCtx* ctx, Str s, float fontSize, float maxW,
                        bool wrap, float x, float y, int u8a, int u8b,
                        Rgba color);
void LayoutEl(PaintCtx* ctx, El* e, float x, float y, float availW,
              float availH, float inheritFont, Rgba inheritFg);
void PaintEl(PaintCtx* ctx, El* e);
int HitTest(PaintCtx* ctx, float x, float y);
const HitRect* HitTestRect(PaintCtx* ctx, float x, float y);
// The topmost element under the pointer that takes a drag of this kind, of
// those that asked for one at all. A drop target that does not want what is
// being dragged is not in the way of one that does.
const HitRect* HitTestDrop(PaintCtx* ctx, float x, float y, Str kind);
const ScrollRect* HitScrollRect(PaintCtx* ctx, float x, float y);
int TextHitOffsetAt(PaintCtx* ctx, float x, float y, bool nearest);
// The same, confined to one selection scope (-1 for any), and reporting the
// scope the point landed in.
int TextHitOffsetIn(PaintCtx* ctx, float x, float y, bool nearest, int scope,
                    int* outScope);
int CopyTextHits(PaintCtx* ctx, int selA, int selB, char* out, int cap);
int CopyTextHitsIn(PaintCtx* ctx, int selA, int selB, int scope, char* out,
                   int cap);
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
bool TextMultiClickRangeIn(PaintCtx* ctx, float x, float y, int clickCount,
                           int scope, int* outA, int* outB, int* outScope);
int HashClickId(Str s);

// Whether a release makes a click. GPUI holds the press as
// `pending_mouse_down` and fires on_click from the mouse-up handler, where it
// asks three things: that a press is waiting at all, that the button coming
// up is the one that went down, and that the element under the pointer is the
// one the press landed on. A press that slid off somewhere else is no click,
// and a drag takes the release the click would have had.
//
// `pending` is Rust's Option being Some: a press the scrollbar, the inspector
// or a non-focusing button took is nobody's pending click. `upId` and
// `pressedId` are 0 for the page itself, which is a click too — that is the
// outside press an overlay dismisses on.
bool ClickFromRelease(bool pending, int pressedId, MouseButton pressedButton,
                      bool dragged, int upId, MouseButton upButton);

// The same for the keyboard: whether the release of Enter or Space makes a
// click. GPUI arms on the key down — `pending_keyboard_down` is the focus
// generation the keystroke went down at — and makes the click from the key
// up, if that stamp still matches. A generation rather than the focused
// element itself, because focus that left and came back is not the same
// press; any other key in between clears the stamp, and a modifier held on
// either half means the keystroke was a shortcut, not an activation.
bool ClickFromKeyRelease(bool pending, int pendingGen, int focusGen, int key,
                         bool modified);

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

// crates/base/src/text_selection.rs WindowSelectionState, one per window.
struct WindowSelection;

struct Window {
    App* app = nullptr;
    PlatWindow* plat = nullptr;
    PaintCtx paint = {};
    Arena* frameArena = nullptr;
    // The selection over this window's selectable runs. Made on the first
    // press that lands on text and dropped with the window.
    WindowSelection* sel = nullptr;
    // The view this window renders. GPUI's Window holds a root view too.
    EntityId root = {};
    int hoverId = 0;
    int focusId = 0;
    // window.focus_generation: bumped every time the focus moves, so a
    // keystroke can tell that it stayed put without holding onto the element.
    int focusGen = 0;
    float mouseX = 0;
    float mouseY = 0;
    // What the pointer looks like right now; the OS is only told on a change.
    CursorKind cursor = CursorKind::Arrow;
    bool maximized = false;
    // is_window_active: whether this window has the focus. A client-decorated
    // frame dims its border when it does not.
    bool active = true;
    bool running = true;
    bool anim = false;
    // window.request_animation_frame(): one more frame after this one, asked
    // for while the frame is being built and cleared as the next one starts,
    // so something that has finished moving stops asking. `anim` is the other
    // thing — a window that draws back to back until it is turned off.
    bool animFrame = false;
    // The instant this frame started. Every transition in it samples the same
    // `now`, which is what Rust gets from reading the executor's clock.
    double frameNow = 0;
    bool mouseDown = false;
    // The multi-click run in progress: when the last press landed, where, and
    // with which button, so WindowClickCount can tell the next press apart
    // from a second click. GPUI keeps the same three in its platform layer.
    double lastDownAt = 0;
    float lastDownX = 0;
    float lastDownY = 0;
    MouseButton lastDownButton = MouseButton::Left;
    int clickRun = 0;
    // The element that took the press, until the button comes back up: what
    // GPUI's drag gives an element for free. 0 when nothing is held.
    int pressedId = 0;
    // What the press that is still down looked like: GPUI keeps the whole
    // MouseDownEvent as `pending_mouse_down` and hands it to the click it
    // makes on release, which is where the count and the modifiers come from.
    int pressedCount = 1;
    // pending_mouse_down being Some: a press is waiting to become a click.
    bool pressPending = false;
    // Where the press landed, and whether the pointer has since travelled far
    // enough to call it a drag. GPUI starts a drag from the move rather than
    // the press, and a drag takes the release the click would have had.
    float pressedX = 0;
    float pressedY = 0;
    bool pressedMoved = false;
    MouseButton pressedButton = MouseButton::Left;
    Modifiers pressedModifiers = {};
    // The drag in flight: what the press picked up, and which drop target the
    // pointer is over right now. GPUI keeps the same pair — `active_drag` and
    // the hitbox its drop handlers consult — on its Window.
    DragPayload activeDrag = {};
    int dragOverId = 0;
    // AnyDrag::cursor_offset: where inside the dragged element the press
    // landed, so whatever is drawn under the pointer sits where the element
    // was rather than jumping its corner to the cursor.
    float dragOffX = 0;
    float dragOffY = 0;
    bool eatReturn = false;
    // pending_keyboard_down: an Enter or Space is down on the focused
    // element, and the generation the focus was at when it went down.
    bool keyPressPending = false;
    int keyPressGen = 0;
    // The scrollbar being dragged, and how far into its thumb the press
    // landed. GPUI keeps the same pair in ScrollbarState::drag_pos.
    int scrollDragId = 0;
    float scrollDragGrab = 0;
    // Which of the box's two bars is being dragged.
    bool scrollDragHorizontal = false;
    InputState* input = nullptr;
    // This window's one TooltipOverlay. Created on first use, the way a
    // field's blink cursor is.
    EntityId tooltip = {};
    Overlay overlay = {};
    InspectorState inspector = {};
    MenuState menu = {};
    Vec<FocusRect> focusEls;
    // The focus trap a container asked to hold focus this frame, settled
    // after the focusables are collected. 0 when nothing asked.
    int pendingTrap = 0;
    Vec<KeyedSlot> keyed;
    // The frame's key contexts and action handlers, in tree order.
    Vec<DispatchNode> dispatch;
    Vec<MotionSlotRec> motionSlots;
    WinOpts opts = {};
    // Window-level subscriptions bound to view entities.
    Listener onKey = {};
    Listener onClick = {};
    Listener onMouseDown = {};
    Listener onMouseUp = {};
    Listener onMouseMove = {};
    Listener onMouseExit = {};
    Listener onScrollWheel = {};
    // Armed timers, any number of them.
    Vec<TimerSub> timers;
    int nextTimerId = 1;
    // Which InputState had focus last frame, so the runtime can start and stop
    // its caret without every app wiring that up.
    InputState* prevInput = nullptr;
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
    l.argBound = true;
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
        l.argBound = true;
    }
    return l;
}

// The same, for the value a widget produces rather than one its caller chose:
// which day of the calendar, the state a checkbox activation lands on. Rust
// passes that beside whatever the closure captured, so a caller that already
// bound its own — which of ten toggles this is — keeps it.
inline Listener ListenerFill(Listener l, intptr_t v) {
    if (l.IsValid() && !l.argBound) {
        l.arg = v;
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

// The same fill-me-in handler as the third Listen above, bound to another
// entity: the component supplies the value — which menu row was taken —
// rather than the caller having captured one.
template <typename T, typename E>
Listener ListenTo(Entity<T> e, void (*fn)(T*, Ctx*, const E*, intptr_t)) {
    Listener l;
    l.fn = (void*)fn;
    l.view = e.id;
    l.hasArg = true;
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
    l.argBound = true;
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

// The transition state behind one id, created zeroed on first ask and marked
// as wanted by this frame. Everything about it is in base/motion.h; this is
// the store, which has to live with the window.
void* WindowMotionState(Window* win, uint32_t key, int size);
// Drop what this frame did not ask for, which is what GPUI does with the
// state of an element it no longer renders.
void WindowMotionSweep(Window* win);
void WindowMotionFree(Window* win);

template <typename T>
T* KeyedState(Ctx* cx, uint32_t key) {
    void* p = WindowKeyedState(cx->win, key, (int)sizeof(T), &EntityDropT<T>);
    return (T*)p;
}

// The same window-keyed state, as an entity. `use_keyed_state` in Rust hands
// back an `Entity<T>`, which is what lets the state own timers and listeners
// of its own; KeyedState above is only a pointer, so nothing can be bound to
// it. A widget whose behavior outlives one frame — a hover card counting down
// to open — needs this one.
EntityId WindowKeyedEntity(Window* win, App* app, uint32_t key, void* fresh,
                           DropFn drop);

template <typename T>
Entity<T> KeyedEntity(Ctx* cx, uint32_t key) {
    Entity<T> e;
    e.id = WindowKeyedEntity(cx->win, cx->app, key, new T(), &EntityDropT<T>);
    return e;
}

// Window-level subscriptions. GPUI spells these window.on_key_down and
// cx.spawn + Timer::after; here each one is a Listener bound to a view.
void WindowOnKey(Window* win, Listener l);
// Fires for a click no element handled — the outside click that dismisses an
// overlay. Elements carry their own listener; this is not a dispatch table.
void WindowOnUnhandledClick(Window* win, Listener l);

// window.toggle_inspector, and the picking mode its magnifier button starts.
// Ctrl+Shift+I toggles it, as it does in Rust on everything but macOS.
void WindowToggleInspector(Window* win);
void WindowInspectorPick(Window* win, bool picking);
const InspectorState* WindowInspector(Ctx* cx);

// window.active_drag: what a press picked up, or an invalid payload when
// nothing is being dragged. A drop target reads it to decide whether to show
// itself at all, the way Rust's `drag_over::<T>` only exists for a drag of
// that type.
const DragPayload* WindowActiveDrag(Ctx* cx);
// window.is_window_active().
bool WindowIsActive(Ctx* cx);
// What the platform calls when the window takes or loses the focus.
void WindowSetActive(Window* win, bool active);
// Which element the drag is over, of those that take its kind — 0 for none.
// `El::Click(id)` names an element, so this answers with that id.
int WindowDragOverId(Ctx* cx);
// The same cursor offset, for whoever draws the thing being dragged.
Point WindowDragOffset(Ctx* cx);
// One subscription per event type, which is what window.on_mouse_event::<T>
// asks for in Rust. Each handler takes the matching event:
// `void OnDown(T* self, Ctx* cx, const MouseDownEvent* ev)`.
void WindowOnMouseDown(Window* win, Listener l);
void WindowOnMouseUp(Window* win, Listener l);
void WindowOnMouseMove(Window* win, Listener l);
void WindowOnMouseExit(Window* win, Listener l);
void WindowOnScrollWheel(Window* win, Listener l);
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

// Port of TooltipOverlay in crates/base/src/tooltip.rs. One per window, the
// way Rust keeps one overlay for every trigger in it, and an entity because it
// owns timers — the same reason BlinkCursor below is one.
//
// A tooltip is not "the hovered element has text". It waits out SHOW_DELAY
// before appearing, and on the way out it keeps a GRACE_PERIOD during which
// `hadRecent` is set: a trigger entered inside that window shows at once
// rather than making the reader wait again for a tip they were already
// reading. Rust guards each countdown with an epoch; there is at most one in
// flight here, so it is cancelled outright instead.
struct TooltipOverlay {
    // Heap-owned: the text came off a frame arena that is gone by the time the
    // countdown lands.
    Str text = {};
    Bounds triggerBounds = {};
    bool visible = false;
    bool hadRecent = false;
    int showTimer = 0;
    int hideTimer = 0;

    ~TooltipOverlay();

    static void OnShow(TooltipOverlay* self, Ctx* cx, const TickEvent* ev);
    static void OnHide(TooltipOverlay* self, Ctx* cx, const TickEvent* ev);
};

// request_show / request_hide, driven by the hover change in
// window_common.cpp rather than by each trigger element.
void TooltipRequestShow(Window* win, Str text, Bounds triggerBounds);
void TooltipRequestHide(Window* win);
// What the paint pass draws, or null when nothing is showing.
const TooltipOverlay* TooltipShowing(Window* win);
void TooltipPaint(PaintCtx* ctx, const TooltipOverlay* tip);

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

// The input engine, when a Ctx is already in hand — which it is inside any
// Render and any listener.
inline void InputFocus(InputState* s, Ctx* cx) {
    InputFocus(s, cx->app, cx->win);
}
inline void InputBlur(InputState* s, Ctx* cx) {
    InputBlur(s, cx->app, cx->win);
}
inline void InputMoveTo(InputState* s, Ctx* cx, int offset) {
    InputMoveTo(s, cx->app, cx->win, offset);
}
inline void InputSelectAll(InputState* s, Ctx* cx) {
    InputSelectAll(s, cx->app, cx->win);
}
inline void InputClean(InputState* s, Ctx* cx) {
    InputClean(s, cx->app, cx->win);
}
inline void InputReplaceAll(InputState* s, Ctx* cx, Str value) {
    InputReplaceAll(s, cx->app, cx->win, value);
}
inline void InputInsert(InputState* s, Ctx* cx, Str value) {
    InputInsert(s, cx->app, cx->win, value);
}
inline bool InputPerform(InputState* s, Ctx* cx, InputAction action,
                         bool shift = false) {
    return InputPerform(s, cx->app, cx->win, action, shift);
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
// Take it back off. The result is arena-allocated and empty when the
// clipboard holds no text.
Str ClipboardGetText(Arena* a, Window* win);

// cx.open_url: hand a link to whatever the desktop opens links with. Rust's
// takes an `&str` and answers nothing, and so does this — a browser that
// refuses to start is not something a caller can do anything about.
void OpenUrl(Str url);
int AppRun(App* app);
Window* WindowOpen(App* app, Str title, int dipW, int dipH, WinOpts opts);
void AppSetTitle(Window* win, Str title);
void AppRequestAnim(Window* win, bool on);
// One more frame, rather than every frame. Safe to call from inside a render.
void WindowRequestAnimationFrame(Window* win);

// Collect focusable click targets from last paint for Tab cycling.
void FocusCollect(Window* win, El* root);
int FocusNext(Window* win, int trapId, bool backward);
// Move the focus. Everything that focuses goes through here, so the
// generation a keystroke is stamped with counts every move.
void WindowSetFocusId(Window* win, int id);
// The action a keystroke resolves to for whatever has focus, and the handlers
// it is then offered to. Answers true when one of them kept it — Rust's
// `dispatch_action` plus the `cx.propagate()` that decides how far it goes.
bool WindowDispatchKeyAction(Window* win, int vk, bool shift, bool ctrl,
                             bool alt);
// cx.on_action: a handler that belongs to the application rather than to any
// element. Tried after the focused element's chain has passed on the action,
// which is where Rust's App-level handlers sit too. A plain function pointer,
// since these are the framework's own and have no view to update.
using ActionFn = void (*)(Window* win, ActionEvent* ev);
void AppOnAction(uint32_t action, ActionFn fn);
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
