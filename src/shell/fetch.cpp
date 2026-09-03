#include "shell/fetch.h"

#include "sys/http.h"

namespace gpui::shell {

static FetchHttpGet gFetchHttpGetForTests = nullptr;

void FetchSetHttpGetForTests(FetchHttpGet get) {
    gFetchHttpGetForTests = get;
}

struct FetchUrl {
    Str scheme;
    Str host;
    Str path;
    uint16_t port = 0;
    bool hasPort = false;
};

static void FetchError(Str* error, Str message) {
    if (!error) return;
    StrFree(*error);
    *error = StrDup(message);
}

static bool ParsePort(Str text, uint16_t* value) {
    if (text.len == 0 || text.len > 5) return false;
    uint32_t port = 0;
    for (int i = 0; i < text.len; i++) {
        if (text.s[i] < '0' || text.s[i] > '9') return false;
        port = port * 10 + (uint32_t)(text.s[i] - '0');
    }
    if (port == 0 || port > 65535) return false;
    *value = (uint16_t)port;
    return true;
}

static bool ParseFetchUrl(Str url, FetchUrl* parsed, Str* error) {
    if (parsed) *parsed = {};
    if (!url.s || url.len <= 0 || url.len > 32768) {
        FetchError(error, StrL("invalid fetch URL"));
        return false;
    }
    int schemeEnd = -1;
    for (int i = 0; i + 2 < url.len; i++) {
        if (url.s[i] == ':' && url.s[i + 1] == '/' &&
            url.s[i + 2] == '/') {
            schemeEnd = i;
            break;
        }
    }
    if (schemeEnd <= 0) {
        FetchError(error, StrL("fetch URL must be an absolute http(s) URL"));
        return false;
    }
    Str scheme(url.s, schemeEnd);
    if (!StrEqI(scheme, StrL("http")) &&
        !StrEqI(scheme, StrL("https"))) {
        FetchError(error, StrL("fetch URL must use http or https"));
        return false;
    }
    int authorityStart = schemeEnd + 3;
    int authorityEnd = authorityStart;
    while (authorityEnd < url.len && url.s[authorityEnd] != '/' &&
           url.s[authorityEnd] != '?' && url.s[authorityEnd] != '#') {
        authorityEnd++;
    }
    if (authorityEnd == authorityStart) {
        FetchError(error, StrL("fetch URL has no host"));
        return false;
    }
    for (int i = authorityStart; i < authorityEnd; i++) {
        unsigned char c = (unsigned char)url.s[i];
        if (c <= 0x20 || c >= 0x7f || c == '@') {
            FetchError(error, StrL("fetch URL host is invalid or carries credentials"));
            return false;
        }
    }

    int hostStart = authorityStart;
    int hostEnd = authorityEnd;
    int portStart = -1;
    if (url.s[hostStart] == '[') {
        hostStart++;
        hostEnd = hostStart;
        while (hostEnd < authorityEnd && url.s[hostEnd] != ']') hostEnd++;
        if (hostEnd >= authorityEnd || hostEnd == hostStart) {
            FetchError(error, StrL("fetch URL has an invalid IPv6 host"));
            return false;
        }
        if (hostEnd + 1 < authorityEnd) {
            if (url.s[hostEnd + 1] != ':') {
                FetchError(error, StrL("fetch URL authority is invalid"));
                return false;
            }
            portStart = hostEnd + 2;
        }
    } else {
        for (int i = authorityStart; i < authorityEnd; i++) {
            if (url.s[i] == ':') {
                if (portStart >= 0) {
                    FetchError(error, StrL("an IPv6 fetch host must use brackets"));
                    return false;
                }
                hostEnd = i;
                portStart = i + 1;
            }
        }
    }
    if (hostEnd <= hostStart) {
        FetchError(error, StrL("fetch URL has no host"));
        return false;
    }
    uint16_t port = 0;
    bool hasPort = portStart >= 0;
    if (hasPort &&
        !ParsePort(Str(url.s + portStart, authorityEnd - portStart), &port)) {
        FetchError(error, StrL("fetch URL has an invalid port"));
        return false;
    }
    int pathStart = authorityEnd;
    int pathEnd = pathStart;
    if (pathStart < url.len && url.s[pathStart] == '/') {
        while (pathEnd < url.len && url.s[pathEnd] != '?' &&
               url.s[pathEnd] != '#') {
            pathEnd++;
        }
    }
    if (parsed) {
        parsed->scheme = scheme;
        parsed->host = Str(url.s + hostStart, hostEnd - hostStart);
        parsed->path = pathEnd > pathStart
                           ? Str(url.s + pathStart, pathEnd - pathStart)
                           : StrL("/");
        parsed->port = port;
        parsed->hasPort = hasPort;
    }
    return true;
}

// RFC 7230's `token`, which is what `reqwest::Method::from_bytes` accepts.
bool FetchIsHttpMethod(Str method) {
    if (!method.s || method.len <= 0) return false;
    for (int i = 0; i < method.len; i++) {
        char c = method.s[i];
        bool token = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9') || c == '!' || c == '#' ||
                     c == '$' || c == '%' || c == '&' || c == '\'' ||
                     c == '*' || c == '+' || c == '-' || c == '.' ||
                     c == '^' || c == '_' || c == '`' || c == '|' || c == '~';
        if (!token) return false;
    }
    return true;
}

bool FetchAuthorizeGet(Str url, const Capabilities& capabilities,
                       Str* error) {
    if (error) {
        StrFree(*error);
        *error = {};
    }
    FetchUrl parsed;
    if (!ParseFetchUrl(url, &parsed, error)) return false;
    if (capabilities.MayRequest(parsed.scheme, parsed.host, parsed.port,
                                parsed.hasPort, StrL("GET"), parsed.path)) {
        return true;
    }
    FetchError(error,
               fmt("HTTP request GET %s is not granted; add it to "
                   "capabilities.network.hosts or capabilities.network.http",
                   url));
    return false;
}

void FetchResult::Free() {
    StrFree(url);
    StrFree(body);
    StrFree(error);
    *this = {};
}

static bool IsRedirect(int status) {
    return status == 301 || status == 302 || status == 303 ||
           status == 307 || status == 308;
}

bool FetchGet(Str url, const Capabilities& capabilities, FetchResult* out) {
    if (!out) return false;
    out->Free();
    Str current = StrDup(url);
    if (!current.s) {
        FetchError(&out->error, StrL("allocating the fetch URL failed"));
        return false;
    }
    for (int redirects = 0;; redirects++) {
        if (!FetchAuthorizeGet(current, capabilities, &out->error)) {
            StrFree(current);
            return false;
        }
        HttpRsp response;
        FetchHttpGet get = gFetchHttpGetForTests
                               ? gFetchHttpGetForTests
                               : HttpGetNoRedirect;
        if (!get(current, &response)) {
            FetchError(&out->error, fmt("fetching %s failed", current));
            HttpRspFree(&response);
            StrFree(current);
            return false;
        }
        if (IsRedirect(response.status)) {
            if (!response.redirectUrl) {
                FetchError(&out->error,
                           fmt("redirect from %s has no valid Location header",
                               current));
                HttpRspFree(&response);
                StrFree(current);
                return false;
            }
            if (redirects >= kFetchMaxRedirects) {
                FetchError(&out->error,
                           fmt("fetch exceeded the %d redirect limit",
                               kFetchMaxRedirects));
                HttpRspFree(&response);
                StrFree(current);
                return false;
            }
            FetchUrl before;
            FetchUrl after;
            if (!ParseFetchUrl(current, &before, &out->error) ||
                !ParseFetchUrl(response.redirectUrl, &after, &out->error)) {
                HttpRspFree(&response);
                StrFree(current);
                return false;
            }
            if (StrEqI(before.scheme, StrL("https")) &&
                !StrEqI(after.scheme, StrL("https"))) {
                FetchError(&out->error,
                           fmt("redirect from %s to %s refused because it is "
                               "an HTTPS downgrade",
                               current, response.redirectUrl));
                HttpRspFree(&response);
                StrFree(current);
                return false;
            }
            Str next = StrDup(response.redirectUrl);
            HttpRspFree(&response);
            StrFree(current);
            current = next;
            if (!current.s) {
                FetchError(&out->error,
                           StrL("allocating the redirect URL failed"));
                return false;
            }
            continue;
        }
        if (response.body.len > kFetchMaxBody) {
            FetchError(&out->error,
                       fmt("response body from %s exceeded the %d byte limit",
                           current, kFetchMaxBody));
            HttpRspFree(&response);
            StrFree(current);
            return false;
        }
        out->status = response.status;
        out->url = current;
        current = {};
        out->body = StrDup(Str((const char*)response.body.els,
                               response.body.len));
        bool copied = out->body.s || response.body.len == 0;
        HttpRspFree(&response);
        if (!copied) {
            StrFree(out->url);
            out->url = {};
            out->status = 0;
            FetchError(&out->error,
                       StrL("allocating the fetch response failed"));
            return false;
        }
        return true;
    }
}

} // namespace gpui::shell
