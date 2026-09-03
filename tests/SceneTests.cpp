/* The portable scene recorder's frame comparison. Upstream owns one Scene
 * per Window; these tests pin the equivalent ownership in this tree. */

#include "Test.h"

using namespace gpui;

static bool RecordClear(PaintCtx* paint, Rgba color) {
    paint->viewW = 320;
    paint->viewH = 200;
    scene::FrameBegin(paint);
    scene::RecClear(paint, color);
    Bounds damage = {};
    return scene::FrameEnd(paint, &damage);
}

static void FrameComparisonBelongsToOnePaintContext() {
    TestSuite("scene window ownership");
    PaintCtx first = {};
    PaintCtx second = {};

    Rgba black = Rgba8(0, 0, 0, 255);
    Rgba white = Rgba8(255, 255, 255, 255);
    utassert(RecordClear(&first, black));
    utassert(!RecordClear(&first, black));

    // This is the second window's first frame even though it has the same
    // dimensions and pixels as the first window's retained frame.
    utassert(RecordClear(&second, black));
    utassert(!RecordClear(&second, black));

    utassert(RecordClear(&first, white));
    utassert(!RecordClear(&second, black));
    utassert(scene::Stats(&first).frames == 3);
    utassert(scene::Stats(&second).frames == 3);

    scene::Free(&first);
    scene::Free(&second);
}

void TestScene() {
    FrameComparisonBelongsToOnePaintContext();
}
