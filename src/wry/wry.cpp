/* The portable half of the port: wry/src/webview2/mod.rs's custom protocol
 * URI work-around, which is string work and nothing else.
 *
 * Part of the C++ port of lb-wry 0.53.3 (see src/wry/readme.md).
 *
 * It lives out here rather than in wry_win.cpp because the crate's own unit
 * test — `checks_if_custom_protocol_uri` — is over these three functions, and
 * tests/WryTests.cpp is one binary on every platform.
 */

#include "wry/wry.h"

namespace wry {

void CookieListFree(Vec<Cookie>* cookies) {
    if (!cookies) {
        return;
    }
    for (int i = 0; i < cookies->len; i++) {
        Cookie& cookie = cookies->els[i];
        base::StrFree(cookie.name);
        base::StrFree(cookie.value);
        base::StrFree(cookie.domain);
        base::StrFree(cookie.path);
    }
    cookies->Reset();
}

static bool StrStartsWith(Str s, Str prefix) {
    if (prefix.len > s.len) {
        return false;
    }
    return base::StrEq(Str(s.s, prefix.len), prefix);
}

Str WorkAroundUriPrefix(Str httpOrHttps, Str protocol) {
    return base::FormatTemp("%s://%s.", httpOrHttps, protocol);
}

bool IsWorkAroundUri(Str uri, Str httpOrHttps, Str protocol) {
    return StrStartsWith(uri, WorkAroundUriPrefix(httpOrHttps, protocol));
}

// `str::replace` replaces every non-overlapping occurrence, including a URL
// embedded in the query or fragment of another custom-protocol URL.
static Str ReplaceAll(Str value, Str from, Str to) {
    if (from.len == 0 || from.len > value.len) {
        return value;
    }
    int count = 0;
    for (int i = 0; i <= value.len - from.len;) {
        if (base::StrEq(Str(value.s + i, from.len), from)) {
            count++;
            i += from.len;
        } else {
            i++;
        }
    }
    if (count == 0) {
        return value;
    }
    int resultLen = value.len + count * (to.len - from.len);
    Str result = base::AllocStrTemp(resultLen + 1);
    if (!result.s) {
        return value;
    }
    int src = 0;
    int dst = 0;
    while (src < value.len) {
        if (src <= value.len - from.len &&
            base::StrEq(Str(value.s + src, from.len), from)) {
            memcpy(result.s + dst, to.s, (size_t)to.len);
            src += from.len;
            dst += to.len;
        } else {
            result.s[dst++] = value.s[src++];
        }
    }
    result.s[dst] = 0;
    result.len = dst;
    return result;
}

Str ApplyUriWorkAround(Str uri, Str httpOrHttps, Str protocol) {
    return ReplaceAll(uri, base::FormatTemp("%s://", protocol),
                      WorkAroundUriPrefix(httpOrHttps, protocol));
}

Str RevertUriWorkAround(Str uri, Str httpOrHttps, Str protocol) {
    return ReplaceAll(uri, WorkAroundUriPrefix(httpOrHttps, protocol),
                      base::FormatTemp("%s://", protocol));
}

}  // namespace wry
