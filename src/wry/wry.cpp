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

static bool StrStartsWith(Str s, Str prefix) {
    if (prefix.len > s.len) {
        return false;
    }
    return memcmp(s.s, prefix.s, (size_t)prefix.len) == 0;
}

Str WorkAroundUriPrefix(Str httpOrHttps, Str protocol) {
    return base::FormatTemp("%s://%s.", httpOrHttps, protocol);
}

bool IsWorkAroundUri(Str uri, Str httpOrHttps, Str protocol) {
    return StrStartsWith(uri, WorkAroundUriPrefix(httpOrHttps, protocol));
}

// `uri.replace(original_prefix, work_around_prefix)` and the reverse. Only
// the leading occurrence can be either prefix, so this is a prefix swap.
static Str SwapPrefix(Str uri, Str from, Str to) {
    if (!StrStartsWith(uri, from)) {
        return uri;
    }
    Str rest = Str(uri.s + from.len, uri.len - from.len);
    return base::FormatTemp("%s%s", to, rest);
}

Str ApplyUriWorkAround(Str uri, Str httpOrHttps, Str protocol) {
    return SwapPrefix(uri, base::FormatTemp("%s://", protocol),
                      WorkAroundUriPrefix(httpOrHttps, protocol));
}

Str RevertUriWorkAround(Str uri, Str httpOrHttps, Str protocol) {
    return SwapPrefix(uri, WorkAroundUriPrefix(httpOrHttps, protocol),
                      base::FormatTemp("%s://", protocol));
}

}  // namespace wry
