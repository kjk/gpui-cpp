#ifndef GPUI_SYS_HTTP_H_
#define GPUI_SYS_HTTP_H_
/* One request, and the OS's own library to make it with.

   This is the whole of the network in this tree: send one http(s) request and
   read its answer. A method, request headers and a request body are carried,
   because `crates/shell`'s `fetch` carries them and an OAuth form exchange or
   an authenticated read is the ordinary shape of what a script does. What is
   still not here: cookies, a session, a connection kept alive, a socket, a
   TLS stack of ours. The image path follows redirects through the platform
   client; the shell path exposes each redirect so its capability policy can
   approve the target first.

   Each platform brings its own, so there is no library to vendor and no TLS
   stack to carry: WinHTTP on Windows, NSURLSession on macOS, libcurl on
   Linux, which is the same place X11, cairo and Pango come from.

   `HttpSend` and `HttpGet` block, so nothing on the UI thread may call them.
   `HttpSendAsync` adapts those clients through the executor and is the native
   shape of browser fetch(). `HttpFetch` uses the browser seam on wasm and the
   established worker path on hosted targets, so a paint asks every frame and
   never waits. */

#include "base.h"

namespace gpui {

// ─── the blocking client ──────────────────────────────────────────────────

struct HttpRsp {
    // The status line's code, or 0 when the request never got one — no DNS,
    // no route, no TLS.
    int status = 0;
    Vec<uint8_t> body;
    // What the server called it, lowercased and without its parameters
    // ("image/png"). Owned; empty when the server said nothing.
    Str contentType;
    // With HttpGetNoRedirect, the absolute target named by a 3xx Location
    // header. Owned and empty for every other response.
    Str redirectUrl;
};

void HttpRspFree(HttpRsp* r);

// One request header. Both halves are borrowed for the length of the call:
// the request is sent before `HttpSend` returns, so nothing here is copied.
struct HttpHeader {
    Str name;
    Str value;
};

// What to send. `method` empty means GET. `body` is sent as the request
// entity for any method that has one; an empty body sends none, so a GET
// carries no Content-Length it did not ask for.
//
// The caller has already decided that these headers may be sent — the
// platform layer sets no header of its own beyond what the client needs to
// make the connection, and rejects none.
struct HttpReq {
    Str url;
    Str method;
    const HttpHeader* headers = nullptr;
    int nHeaders = 0;
    Str body;
    // Answer a 3xx rather than following it, filling `redirectUrl` with the
    // absolute target. Shell uses this so its capability policy can approve a
    // Location before anything contacts it.
    bool noRedirect = false;
};

// Answers true when the transfer finished, whatever the status: `status` and
// `body` are then the server's. False means it never got that far.
//
// A response body over `kHttpMaxBody` is refused rather than truncated, and
// the whole thing gives up after `kHttpTimeoutMs`.
bool HttpSend(const HttpReq& req, HttpRsp* out);

// The two GETs the image path is written against, in terms of the above.
bool HttpGet(Str url, HttpRsp* out);
bool HttpGetNoRedirect(Str url, HttpRsp* out);

// Big enough for any picture a document sensibly holds, small enough that a
// URL pointing at a disk image cannot take the process down with it.
constexpr int kHttpMaxBody = 16 * 1024 * 1024;
constexpr int kHttpTimeoutMs = 15000;

// Whether `url` is one this can even try: http:// or https://, and nothing
// else. Every other scheme — data:, file:, ftp: — belongs to someone else.
bool HttpUrlIsRemote(Str url);

// ─── one request without waiting ─────────────────────────────────────────

struct HttpAsyncResult {
    bool ok = false;
    // Borrowed for the callback. A receiver that needs the fields afterwards
    // moves or copies them before returning.
    HttpRsp* response = nullptr;
};

// Copies `req` before returning and calls `done` on the main thread after the
// response lands. Hosted targets run HttpSend on the executor; wasm uses the
// browser's asynchronous fetch(). False means the request could not be copied
// or started and no callback will arrive.
bool HttpSendAsync(const HttpReq& req, Func1<HttpAsyncResult> done);

// ─── fetching without waiting ─────────────────────────────────────────────

enum class FetchState : uint8_t {
    // Nothing has been asked for this URL, or the table has forgotten it.
    None = 0,
    // An asynchronous transfer has it.
    Pending,
    // The bytes are here.
    Done,
    // It will not arrive: the transfer failed, or the status was not 2xx.
    Failed
};

// What the table holds for `url`, starting the fetch if it holds nothing.
// Pending on the first call and for as long as the transfer runs; Done with
// `*bytes` / `*len` pointing at the body, which belongs to the table and
// lives until the entry is evicted or the table is cleared.
//
// This is meant to be called from a paint: it never blocks, and the answer
// changing from Pending to Done is what a repaint is for.
FetchState HttpFetch(Str url, const uint8_t** bytes, int* len);

// How many fetches are running.
int HttpFetchPending();

// What to call on the main thread once a fetch has landed, whether it
// arrived or failed. The window installs a repaint here: a picture that was
// not ready when the frame was drawn is a reason to draw another one, and
// this is what says when. Without it a fetch still completes and the table
// still answers Done — nothing would think to look.
void HttpSetOnFetchDone(Func0 f);

// Forget every fetched body. On hosted targets this waits briefly for worker
// requests; a still-pending request owns its slot through process teardown.
// AppFree calls it. The wasm main loop does not return to AppFree.
void HttpFetchClear();

// Release the downloaded body for a specific URL once it has been decoded.
void HttpFetchDrop(Str url);

// Whether fetching is allowed at all. On by default; the tests turn it off so
// a suite never touches the network.
void HttpSetEnabled(bool on);
bool HttpEnabled();

} // namespace gpui
#endif // GPUI_SYS_HTTP_H_
