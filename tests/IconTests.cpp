/* crates/ui/src/icon.rs has no unit tests upstream. These are seam tests for
 * the C++ representation of its IconNamed trait and RenderOnce behavior. */

#include "Test.h"

static void DefaultSizeAndColorAreInherited() {
    Arena* a = ArenaNew();
    Ctx cx = {};
    cx.a = a;

    El* icon = component::Icon::New(&cx, IconName::Check)->IntoEl();
    El* row = Div(a)->FlexRow()->Font(21)->Fg(Rgba{1, 2, 3, 255})->Child(icon);
    LayoutCache* cache = LayoutCacheNew();
    LayoutEl(nullptr, row, 0, 0, 200, 100, 14, Rgba{}, cache);

    utassertnear(icon->w, 21.f);
    utassertnear(icon->h, 21.f);
    utassert(icon->style.hasColor);
    utassert(icon->style.color.r == 1);
    utassert(icon->style.color.g == 2);
    utassert(icon->style.color.b == 3);

    LayoutCacheFree(cache);
    ArenaDelete(a);
}

static void NamedAndCustomPathsUseTheSameSvgElement() {
    Arena* a = ArenaNew();
    Ctx cx = {};
    cx.a = a;

    El* named = component::Icon::New(
                    &cx, component::IconNamed::From(IconName::Close))
                    ->IntoEl();
    utassert(base::StrEq(named->iconPath, "icons/close.svg"));

    El* custom = component::Icon::Empty(&cx)
                     ->Path(StrL("icons/application-logo.svg"))
                     ->Size(UiSize::Large)
                     ->Rotate(0.25f)
                     ->Color(Rgba{10, 20, 30, 255})
                     ->IntoEl();
    utassert(base::StrEq(custom->iconPath, "icons/application-logo.svg"));
    utassertnear(custom->style.width, 24.f);
    utassertnear(custom->style.height, 24.f);
    utassertnear(custom->style.rotate, 0.25f);
    utassert(custom->style.hasColor);
    utassert(custom->style.flexShrink == 0);

    ArenaDelete(a);
}

static void PinnedIconAdditionsHaveExactAssetPaths() {
    utassert(base::StrEq(IconNamePath(IconName::ALargeSmall),
                     "icons/a-large-small.svg"));
    utassert(base::StrEq(IconNamePath(IconName::BatteryWarning),
                     "icons/battery-warning.svg"));
    utassert(base::StrEq(IconNamePath(IconName::EllipsisVertical),
                     "icons/ellipsis-vertical.svg"));
    utassert(base::StrEq(IconNamePath(IconName::ResizeCorner),
                     "icons/resize-corner.svg"));
    utassert(base::StrEq(IconNamePath(IconName::SortAscending),
                     "icons/sort-ascending.svg"));
    utassert(base::StrEq(IconNamePath(IconName::Undo2), "icons/undo-2.svg"));
    // The old C++ spelling remains a compatibility alias, not a replacement
    // for the pinned Close variant.
    utassert(base::StrEq(IconNamePath(IconName::X), "icons/x.svg"));
}

void TestIcon() {
    TestSuite("icon");
    DefaultSizeAndColorAreInherited();
    NamedAndCustomPathsUseTheSameSvgElement();
    PinnedIconAdditionsHaveExactAssetPaths();
}
