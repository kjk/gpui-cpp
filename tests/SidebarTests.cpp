/* Ported from crates/ui/src/sidebar/mod.rs.
 *
 * `SidebarLayout::new` is what a collapsible mode and a collapsed flag come
 * to: which rendering the rows take, what the wrapper does with its width, and
 * which end the content is pinned to. Rust's own tests are the five cases
 * below. */

#include "Test.h"

using namespace gpui::component;

static SidebarLayout Layout(SidebarCollapsible collapsible, bool collapsed,
                            float expandedWidth, Side side) {
    return SidebarLayoutFor(collapsible, collapsed, expandedWidth, side);
}

static void IconCollapsedUsesTheIconWidth() {
    SidebarLayout l = Layout(SidebarCollapsible::Icon, true, 240, Side::Left);
    utassert(l.iconCollapsed);
    utassert(!l.offcanvasCollapsed);
    utassert(!l.alignChildToEnd);
    utassert(l.wrapper == SidebarWrapperKind::Animated);
    utassertnear(l.wrapperWidth, kSidebarCollapsedWidth);
}

static void IconExpandedUsesTheExpandedWidth() {
    SidebarLayout l = Layout(SidebarCollapsible::Icon, false, 240, Side::Left);
    utassert(!l.iconCollapsed);
    utassert(!l.offcanvasCollapsed);
    utassert(l.wrapper == SidebarWrapperKind::Animated);
    utassertnear(l.wrapperWidth, 240.f);
}

static void AWidthThatIsNotInPixelsLeavesTheWrapperAlone() {
    // Rust's `None` width: the sidebar sizes itself and the wrapper stays out
    // of the way.
    SidebarLayout l = Layout(SidebarCollapsible::Icon, false, 0, Side::Left);
    utassert(!l.iconCollapsed);
    utassert(!l.offcanvasCollapsed);
    utassert(l.wrapper == SidebarWrapperKind::None);
}

static void NoneIgnoresTheCollapsedFlag() {
    SidebarLayout l = Layout(SidebarCollapsible::None, true, 240, Side::Right);
    utassert(!l.iconCollapsed);
    utassert(!l.offcanvasCollapsed);
    // On the right, the content is pinned to the far end.
    utassert(l.alignChildToEnd);
    utassert(l.wrapper == SidebarWrapperKind::None);
}

static void OffcanvasCollapsesToNothing() {
    SidebarLayout l =
        Layout(SidebarCollapsible::Offcanvas, true, 240, Side::Left);
    utassert(!l.iconCollapsed);
    utassert(l.offcanvasCollapsed);
    // Offcanvas flips which end the content is pinned to: on the left, it
    // holds the right edge as the width goes to nothing.
    utassert(l.alignChildToEnd);
    utassert(l.wrapper == SidebarWrapperKind::Animated);
    utassertnear(l.wrapperWidth, 0.f);

    SidebarLayout open =
        Layout(SidebarCollapsible::Offcanvas, false, 240, Side::Left);
    utassert(!open.offcanvasCollapsed);
    utassert(open.wrapper == SidebarWrapperKind::Animated);
    utassertnear(open.wrapperWidth, 240.f);

    // With no width of its own there is nothing to animate, so a collapsed
    // one is simply zero wide.
    SidebarLayout noWidth =
        Layout(SidebarCollapsible::Offcanvas, true, 0, Side::Left);
    utassert(noWidth.wrapper == SidebarWrapperKind::Static);
    utassertnear(noWidth.wrapperWidth, 0.f);
    SidebarLayout noWidthOpen =
        Layout(SidebarCollapsible::Offcanvas, false, 0, Side::Left);
    utassert(noWidthOpen.wrapper == SidebarWrapperKind::None);
}

void TestSidebar() {
    TestSuite("sidebar");
    IconCollapsedUsesTheIconWidth();
    IconExpandedUsesTheExpandedWidth();
    AWidthThatIsNotInPixelsLeavesTheWrapperAlone();
    NoneIgnoresTheCollapsedFlag();
    OffcanvasCollapsesToNothing();
}
