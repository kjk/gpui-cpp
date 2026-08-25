#include "Showcase.h"
#include "gpui.h"

using namespace gpui;

// `div().id(..).overflow_scroll().track_scroll(&self.example_scroll)` with
// `Scrollbar::new(&self.example_scroll).mode(Always)` hung off it. The offset
// is the view's, so the box reports where it should be and the page stores
// it — which is what `track_scroll` does with a handle.
static void OnExampleScroll(ShowcaseApp* app, Ctx* cx, const ScrollEvent* ev) {
    app->exampleScroll = ev->offsetY;
    Notify(cx);
}

El* ShowcaseScrollbar(ShowcaseApp* app, Ctx* cx) {
    Arena* a = cx->a;
    El* list = Div(a)->FlexCol();
    for (int i = 1; i <= 20; i++) {
        list->Child(Div(a)
                        ->H(28)
                        ->PadX(8)
                        ->ItemsCenter()
                        ->JustifyBetween()
                        ->BorderB(1, Rgb(0xe5, 0xe7, 0xeb))
                        ->Child(TextEl(a, DupFmt(cx, "Activity %d", i))
                                    ->Font(12)
                                    ->Fg(ScInk()))
                        ->Child(TextEl(a, i % 3 == 0 ? StrL("Completed")
                                                     : StrL("Pending"))
                                    ->Font(12)
                                    ->Fg(ScInk())));
    }
    // The thumb, the track press and the drag all belong to the scrolled box,
    // so nothing here works out where the thumb goes. The page used to, and
    // what it drew could not be dragged.
    return Scrollbar::Vertical(cx, StrL("example-scroll-region"),
                               app->exampleScroll, Listen(cx, &OnExampleScroll))
        ->W(288)
        ->H(192)
        ->Border(1, ScInk())
        ->Child(list);
}

SHOWCASE_PAGE(CompScrollbar, ShowcaseScrollbar);
