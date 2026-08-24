/* crates/webview/src/lib.rs — see webview.h. */

#include "webview/webview.h"

#include "gpui/platform.h"

namespace gpui {

WebView::~WebView() {
    // `impl Drop for WebView` hides it; the Rc it held is what actually
    // closed the webview, and here that is this call.
    if (webview) {
        wry::WebViewFree(webview);
        webview = nullptr;
    }
}

void WebView::OnWindowMouseDown(WebView* self, Ctx* cx, const MouseDownEvent* ev) {
    (void)cx;
    if (!self->webview || !ev) {
        return;
    }
    Point p = {ev->x, ev->y};
    if (!self->bounds.Contains(p)) {
        wry::WebViewFocusParent(self->webview);
    }
}

Entity<WebView> WebViewNew(Ctx* cx, const wry::WebViewAttributes* attrs) {
    // `EntityNewState`, not `EntityNew`: a `WebView` has no Render of its
    // own here, since `WebViewEl` hands the element to whoever wants it.
    Entity<WebView> handle = EntityNewState<WebView>(cx->app);
    WebView* self = handle.Get(cx->app);
    if (!self) {
        return handle;
    }
    void* window = PlatWindowHandle(cx->win);
    if (!window) {
        logf("webview: this window has no OS handle to parent a webview into\n");
        return handle;
    }
    wry::WebViewAttributes copy = *attrs;
    // `WebView::new` starts it at nothing and lets the element place it.
    copy.bounds = wry::Rect{wry::LogicalPosition(0, 0), wry::LogicalSize(0, 0)};
    self->webview = wry::WebViewNew(window, &copy, /*asChild=*/true);
    self->visible = self->webview != nullptr;
    return handle;
}

void WebViewShow(WebView* self) {
    if (!self || !self->webview) {
        return;
    }
    wry::WebViewSetVisible(self->webview, true);
    self->visible = true;
}

void WebViewHide(WebView* self) {
    if (!self || !self->webview) {
        return;
    }
    wry::WebViewFocusParent(self->webview);
    wry::WebViewSetVisible(self->webview, false);
    self->visible = false;
}

bool WebViewVisible(const WebView* self) {
    return self && self->visible;
}

Bounds WebViewBounds(const WebView* self) {
    return self ? self->bounds : Bounds{};
}

void WebViewLoadUrl(WebView* self, Str url) {
    if (self && self->webview) {
        wry::WebViewLoadUrl(self->webview, url);
    }
}

void WebViewBack(WebView* self) {
    if (self && self->webview) {
        wry::WebViewEval(self->webview, StrL("history.back();"));
    }
}

wry::WebView* WebViewRaw(WebView* self) {
    return self ? self->webview : nullptr;
}

// The element's prepaint: layout has decided where the box is, so the OS
// control is moved there. Rust does this in `Element::prepaint`, which runs
// before the frame is drawn; the paint pass is where an element here first
// knows its box, and the two are a frame apart only if something else in the
// same frame reads the webview's bounds.
static void PaintWebView(PaintCtx* ctx, El* e, void* user) {
    (void)ctx;
    WebView* self = (WebView*)user;
    Bounds b = e->Bounds();
    self->bounds = b;
    if (!self->webview || !self->visible) {
        return;
    }
    bool same = self->hasApplied && self->applied.x == b.x && self->applied.y == b.y &&
                self->applied.w == b.w && self->applied.h == b.h;
    if (same) {
        return;
    }
    self->applied = b;
    self->hasApplied = true;
    wry::Rect r;
    r.position = wry::LogicalPosition(b.x, b.y);
    r.size = wry::LogicalSize(b.w, b.h);
    wry::WebViewSetBounds(self->webview, r);
}

El* WebViewEl(Entity<WebView> view, Ctx* cx) {
    WebView* self = view.Get(cx->app);
    // `Style { size: Size::full(), flex_shrink: 1., ..Default }`.
    El* e = Div(cx->a)->SizeFull();
    if (!self) {
        return e;
    }
    if (!self->subscribed) {
        self->subscribed = true;
        WindowOnMouseDown(cx->win, ListenTo(view, &WebView::OnWindowMouseDown));
    }
    e->customPaint = PaintWebView;
    e->customUser = self;
    return e;
}

}  // namespace gpui
