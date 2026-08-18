/* Themed input — crates/ui/src/input */

#include "component/Common.h"

namespace gpui {

namespace component {

struct Input {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    LineInput* state = nullptr;
    Str label = {};
    float width = kFill;
    Listener onChange;
    Listener onFocus;

    static Input* New(Ctx* cx, Str id, LineInput* state);
    Input* Label(Str s);
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
    Listener onFocus;

    static Textarea* New(Ctx* cx, Str id, const char* text);
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
