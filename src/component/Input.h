/* Themed input — crates/ui/src/input */

#pragma once

#include "component/Common.h"

namespace gpui {

namespace component {

struct Input {
    Arena* a = nullptr;
    Str id = {};
    LineInput* state = nullptr;
    Str label = {};
    Func0 onChange;

    static Input* New(Arena* a, Str id, LineInput* state);
    Input* Label(Str s);
    Input* OnChange(Func0 fn);
    El* IntoEl();
};

struct Textarea {
    Arena* a = nullptr;
    Str id = {};
    const char* text = nullptr;
    Func0 onFocus;

    static Textarea* New(Arena* a, Str id, const char* text);
    Textarea* OnFocus(Func0 fn);
    El* IntoEl();
};

struct NumberInput {
    Arena* a = nullptr;
    LineInput* state = nullptr;
    Func0 onInc;
    Func0 onDec;

    static NumberInput* New(Arena* a, LineInput* state);
    NumberInput* OnInc(Func0 fn);
    NumberInput* OnDec(Func0 fn);
    El* IntoEl();
};

struct OtpInput {
    Arena* a = nullptr;
    const char* value = nullptr;
    int len = 0;
    int slots = 6;
    Func0 onFocus;

    static OtpInput* New(Arena* a, const char* value, int len);
    OtpInput* OnFocus(Func0 fn);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
