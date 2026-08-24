/* wry/src/wkwebview/mod.rs — not ported yet. This is the seam, answering the
 * portable API with "there is no webview here".
 *
 * Part of the C++ port of lb-wry 0.53.3 (see src/wry/readme.md).
 *
 * The macOS backend is the one that could be written next: WKWebView is in
 * the system SDK, so it takes no dependency this tree has ruled out, and the
 * shape of the work is the eight delegate classes under `wkwebview/class`
 * plus the `WKURLSchemeHandler` half of the custom protocols. It is not here
 * because
 * nothing on this machine can build or run it, and an unverified
 * Objective-C backend is worth less than an honest stub.
 */
#include "wry/wry.h"

namespace wry {

using base::logf;
using base::Str;

static void Unsupported() {
    static bool said = false;
    if (!said) {
        said = true;
        logf("wry: no webview backend on this platform\n");
    }
}

WebView* WebViewNew(void*, const WebViewAttributes*, bool) {
    Unsupported();
    return nullptr;
}
void WebViewFree(WebView*) {}
Str WebViewId(WebView*) {
    return {};
}
bool WebViewEval(WebView*, Str) {
    return false;
}
bool WebViewEvalWithCallback(WebView*, Str, void*, void (*)(void*, Str)) {
    return false;
}
Str WebViewUrlTemp(WebView*) {
    return {};
}
bool WebViewLoadUrl(WebView*, Str) {
    return false;
}
bool WebViewLoadUrlWithHeaders(WebView*, Str, const Header*, int) {
    return false;
}
bool WebViewLoadHtml(WebView*, Str) {
    return false;
}
bool WebViewReload(WebView*) {
    return false;
}
bool WebViewBounds(WebView*, Rect*) {
    return false;
}
bool WebViewSetBounds(WebView*, Rect) {
    return false;
}
bool WebViewSetVisible(WebView*, bool) {
    return false;
}
bool WebViewFocus(WebView*) {
    return false;
}
bool WebViewFocusParent(WebView*) {
    return false;
}
bool WebViewZoom(WebView*, double) {
    return false;
}
bool WebViewSetBackgroundColor(WebView*, Rgba) {
    return false;
}
bool WebViewSetTheme(WebView*, Theme) {
    return false;
}
bool WebViewSetMemoryUsageLevel(WebView*, MemoryUsageLevel) {
    return false;
}
bool WebViewReparent(WebView*, void*) {
    return false;
}
bool WebViewPrint(WebView*) {
    return false;
}
bool WebViewClearAllBrowsingData(WebView*) {
    return false;
}
void WebViewOpenDevtools(WebView*) {}
void WebViewCloseDevtools(WebView*) {}
bool WebViewIsDevtoolsOpen(WebView*) {
    return false;
}
void Respond(RequestResponder*, const Response*) {}

Str WebViewVersionTemp() {
    return {};
}
bool WebViewAvailable() {
    return false;
}

}  // namespace wry
