#ifndef GPUI_GPUI_IMAGE_H_
#define GPUI_GPUI_IMAGE_H_
/* Where an image element's `src` turns into pixels.

   GPUI resolves `img(source)` through its asset system: a path goes to the
   AssetSource, a URL to the http client, and the bytes are decoded by the
   `image` crate and cached in the window's image cache. Here the decode is
   the platform's (paint.h RenderImageDecode) and this is the rest of it — what
   a src may name, and one cache so a document with the same image twice decodes
   it once.

   What a src may name:
     - an asset path, resolved through gpui/assets.h the way an icon is
     - a `data:` URI, base64 or percent-encoded
     - an http(s) URL, fetched asynchronously by sys/http.h

   Fetching is asynchronous; desktop decode and local file reads run on the
   main thread. A fetch that has not landed yet answers null, the
   element measures and paints its alt text for those frames, and the window
   keeps repainting while `HttpFetchPending` is non-zero — so the picture
   appears when it arrives rather than the frame freezing until it does. */

#include "gpui/gpui.h"

namespace gpui {

// paint.h owns it — one decoded bitmap, in whatever shape the backend keeps.
struct RenderImage;

// The decoded image for `src`, or null when there is nothing to draw yet: a
// fetch still running, a missing asset, a vector picture (see below), or a
// format this platform does not decode. The result is owned by the cache; do
// not release it. Retain it explicitly if it must survive cache eviction.
RenderImage* ImageForSrc(PaintApp* pa, Str src);

// The draw-ops for a src that is a vector picture rather than a bitmap — a
// local or shipped `.svg`, or one fetched from the network. None of the three
// backends decodes SVG, so this is the icon renderer's byte stream instead
// and `SvgDrawOps` paints it. Null when the src is not one. The bytes belong
// to the cache.
const uint8_t* ImageVectorForSrc(Str src, int* lenOut);

// Whether `src` names something on this machine — an asset path or a data:
// URI. An http(s) URL answers false.
bool ImageSrcIsLocal(Str src);

// The asset a local `src` names, when the application ships one. A URI is a
// network resource and answers empty, even if an asset has the same basename.
Str ImageAssetFor(Arena* a, Str src);

// Drop every decoded image and every fetched body. AppFree calls it; a test
// may too.
void ImageCacheClear();

} // namespace gpui
#endif // GPUI_GPUI_IMAGE_H_
