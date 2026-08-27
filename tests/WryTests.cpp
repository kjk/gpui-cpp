/* wry/src/webview2/mod.rs's own test module: `checks_if_custom_protocol_uri`,
 * over the URI work-around a custom protocol goes through on Windows.
 *
 * The two `replace` helpers beside it carry no test upstream; the round trip
 * is asserted here because they are what the request handler and the initial
 * navigation each use one half of, and a mismatch between them would only
 * show up as a page that never loads. */

#include "Test.h"

void TestWryUri() {
    TestSuite("wry_uri");

    // `WebViewAttributes::default`, including the Darwin fields that live in
    // Rust's separate `PlatformSpecificWebViewAttributes` builder state.
    wry::WebViewAttributes attrs;
    utassert(attrs.visible);
    utassert(attrs.acceptFirstMouse == false);
    utassert(attrs.allowLinkPreview);
    utassert(!attrs.hasDataStoreIdentifier);
    utassert(!attrs.hasTrafficLightInset);
    utassert(!attrs.hasBackgroundThrottling);
    utassert(attrs.webviewConfiguration == nullptr);
    utassert(attrs.downloadStartedHandler == nullptr);
    utassert(attrs.downloadCompletedHandler == nullptr);
    utassert(attrs.dragDropHandler == nullptr);
    utassert(attrs.webviewEnvironment == nullptr);

    wry::Cookie cookie;
    utassert(cookie.session);
    utassert(!cookie.hasExpires);
    utassert(!cookie.hasMaxAge);
    utassert(!cookie.hasHttpOnly);
    utassert(!cookie.hasSecure);
    utassert(!cookie.hasSameSite);
    Vec<wry::Cookie> cookies;
    cookie.name = StrDup(StrL("session"));
    cookie.value = StrDup(StrL("value"));
    cookie.domain = StrDup(StrL("example.com"));
    cookie.path = StrDup(StrL("/"));
    cookies.Append(cookie);
    wry::CookieListFree(&cookies);
    utassert(cookies.len == 0);

    // The crate's own case, verbatim.
    Str scheme = StrL("http");
    Str uri = StrL("http://wry.localhost/path/to/page");
    utassert(wry::IsWorkAroundUri(uri, scheme, StrL("wry")));
    utassert(!wry::IsWorkAroundUri(uri, scheme, StrL("asset")));

    // The prefix the filter is built from.
    utassert(base::StrEq(wry::WorkAroundUriPrefix(scheme, StrL("wry")), "http://wry."));
    utassert(base::StrEq(wry::WorkAroundUriPrefix(StrL("https"), StrL("asset")), "https://asset."));

    // What the initial navigation does to a `wry://` url, and what the
    // request handler undoes before the handler sees it.
    Str original = StrL("wry://localhost/path/to/page");
    Str applied = wry::ApplyUriWorkAround(original, scheme, StrL("wry"));
    utassert(base::StrEq(applied, "http://wry.localhost/path/to/page"));
    utassert(base::StrEq(wry::RevertUriWorkAround(applied, scheme, StrL("wry")), original.s));

    // A URI in another protocol is left where it is, both ways round.
    Str other = StrL("https://example.com/x");
    utassert(base::StrEq(wry::ApplyUriWorkAround(other, scheme, StrL("wry")), other.s));
    utassert(base::StrEq(wry::RevertUriWorkAround(other, scheme, StrL("wry")), other.s));

    // The https variant is a different prefix, so an http-tunnelled URI is
    // not one of its.
    utassert(!wry::IsWorkAroundUri(uri, StrL("https"), StrL("wry")));
}
