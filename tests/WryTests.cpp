/* wry/src/webview2/mod.rs's own test module: `checks_if_custom_protocol_uri`,
 * over the URI work-around a custom protocol goes through on Windows.
 *
 * The two `replace` helpers beside it carry no test upstream; the round trip
 * is asserted here because they are what the request handler and the initial
 * navigation each use one half of, and a mismatch between them would only
 * show up as a page that never loads. */

#include "Test.h"

static bool Same(Str got, const char* want) {
    return StrEq(got, Str(want));
}

void TestWryUri() {
    TestSuite("wry_uri");

    // The crate's own case, verbatim.
    Str scheme = StrL("http");
    Str uri = StrL("http://wry.localhost/path/to/page");
    utassert(wry::IsWorkAroundUri(uri, scheme, StrL("wry")));
    utassert(!wry::IsWorkAroundUri(uri, scheme, StrL("asset")));

    // The prefix the filter is built from.
    utassert(Same(wry::WorkAroundUriPrefix(scheme, StrL("wry")), "http://wry."));
    utassert(Same(wry::WorkAroundUriPrefix(StrL("https"), StrL("asset")), "https://asset."));

    // What the initial navigation does to a `wry://` url, and what the
    // request handler undoes before the handler sees it.
    Str original = StrL("wry://localhost/path/to/page");
    Str applied = wry::ApplyUriWorkAround(original, scheme, StrL("wry"));
    utassert(Same(applied, "http://wry.localhost/path/to/page"));
    utassert(Same(wry::RevertUriWorkAround(applied, scheme, StrL("wry")), original.s));

    // A URI in another protocol is left where it is, both ways round.
    Str other = StrL("https://example.com/x");
    utassert(Same(wry::ApplyUriWorkAround(other, scheme, StrL("wry")), other.s));
    utassert(Same(wry::RevertUriWorkAround(other, scheme, StrL("wry")), other.s));

    // The https variant is a different prefix, so an http-tunnelled URI is
    // not one of its.
    utassert(!wry::IsWorkAroundUri(uri, StrL("https"), StrL("wry")));
}
