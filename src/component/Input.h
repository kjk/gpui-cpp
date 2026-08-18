/* Themed input — crates/ui/src/input */

#include "component/Common.h"

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
    LineInput* state = nullptr;
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
    Rgba textColor = {};
    bool hasTextColor = false;
    Listener onChange;
    Listener onFocus;
    Listener onClear;
    Listener onToggleMask;

    static Input* New(Ctx* cx, Str id, LineInput* state);
    Input* Label(Str s);
    Input* WithSize(UiSize s);
    Input* Align(InputAlign v);
    Input* Disabled(bool v);
    Input* Cleanable(bool v = true);
    Input* Masked(bool v);
    Input* MaskToggle(bool v = true);
    Input* Appearance(bool v);
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
    const char* text = nullptr;
    Str placeholder = {};
    int rows = 0;
    float height = 0;
    bool softWrap = true;
    Listener onFocus;

    static Textarea* New(Ctx* cx, Str id, const char* text);
    // Rust sizes a textarea by rows (`auto_grow(min, max)`); without one it
    // keeps the two-row default. An explicit height wins, as `.h(px(..))`
    // does there.
    Textarea* Rows(int n);
    Textarea* H(float px);
    Textarea* Placeholder(Str s);
    Textarea* SoftWrap(bool v);
    Textarea* OnFocus(Listener fn);
    El* IntoEl();
};

struct NumberInput {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    LineInput* state = nullptr;
    float width = kFill;
    Listener onInc;
    Listener onDec;

    static NumberInput* New(Ctx* cx, LineInput* state);
    // Fills its parent unless the caller sizes it, as in Rust.
    NumberInput* W(float v);
    NumberInput* OnInc(Listener fn);
    NumberInput* OnDec(Listener fn);
    El* IntoEl();
};

struct OtpInput {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const char* value = nullptr;
    int len = 0;
    int slots = 6;
    Listener onFocus;

    static OtpInput* New(Ctx* cx, const char* value, int len);
    OtpInput* OnFocus(Listener fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
