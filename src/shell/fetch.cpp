#include "shell/fetch.h"

#include "sys/http.h"

namespace gpui::shell {

static FetchHttpSend gFetchHttpSendForTests = nullptr;

void FetchSetHttpSendForTests(FetchHttpSend send) {
    gFetchHttpSendForTests = send;
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
        if (url.s[i] == ':' && url.s[i + 1] == '/' && url.s[i + 2] == '/') {
            schemeEnd = i;
            break;
        }
    }
    if (schemeEnd <= 0) {
        FetchError(error, StrL("fetch URL must be an absolute http(s) URL"));
        return false;
    }
    Str scheme(url.s, schemeEnd);
    if (!StrEqI(scheme, StrL("http")) && !StrEqI(scheme, StrL("https"))) {
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
            FetchError(
                error,
                StrL("fetch URL host is invalid or carries credentials"));
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
                    FetchError(error,
                               StrL("an IPv6 fetch host must use brackets"));
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
                     c == '*' || c == '+' || c == '-' || c == '.' || c == '^' ||
                     c == '_' || c == '`' || c == '|' || c == '~';
        if (!token) return false;
    }
    return true;
}

bool FetchAuthorize(Str url, Str method, const Capabilities& capabilities,
                    Str* error) {
    if (error) {
        StrFree(*error);
        *error = {};
    }
    FetchUrl parsed;
    if (!ParseFetchUrl(url, &parsed, error)) return false;
    Str verb = method.len > 0 ? method : StrL("GET");
    if (capabilities.MayRequest(parsed.scheme, parsed.host, parsed.port,
                                parsed.hasPort, verb, parsed.path)) {
        return true;
    }
    FetchError(error,
               fmt("HTTP request %s %s is not granted; add it to "
                   "capabilities.network.hosts or capabilities.network.http",
                   verb, url));
    return false;
}

// The port a scheme means when the URL does not say. An origin comparison
// has to agree that https://h and https://h:443 are the same place.
static uint16_t EffectivePort(const FetchUrl& url) {
    if (url.hasPort) return url.port;
    return StrEqI(url.scheme, StrL("https")) ? 443 : 80;
}

bool FetchSameOrigin(Str left, Str right) {
    FetchUrl a;
    FetchUrl b;
    if (!ParseFetchUrl(left, &a, nullptr) ||
        !ParseFetchUrl(right, &b, nullptr)) {
        return false;
    }
    return StrEqI(a.scheme, b.scheme) && StrEqI(a.host, b.host) &&
           EffectivePort(a) == EffectivePort(b);
}

bool FetchAuthorizeRedirect(const Capabilities& capabilities, Str method,
                            Str current, Str next,
                            const Vec<FetchHeader>& headers, Str* error) {
    Str inner = {};
    if (!FetchAuthorize(next, method, capabilities, &inner)) {
        FetchError(error, fmt("redirect target refused: %s", inner));
        StrFree(inner);
        return false;
    }
    StrFree(inner);

    FetchUrl before;
    FetchUrl after;
    if (!ParseFetchUrl(current, &before, error) ||
        !ParseFetchUrl(next, &after, error)) {
        return false;
    }
    if (StrEqI(before.scheme, StrL("https")) &&
        !StrEqI(after.scheme, StrL("https"))) {
        FetchError(error, fmt("redirect from %s to %s refused because it is "
                              "an HTTPS downgrade",
                              current, next));
        return false;
    }
    bool sameOrigin = FetchSameOrigin(current, next);
    Str verb = method.len > 0 ? method : StrL("GET");
    if (!StrEq(verb, StrL("GET")) && !sameOrigin) {
        FetchError(error,
                   fmt("cross-origin redirect from %s to %s refused because "
                       "it would replay a %s request",
                       current, next, verb));
        return false;
    }
    // A bearer credential may follow a same-origin redirect, never a
    // cross-origin one, even when both hosts are individually granted.
    for (int i = 0; i < headers.len; i++) {
        if (StrEqI(headers[i].name, StrL("authorization")) && !sameOrigin) {
            FetchError(error, fmt("cross-origin redirect from %s to %s refused "
                                  "because the request carries Authorization",
                                  current, next));
            return false;
        }
    }
    if (headers.len > 0 && !sameOrigin) {
        FetchError(error,
                   fmt("cross-origin redirect from %s to %s refused because "
                       "caller-supplied request headers would be replayed",
                       current, next));
        return false;
    }
    return true;
}

void FetchResult::Free() {
    StrFree(url);
    StrFree(body);
    StrFree(error);
    *this = {};
}

void FetchRequest::Free() {
    StrFree(url);
    StrFree(method);
    for (int i = 0; i < headers.len; i++) {
        StrFree(headers[i].name);
        StrFree(headers[i].value);
    }
    VecReset(headers);
    StrFree(body);
    url = {};
    method = {};
    body = {};
}

bool FetchHeaderIsProhibited(Str name) {
    static const char* kProhibited[] = {"host",
                                        "content-length",
                                        "connection",
                                        "expect",
                                        "proxy-authenticate",
                                        "proxy-authorization",
                                        "te",
                                        "trailer",
                                        "transfer-encoding",
                                        "upgrade"};
    for (size_t i = 0; i < sizeof(kProhibited) / sizeof(kProhibited[0]); i++) {
        if (StrEqI(name, Str(kProhibited[i]))) return true;
    }
    return false;
}

bool FetchFollowsLocation(int status) {
    return status == 301 || status == 302 || status == 303 || status == 307 ||
           status == 308;
}

void FetchRewriteRedirect(int status, Str* method, Vec<FetchHeader>* headers,
                          Str* body) {
    if (!method) return;
    Str verb = method->len > 0 ? *method : StrL("GET");
    bool becomesGet =
        ((status == 301 || status == 302) && StrEq(verb, StrL("POST"))) ||
        (status == 303 && !StrEq(verb, StrL("HEAD")));
    if (!becomesGet) return;

    StrFree(*method);
    *method = StrDup(StrL("GET"));
    if (body) {
        StrFree(*body);
        *body = {};
    }
    // The entity is gone, so the two headers that described it go with it.
    if (headers) {
        for (int i = headers->len - 1; i >= 0; i--) {
            if (StrEqI((*headers)[i].name, StrL("content-length")) ||
                StrEqI((*headers)[i].name, StrL("content-type"))) {
                StrFree((*headers)[i].name);
                StrFree((*headers)[i].value);
                VecRemoveAt(*headers, i);
            }
        }
    }
}

// The walk owns the three things a redirect may rewrite, plus the header list
// that travels with them.
struct FetchWalk {
    Str url;
    Str method;
    Str body;
    Vec<FetchHeader> headers;

    void Free() {
        StrFree(url);
        StrFree(method);
        StrFree(body);
        for (int i = 0; i < headers.len; i++) {
            StrFree(headers[i].name);
            StrFree(headers[i].value);
        }
        VecReset(headers);
    }
};

bool FetchSend(const FetchRequest& request, const Capabilities& capabilities,
               FetchResult* out) {
    if (!out) return false;
    out->Free();

    FetchWalk walk;
    walk.url = StrDup(request.url);
    walk.method = StrDup(request.method.len > 0 ? request.method : StrL("GET"));
    walk.body = StrDup(request.body);
    bool allocated =
        walk.url.s && walk.method.s && (walk.body.s || request.body.len == 0);
    for (int i = 0; i < request.headers.len && allocated; i++) {
        FetchHeader copy;
        copy.name = StrDup(request.headers[i].name);
        copy.value = StrDup(request.headers[i].value);
        if (!copy.name.s || !copy.value.s) {
            StrFree(copy.name);
            StrFree(copy.value);
            allocated = false;
        } else {
            VecAppend(walk.headers, copy);
        }
    }
    if (!allocated) {
        FetchError(&out->error, StrL("allocating the fetch request failed"));
        walk.Free();
        return false;
    }
    if (walk.body.len > kFetchMaxRequestBody) {
        FetchError(&out->error,
                   fmt("fetch request body exceeded the %d byte limit",
                       kFetchMaxRequestBody));
        walk.Free();
        return false;
    }
    if (!FetchAuthorize(walk.url, walk.method, capabilities, &out->error)) {
        walk.Free();
        return false;
    }

    Vec<HttpHeader> wire;
    for (int redirects = 0;; redirects++) {
        VecReset(wire);
        for (int i = 0; i < walk.headers.len; i++) {
            HttpHeader h;
            h.name = walk.headers[i].name;
            h.value = walk.headers[i].value;
            VecAppend(wire, h);
        }
        HttpReq req;
        req.url = walk.url;
        req.method = walk.method;
        req.headers = wire.len > 0 ? wire.els : nullptr;
        req.nHeaders = wire.len;
        req.body = walk.body;
        req.noRedirect = true;

        HttpRsp response;
        FetchHttpSend send =
            gFetchHttpSendForTests ? gFetchHttpSendForTests : HttpSend;
        if (!send(req, &response)) {
            FetchError(&out->error, fmt("fetching %s failed", walk.url));
            HttpRspFree(&response);
            VecReset(wire);
            walk.Free();
            return false;
        }

        if (FetchFollowsLocation(response.status)) {
            bool ok = true;
            if (!response.redirectUrl) {
                FetchError(&out->error,
                           fmt("redirect from %s has no valid Location header",
                               walk.url));
                ok = false;
            } else if (redirects >= kFetchMaxRedirects) {
                FetchError(&out->error,
                           fmt("fetch exceeded the %d redirect limit",
                               kFetchMaxRedirects));
                ok = false;
            }
            if (ok) {
                // The rewrite runs before the check, so what is authorized is
                // the request that would actually be sent.
                FetchRewriteRedirect(response.status, &walk.method,
                                     &walk.headers, &walk.body);
                ok = FetchAuthorizeRedirect(capabilities, walk.method, walk.url,
                                            response.redirectUrl, walk.headers,
                                            &out->error);
            }
            Str next = ok ? StrDup(response.redirectUrl) : Str{};
            if (ok && !next.s) {
                FetchError(&out->error,
                           StrL("allocating the redirect URL failed"));
                ok = false;
            }
            HttpRspFree(&response);
            if (!ok) {
                VecReset(wire);
                walk.Free();
                return false;
            }
            StrFree(walk.url);
            walk.url = next;
            continue;
        }

        if (response.body.len > kFetchMaxBody) {
            FetchError(&out->error,
                       fmt("response body from %s exceeded the %d byte limit",
                           walk.url, kFetchMaxBody));
            HttpRspFree(&response);
            VecReset(wire);
            walk.Free();
            return false;
        }
        out->status = response.status;
        out->body =
            StrDup(Str((const char*)response.body.els, response.body.len));
        bool copied = out->body.s || response.body.len == 0;
        HttpRspFree(&response);
        VecReset(wire);
        if (!copied) {
            out->status = 0;
            FetchError(&out->error,
                       StrL("allocating the fetch response failed"));
            walk.Free();
            return false;
        }
        // The final URL is the answer's; the walk hands it over rather than
        // freeing it.
        out->url = walk.url;
        walk.url = {};
        walk.Free();
        return true;
    }
}

} // namespace gpui::shell
