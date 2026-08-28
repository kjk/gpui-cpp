#ifndef GPUI_SYS_HTTP_H_
#define GPUI_SYS_HTTP_H_
/* A GET, and the OS's own library to make it with.

   This is the whole of the network in this tree: fetch the bytes at an
   http(s) URL, and nothing else — no POST, no cookies, no connection kept.
   The image path follows redirects through the platform client; the shell
   path exposes each redirect so its capability policy can approve the target
   first. That is what a remote image and a guarded fetch need, and a bigger
   client would need a bigger reason.

   Each platform brings its own, so there is no library to vendor and no TLS
   stack to carry: WinHTTP on Windows, NSURLSession on macOS, libcurl on
   Linux, which is the same place X11, cairo and Pango come from.

   `HttpGet` blocks, so nothing on the UI thread may call it. `HttpFetch` is
   the half that does: it hands the URL to a worker thread and answers what
   the cache holds now, so a paint asks every frame and never waits. */

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

// Answers true when the transfer finished, whatever the status: `status` and
// `body` are then the server's. False means it never got that far.
//
// A body over `kHttpMaxBody` is refused rather than truncated, and the whole
// thing gives up after `kHttpTimeoutMs`.
bool HttpGet(Str url, HttpRsp* out);

// One GET without automatic redirects. Shell uses this narrower seam so it
// can capability-check a Location target before contacting it. HttpGet keeps
// the platform client's ordinary redirect behavior for remote images.
bool HttpGetNoRedirect(Str url, HttpRsp* out);

// Big enough for any picture a document sensibly holds, small enough that a
// URL pointing at a disk image cannot take the process down with it.
constexpr int kHttpMaxBody = 16 * 1024 * 1024;
constexpr int kHttpTimeoutMs = 15000;

// Whether `url` is one this can even try: http:// or https://, and nothing
// else. Every other scheme — data:, file:, ftp: — belongs to someone else.
bool HttpUrlIsRemote(Str url);

// ─── fetching without waiting ─────────────────────────────────────────────

enum class FetchState : int32_t {
    // Nothing has been asked for this URL, or the table has forgotten it.
    None = 0,
    // A worker thread has it.
    Pending,
    // The bytes are here.
    Done,
    // It will not arrive: the transfer failed, or the status was not 2xx.
    Failed
};

// What the table holds for `url`, starting the fetch if it holds nothing.
// Pending on the first call and for as long as the thread runs; Done with
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

// Forget every fetched body, waiting for the workers to finish first.
// AppFree calls it.
void HttpFetchClear();

// Whether fetching is allowed at all. On by default; the tests turn it off so
// a suite never touches the network.
void HttpSetEnabled(bool on);
bool HttpEnabled();

} // namespace gpui
#endif // GPUI_SYS_HTTP_H_
