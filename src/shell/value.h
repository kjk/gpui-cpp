#ifndef GPUI_SHELL_VALUE_H_
#define GPUI_SHELL_VALUE_H_

#include "shell/error.h"
#include "gpui/gpui.h"

namespace gpui::shell {

enum class BridgedKind : uint8_t {
    Nil,
    Bool,
    Number,
    String,
};

struct Bridged {
    BridgedKind kind = BridgedKind::Nil;
    bool boolean = false;
    double number = 0;
    Str string;

    static Bridged Nil();
    static Bridged Bool(bool value);
    static Bridged Number(double value);
    static Bridged String(Str value);
};

bool BridgedAsF32(const Bridged& value, float* out,
                  ShellError* error = nullptr);
bool BridgedAsString(const Bridged& value, Str* out,
                     ShellError* error = nullptr);
bool BridgedAsPixels(const Bridged& value, float* out,
                     ShellError* error = nullptr);
bool BridgedAsColor(const Bridged& value, Hsla* out,
                    ShellError* error = nullptr);
bool BridgedIsTruthy(const Bridged& value);
Str BridgedDescribe(Arena* arena, const Bridged& value);
bool BridgedArg(const Bridged* args, int count, int index, Str method,
                Bridged* out, ShellError* error = nullptr);

} // namespace gpui::shell
#endif // GPUI_SHELL_VALUE_H_
