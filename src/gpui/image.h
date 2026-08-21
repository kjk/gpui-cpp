/* Where an image element's `src` turns into pixels.

   GPUI resolves `img(source)` through its asset system: a path goes to the
   AssetSource, a URL to the http client, and the bytes are decoded by the
   `image` crate and cached in the window's image cache. Here the decode is
   the platform's (paint.h ImageDecode) and this is the rest of it — what a
   src may name, and one cache so a document with the same image twice
   decodes it once.

   What a src may name:
     - an asset path, resolved through gpui/assets.h the way an icon is
     - a `data:` URI, base64 or percent-encoded

   What it may not: an http(s) URL. Fetching one needs a socket and a TLS
   stack, neither of which this tree has, so a remote image renders as its
   alt text — which is what a document written for the web mostly holds, and
   why TextView keeps the alt text beside the source. */

#include "gpui/gpui.h"

namespace gpui {

// paint.h owns it — one decoded bitmap, in whatever shape the backend keeps.
struct Image;

// The decoded image for `src`, or null when there is nothing to draw: a
// remote URL, a missing asset, or a format this platform does not decode.
// The result is owned by the cache; do not free it.
Image* ImageForSrc(PaintApp* pa, Str src);

// Whether `src` is one this tree can even try — an asset path or a data:
// URI. A remote URL answers false without touching the cache.
bool ImageSrcIsLocal(Str src);

// The asset a `src` names, when the application ships one. A local path is
// itself; a remote URL is its last path segment looked for in the asset roots,
// which is what lets a document written for the web show the picture an
// application bundled beside it rather than its alt text. Answers an empty Str
// when nothing local matches.
Str ImageAssetFor(Arena* a, Str src);

// Drop every decoded image. AppFree calls it; a test may too.
void ImageCacheClear();

} // namespace gpui
