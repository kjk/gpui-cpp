/* Themed input — crates/ui/src/input */

#include "ui/sizing.h"

namespace gpui {

namespace component {

enum class InputAlign : uint8_t {
    Left,
    Center,
    Right
};

struct Input {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    InputState* state = nullptr;
    Str label = {};
    float width = kFill;
    El* prefix = nullptr;
    El* suffix = nullptr;
    UiSize size = UiSize::Medium;
    InputAlign align = InputAlign::Left;
    bool disabled = false;
    bool cleanable = false;
    // A masked input draws bullets; mask_toggle adds the eye that flips it.
    bool masked = false;
    bool maskToggle = false;
    bool appearance = true;
    bool focusRing = true;
    Rgba textColor = {};
    bool hasTextColor = false;
    Listener onChange;
    Listener onFocus;
    Listener onClear;
    Listener onToggleMask;

    static Input* New(Ctx* cx, Str id, InputState* state);
    Input* Label(Str s);
    Input* WithSize(UiSize s);
    Input* Align(InputAlign v);
    Input* Disabled(bool v);
    Input* Cleanable(bool v = true);
    Input* Masked(bool v);
    Input* MaskToggle(bool v = true);
    Input* Appearance(bool v);
    // FocusableExt::focus_ring: no focus appearance on this control.
    Input* FocusRing(bool v);
    Input* TextColor(Rgba c);
    Input* OnClear(Listener fn);
    Input* OnToggleMask(Listener fn);
    // Rust's Input::prefix / Input::suffix: content inside the border box, on
    // either side of the editor. A prefix brings its own left padding.
    Input* Prefix(El* el);
    Input* Suffix(El* el);
    // Rust's Input fills its parent (`size_full`); a caller that puts one in a
    // row next to other content sizes it with `.w(px(..))` instead.
    Input* W(float v);
    Input* OnChange(Listener fn);
    Input* OnFocus(Listener fn);
    El* IntoEl();
};

struct Textarea {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    // Rust's is `Textarea::new(&state)` where the state is a TextareaState —
    // the same engine as an Input's, with InputKind::Textarea.
    InputState* state = nullptr;
    int rows = 0;
    float height = 0;
    bool softWrap = true;
    Listener onFocus;

    static Textarea* New(Ctx* cx, Str id, InputState* state);
    // Rust sizes a textarea by rows (`auto_grow(min, max)`); without one it
    // keeps the two-row default. An explicit height wins, as `.h(px(..))`
    // does there.
    Textarea* Rows(int n);
    Textarea* H(float px);
    Textarea* SoftWrap(bool v);
    Textarea* OnFocus(Listener fn);
    El* IntoEl();
};

struct NumberInput {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    InputState* state = nullptr;
    float width = kFill;
    UiSize size = UiSize::Medium;
    bool disabled = false;
    bool appearance = true;
    bool focusRing = true;
    El* suffix = nullptr;
    Background bg = {};
    bool hasBg = false;
    Rgba textColor = {};
    bool hasTextColor = false;
    Listener onInc;
    Listener onDec;
    Listener onFocus;

    static NumberInput* New(Ctx* cx, InputState* state);
    static NumberInput* New(Ctx* cx, Str id, InputState* state);
    // Fills its parent unless the caller sizes it, as in Rust.
    NumberInput* W(float v);
    NumberInput* WithSize(UiSize s);
    NumberInput* Disabled(bool v);
    NumberInput* Appearance(bool v);
    // FocusableExt::focus_ring: no focus appearance on this control.
    NumberInput* FocusRing(bool v);
    NumberInput* Suffix(El* el);
    NumberInput* Bg(Background c);
    NumberInput* TextColor(Rgba c);
    NumberInput* OnFocus(Listener fn);
    NumberInput* OnInc(Listener fn);
    NumberInput* OnDec(Listener fn);
    El* IntoEl();
};

struct OtpInput {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    const char* value = nullptr;
    int len = 0;
    int slots = 6;
    // The cells are split into this many groups, spaced further apart.
    int groups = 2;
    bool masked = false;
    bool disabled = false;
    bool focusRing = true;
    UiSize size = UiSize::Medium;
    float cellPx = 0; // with_size(px(..)): a custom cell edge
    Listener onFocus;
    // The field's own state, when it has one: the value, the focus and the
    // caret are its, and typing into it edits them. A caller with a fixed
    // value passes none and gets the cells with nothing behind them.
    Entity<OtpState> state = {};

    static OtpInput* New(Ctx* cx, const char* value, int len);
    static OtpInput* New(Ctx* cx, Str id, Entity<OtpState> state);
    OtpInput* Id(Str s);
    OtpInput* Slots(int n);
    OtpInput* Groups(int n);
    OtpInput* Masked(bool v);
    OtpInput* Disabled(bool v);
    // FocusableExt::focus_ring: no focus appearance on this control.
    OtpInput* FocusRing(bool v);
    OtpInput* WithSize(UiSize s);
    OtpInput* CellSize(float px);
    OtpInput* OnFocus(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
