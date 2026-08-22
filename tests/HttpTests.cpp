/* sys/http.h without a network: what counts as a URL this can fetch, and
   that the fetch table refuses everything else and starts nothing at all
   when fetching is turned off.

   Nothing here makes a request. A suite that reached the network would fail
   on a machine without one and would be slow on a machine with one, so
   TestsMain turns fetching off before any of it runs and the check below is
   that the switch is honoured. */

#include "Test.h"

static void HttpOnlyKnowsTwoSchemes() {
    utassert(HttpUrlIsRemote(StrL("http://example.com/a.png")));
    utassert(HttpUrlIsRemote(StrL("https://example.com/a.png")));
    // The scheme is not case sensitive; a host without a path is still a URL.
    utassert(HttpUrlIsRemote(StrL("HTTPS://EXAMPLE.COM")));
    utassert(!HttpUrlIsRemote(StrL("ftp://example.com/a.png")));
    utassert(!HttpUrlIsRemote(StrL("data:image/png;base64,iVBORw0KGgo=")));
    utassert(!HttpUrlIsRemote(StrL("icons/inbox.svg")));
    utassert(!HttpUrlIsRemote(StrL("")));
    utassert(!HttpUrlIsRemote(StrL("https:/")));
}

// A src the client cannot even attempt is a failure now, not something to
// wait for: image.cpp writes that answer down and stops asking.
static void AFetchOfSomethingLocalFailsAtOnce() {
    const uint8_t* bytes = (const uint8_t*)1;
    int len = 7;
    utassert(HttpFetch(StrL("icons/inbox.svg"), &bytes, &len) ==
             FetchState::Failed);
    utassert(bytes == nullptr);
    utassert(len == 0);
    utassert(HttpFetchPending() == 0);
}

static void FetchingOffStartsNothing() {
    utassert(!HttpEnabled());
    const uint8_t* bytes = nullptr;
    int len = 0;
    // None, not Pending: nothing was handed to a thread, and the caller is
    // free to ask again later.
    utassert(HttpFetch(StrL("https://example.com/a.png"), &bytes, &len) ==
             FetchState::None);
    utassert(HttpFetchPending() == 0);
}

void TestHttp() {
    TestSuite("http");
    HttpOnlyKnowsTwoSchemes();
    AFetchOfSomethingLocalFailsAtOnce();
    FetchingOffStartsNothing();
}
