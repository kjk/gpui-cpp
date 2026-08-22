/* WinHTTP: the GET the fetch table makes off the main thread.

   WinHTTP rather than WinINet because this runs off the UI thread and wants
   no per-user cache and no dial-up prompt, and rather than sockets because
   the TLS is the point — the system's, with the system's root store. */

#include "sys/http.h"

#include <winhttp.h>

namespace gpui {

// One WinHTTP handle, closed however the function below leaves. There is no
// exception here to unwind; this is only so that the several early returns do
// not each have to remember three closes.
struct WinHttpHandle {
    HINTERNET h = nullptr;
    ~WinHttpHandle() {
        if (h) {
            WinHttpCloseHandle(h);
        }
    }
};

// UTF-8 to UTF-16, into a buffer the caller frees with Free.
static wchar_t* ToWide(Str s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, nullptr, 0);
    wchar_t* w = AllocArray<wchar_t>(n + 1);
    if (!w) {
        return nullptr;
    }
    if (n > 0) {
        MultiByteToWideChar(CP_UTF8, 0, s.s, s.len, w, n);
    }
    w[n] = 0;
    return w;
}

static Str FromWide(const wchar_t* w) {
    if (!w) {
        return {};
    }
    int n =
        WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) {
        return {};
    }
    char* s = AllocArray<char>(n);
    if (!s) {
        return {};
    }
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n, nullptr, nullptr);
    return Str(s, n - 1);
}

// "image/png; charset=..." -> "image/png", lowercased where it stands.
static void TrimMediaType(Str* s) {
    for (int i = 0; i < s->len; i++) {
        char c = s->s[i];
        if (c == ';' || c == ' ') {
            s->len = i;
            break;
        }
        if (c >= 'A' && c <= 'Z') {
            s->s[i] = (char)(c - 'A' + 'a');
        }
    }
}

// Everything after the handles are open. Split out so the closes above it
// happen once rather than at each of the ways this can stop.
static bool ReadResponse(HINTERNET req, HttpRsp* out) {
    DWORD status = 0;
    DWORD size = sizeof(status);
    if (!WinHttpQueryHeaders(
            req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
            WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }
    out->status = (int)status;

    wchar_t ctype[128] = {};
    size = sizeof(ctype);
    if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_TYPE,
                            WINHTTP_HEADER_NAME_BY_INDEX, ctype, &size,
                            WINHTTP_NO_HEADER_INDEX)) {
        Str ct = FromWide(ctype);
        TrimMediaType(&ct);
        out->contentType = ct;
    }

    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(req, &avail)) {
            return false;
        }
        if (avail == 0) {
            return true;
        }
        if ((int64_t)out->body.len + (int64_t)avail > (int64_t)kHttpMaxBody) {
            return false; // refused, not truncated
        }
        int at = out->body.len;
        uint8_t* dst = out->body.AppendBlanks((int)avail);
        if (!dst) {
            return false;
        }
        DWORD got = 0;
        if (!WinHttpReadData(req, dst, avail, &got)) {
            return false;
        }
        out->body.len = at + (int)got;
    }
}

bool HttpGet(Str url, HttpRsp* out) {
    if (!out || !HttpUrlIsRemote(url)) {
        return false;
    }
    wchar_t* wurl = ToWide(url);
    if (!wurl) {
        return false;
    }

    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    uc.dwSchemeLength = (DWORD)-1;
    uc.dwHostNameLength = (DWORD)-1;
    uc.dwUrlPathLength = (DWORD)-1;
    uc.dwExtraInfoLength = (DWORD)-1;
    wchar_t host[256] = {};
    bool cracked =
        WinHttpCrackUrl(wurl, 0, 0, &uc) && uc.dwHostNameLength > 0 &&
        uc.dwHostNameLength < (DWORD)(sizeof(host) / sizeof(host[0]));
    if (cracked) {
        memcpy(host, uc.lpszHostName,
               (size_t)uc.dwHostNameLength * sizeof(wchar_t));
    }
    // The path and the query sit next to each other in the same string, so
    // one run covers both; a URL with neither is "/".
    DWORD pathLen = uc.dwUrlPathLength + uc.dwExtraInfoLength;
    wchar_t* path = cracked ? AllocArray<wchar_t>(pathLen + 2) : nullptr;
    if (cracked && path && pathLen > 0) {
        memcpy(path, uc.lpszUrlPath, (size_t)pathLen * sizeof(wchar_t));
    }
    Free(nullptr, wurl);
    if (!cracked || !path) {
        Free(nullptr, path);
        return false;
    }

    bool ok = false;
    {
        WinHttpHandle session;
        // WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY wants Windows 8.1; this one is
        // the constant that works everywhere this tree runs.
        session
            .h = WinHttpOpen(L"gpui2/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (session.h) {
            WinHttpSetTimeouts(session.h, kHttpTimeoutMs, kHttpTimeoutMs,
                               kHttpTimeoutMs, kHttpTimeoutMs);
            WinHttpHandle conn;
            conn.h = WinHttpConnect(session.h, host, uc.nPort, 0);
            if (conn.h) {
                DWORD flags = uc.nScheme == INTERNET_SCHEME_HTTPS
                                  ? WINHTTP_FLAG_SECURE
                                  : 0;
                WinHttpHandle req;
                req.h = WinHttpOpenRequest(
                    conn.h, L"GET", pathLen > 0 ? path : L"/", nullptr,
                    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
                if (req.h &&
                    WinHttpSendRequest(req.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                    WinHttpReceiveResponse(req.h, nullptr)) {
                    ok = ReadResponse(req.h, out);
                }
            }
        }
    }
    Free(nullptr, path);
    if (!ok) {
        out->body.Reset();
    }
    return ok;
}

} // namespace gpui
