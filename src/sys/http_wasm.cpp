/* The browser has the best HTTP client of the four platforms here and this
   cannot use it, because the shapes do not meet: `HttpGet` blocks and answers
   the bytes, and everything a page can reach — fetch(), XMLHttpRequest,
   emscripten_fetch — answers a promise instead. The one synchronous form left
   is a synchronous XMLHttpRequest on the main thread, which freezes the tab
   for the length of the transfer and is being removed from the engines; the
   other is emscripten_fetch in synchronous mode, which needs a worker thread,
   and a wasm page here has none (see the note on PlatThreadRun in
   sys/executor.cpp).

   So this answers false, the way the Linux build does when it was compiled
   without libcurl: a remote image renders as its alt text and nothing else
   changes. Assets preloaded into the page and `data:` URLs go nowhere near
   here — gpui/image.h reads those directly — which is every picture the
   examples in this tree actually draw.

   What it would take to do properly: a seam in sys/http.cpp between "start a
   transfer" and "a transfer landed", so a platform that can only be
   asynchronous fills the slot from a callback instead of from a worker. The
   table in that file is already written for exactly that lifecycle. */

#include "sys/http.h"

namespace gpui {

bool HttpGet(Str url, HttpRsp* out) {
    (void)url;
    (void)out;
    return false;
}

} // namespace gpui
