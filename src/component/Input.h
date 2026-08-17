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
    Func0 onChange;

    static Input* New(Ctx* cx, Str id, LineInput* state);
    Input* Label(Str s);
    Input* OnChange(Func0 fn);
    El* IntoEl();
};

struct Textarea {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    const char* text = nullptr;
    Func0 onFocus;

    static Textarea* New(Ctx* cx, Str id, const char* text);
    Textarea* OnFocus(Func0 fn);
    El* IntoEl();
};

struct NumberInput {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    LineInput* state = nullptr;
    Func0 onInc;
    Func0 onDec;

    static NumberInput* New(Ctx* cx, LineInput* state);
    NumberInput* OnInc(Func0 fn);
    NumberInput* OnDec(Func0 fn);
    El* IntoEl();
};

struct OtpInput {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const char* value = nullptr;
    int len = 0;
    int slots = 6;
    Func0 onFocus;

    static OtpInput* New(Ctx* cx, const char* value, int len);
    OtpInput* OnFocus(Func0 fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
