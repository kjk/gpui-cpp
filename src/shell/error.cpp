#include "shell/error.h"

namespace gpui {

void ShellErrorClear(ShellError* error) {
    if (!error) {
        return;
    }
    StrFree(error->message);
    error->message = {};
}

void ShellErrorSet(ShellError* error, Str message) {
    if (!error) {
        return;
    }
    ShellErrorClear(error);
    error->message = StrDup(message);
}

} // namespace gpui
