/* Shared example implementation of Base.h log() / loga() / _uploadDebugReport. */

#include "base/Base.h"

static void LogToFile(Str s) {
    static HANDLE h = INVALID_HANDLE_VALUE;
    if (h == INVALID_HANDLE_VALUE) {
        h = CreateFileA("out\\gpui2.log", GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                        nullptr);
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

void loga(Str s) {
    log(s);
}

void _uploadDebugReport(Str condStr, Str fileLine, bool isCrash, bool captureCallstack) {
    (void)captureCallstack;
    char buf[1024];
    int n = 0;
    const char* cond = condStr.s ? condStr.s : "(null)";
    const char* loc = fileLine.s ? fileLine.s : "(unknown)";
    n = _snprintf_s(buf, sizeof(buf), _TRUNCATE, "ReportIf: %s at %s%s\n", cond, loc, isCrash ? " (crash)" : "");
    if (n > 0) {
        OutputDebugStringA(buf);
    }
#ifdef DEBUG
    if (isCrash) {
        DebugBreak();
    }
#else
    (void)isCrash;
#endif
}
