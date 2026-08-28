/* libcurl: the one HTTP client a Linux desktop already has, found the same
   way X11, cairo and Pango are — pkg-config, at build time.

   It is a soft dependency, which the other three are not. A machine without
   `libcurl4-openssl-dev` still builds this tree; it just cannot fetch, and a
   remote image renders as its alt text there the way it did before any of
   this existed. cmd/build-linux.ts defines GPUI_HAVE_CURL when the package
   answers, and cmd/ubuntu-install-deps.sh installs it. */

#include "sys/http.h"

#if defined(GPUI_HAVE_CURL) && GPUI_HAVE_CURL
#include <curl/curl.h>
#endif

namespace gpui {

#if defined(GPUI_HAVE_CURL) && GPUI_HAVE_CURL

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

// Returns short of `n * size` to tell curl to stop, which is how a body over
// the cap is refused rather than truncated.
static size_t OnBody(char* data, size_t size, size_t n, void* userp) {
    HttpRsp* out = (HttpRsp*)userp;
    size_t want = size * n;
    if ((int64_t)out->body.len + (int64_t)want > (int64_t)kHttpMaxBody) {
        return 0;
    }
    uint8_t* dst = out->body.AppendBlanks((int)want);
    if (!dst) {
        return 0;
    }
    memcpy(dst, data, want);
    return want;
}

static bool HttpGetInternal(Str url, HttpRsp* out, bool noRedirect) {
    if (!out || !HttpUrlIsRemote(url)) {
        return false;
    }
    CURL* c = curl_easy_init();
    if (!c) {
        return false;
    }
    // curl wants a C string and Str is a slice, so the URL is copied once.
    char* u = AllocArray<char>(url.len + 1);
    if (!u) {
        curl_easy_cleanup(c);
        return false;
    }
    memcpy(u, url.s, (size_t)url.len);
    u[url.len] = 0;

    curl_easy_setopt(c, CURLOPT_URL, u);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, noRedirect ? 0L : 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, (long)kHttpTimeoutMs);
    curl_easy_setopt(c, CURLOPT_USERAGENT, "gpui/1.0");
    // This runs on a worker thread and curl's alarm-based DNS timeout is not
    // something to install from one.
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, OnBody);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, out);
    // http and https and nothing else, whatever a redirect asks for. The
    // string form is 7.85 and later; older curl takes the bitmask, which 7.85
    // deprecated rather than removed — and which -Werror will not let us name
    // once it is. Both spellings are enum values rather than macros, so the
    // version is what this can ask about.
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(c, CURLOPT_PROTOCOLS,
                     (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif

    CURLcode err = curl_easy_perform(c);
    bool ok = err == CURLE_OK;
    if (ok) {
        long status = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);
        out->status = (int)status;
        if (noRedirect && status >= 300 && status < 400) {
            char* redirect = nullptr;
            if (curl_easy_getinfo(c, CURLINFO_REDIRECT_URL, &redirect) ==
                    CURLE_OK &&
                redirect) {
                out->redirectUrl = StrDup(Str(redirect));
            }
        }
        char* ct = nullptr;
        if (curl_easy_getinfo(c, CURLINFO_CONTENT_TYPE, &ct) == CURLE_OK &&
            ct) {
            Str s = StrDup(Str(ct));
            TrimMediaType(&s);
            out->contentType = s;
        }
    }
    curl_easy_cleanup(c);
    Free(nullptr, u);
    if (!ok) {
        out->body.Reset();
    }
    return ok;
}

bool HttpGet(Str url, HttpRsp* out) {
    return HttpGetInternal(url, out, false);
}

bool HttpGetNoRedirect(Str url, HttpRsp* out) {
    return HttpGetInternal(url, out, true);
}

#else

bool HttpGet(Str url, HttpRsp* out) {
    // Built without libcurl: there is no client here to make the request
    // with, so every fetch fails the way a request to an unreachable host
    // would and the picture stays its alt text.
    (void)url;
    (void)out;
    return false;
}

bool HttpGetNoRedirect(Str url, HttpRsp* out) {
    (void)url;
    (void)out;
    return false;
}

#endif

} // namespace gpui
