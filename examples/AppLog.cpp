/* Shared example implementation of Base.h log(). */

#include "Base.h"

static void LogToFile(Str s) {
    static HANDLE h = INVALID_HANDLE_VALUE;
    if (h == INVALID_HANDLE_VALUE) {
        h = CreateFileA("out\\gpui2.log", GENERIC_WRITE, FILE_SHARE_READ,
                        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }
    if (s.s && s.len > 0) {
        DWORD w = 0;
        WriteFile(h, s.s, (DWORD)s.len, &w, nullptr);
        if (s.s[s.len - 1] != '\n') {
            WriteFile(h, "\n", 1, &w, nullptr);
        }
    }
}

void log(Str s) {
    if (s.s && s.len > 0) {
        OutputDebugStringA(s.s);
        if (s.s[s.len - 1] != '\n') {
            OutputDebugStringA("\n");
        }
    } else {
        OutputDebugStringA("\n");
    }
    LogToFile(s);
}
