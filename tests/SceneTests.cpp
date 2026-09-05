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

static void TextLayoutsHaveStableGenerations() {
    TestSuite("scene resource generations");
    PaintApp* app = PaintAppNew();
    utassert(app);
    if (!app) {
        return;
    }
    PaintCtx paint = {};
    paint.pa = app;
    Size size = {};
    TextLayout* first =
        TextLayoutNew(&paint, StrL("same"), 14, 0, false, 0, 0, &size);
    TextLayout* second =
        TextLayoutNew(&paint, StrL("same"), 14, 0, false, 0, 0, &size);
    utassert(first && second);
    if (first && second) {
        uint64_t firstGeneration = TextLayoutGeneration(first);
        utassert(firstGeneration != 0);
        utassert(TextLayoutGeneration(second) != 0);
        utassert(TextLayoutGeneration(second) != firstGeneration);
        TextLayoutAddRef(first);
        TextLayoutRelease(first);
        utassert(TextLayoutGeneration(first) == firstGeneration);
    }
    TextLayoutRelease(first);
    TextLayoutRelease(second);
    PaintAppFree(app);
}

static bool RecordTriangle(PaintCtx* paint, float x, float y) {
    paint->viewW = 320;
    paint->viewH = 200;
    scene::FrameBegin(paint);
    Path* path = scene::RecPathNew(paint, true);
    scene::RecPathMoveTo(path, x, y);
    scene::RecPathLineTo(path, x + 20, y);
    scene::RecPathLineTo(path, x + 10, y + 20);
    scene::RecPathClose(path);
    scene::RecPathFill(paint, path, Rgba8(0, 0, 0, 255));
    Bounds damage = {};
    return scene::FrameEnd(paint, &damage);
}

static void PathPlacementRemainsPartOfTheFrameHash() {
    TestSuite("scene translated path placement");
    PaintCtx paint = {};

    utassert(RecordTriangle(&paint, 10, 20));
    // The cache may share these two paths' relative geometry, but the frame
    // diff must still see that the primitive moved.
    utassert(RecordTriangle(&paint, 30, 40));
    utassert(!RecordTriangle(&paint, 30, 40));
    utassert(scene::Stats(&paint).pathPrims == 1);

    scene::Free(&paint);
}

// Rust's Arc<RenderImage> keeps decoded pixels alive independently of the
// loading cache. Exercise that contract across cache eviction and two scenes.
static void RecordedImagesSurviveCacheEviction() {
#if !GPUI_OS_WASM
    TestSuite("scene image ownership");
    App* owner = AppNew();
    PaintApp* app = owner ? owner->paint : nullptr;
    utassert(app);
    if (!app) return;
    PaintCtx first = {};
    PaintCtx second = {};
    first.pa = second.pa = app;
    first.viewW = second.viewW = 100;
    first.viewH = second.viewH = 100;
    const char* png =
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4z8DwHwAFAA"
        "H/iZk9HQAAAABJRU5ErkJggg==";
    RenderImage* image =
        ImageForSrc(app, fmt("data:image/png;base64,%s", Str(png)));
    utassert(image);
    if (image) {
        uint64_t generation = RenderImageGeneration(image);
        scene::FrameBegin(&first);
        scene::RecImageDraw(&first, image, Bounds{0, 0, 1, 1}, 0);
        for (int i = 0; i < 40; i++) {
            RenderImage* next = ImageForSrc(
                app, fmt("data:image/png;tag=%d;base64,%s", i, Str(png)));
            utassert(next);
            if (next)
                scene::RecImageDraw(&first, next, Bounds{(float)i, 0, 1, 1}, 0);
        }
        Bounds damage = {};
        scene::FrameEnd(&first, &damage);
        scene::FrameBegin(&second);
        scene::RecImageDraw(&second, image, Bounds{0, 0, 1, 1}, 0);
        scene::FrameEnd(&second, &damage);
        ImageCacheClear();
        utassert(RenderImageGeneration(image) == generation);
        utassert(RenderImageSizePx(image).w == 1);
        scene::FrameBegin(&first); // Release only this scene's ownership.
        scene::FrameEnd(&first, &damage);
        utassert(RenderImageGeneration(image) == generation);
        utassert(RenderImageSizePx(image).h == 1);
    }
    scene::Free(&first);
    scene::Free(&second);
    ImageCacheClear();
    AppFree(owner);
#endif
}

void TestScene() {
    RecordedImagesSurviveCacheEviction();
    FrameComparisonBelongsToOnePaintContext();
    TextLayoutsHaveStableGenerations();
    PathPlacementRemainsPartOfTheFrameHash();
}
