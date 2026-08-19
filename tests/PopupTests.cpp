/* Ported from crates/base/src/popup.rs resolved_corner.
 *
 * Which point of the trigger the content is placed against. The bottom
 * anchors take the trigger's bottom edge, so content hanging below starts
 * where the trigger ends rather than where it began. */

#include "Test.h"

// A trigger at (100, 50), 80 wide and 20 tall.
static const Bounds kTrigger = {100, 50, 80, 20};

static void TheTopAnchorsTakeTheTopEdge() {
    Point p = PopupResolvedCorner(PopupAnchor::TopLeft, kTrigger);
    utassertnear(p.x, 100.f);
    utassertnear(p.y, 50.f);

    p = PopupResolvedCorner(PopupAnchor::TopCenter, kTrigger);
    utassertnear(p.x, 140.f);
    utassertnear(p.y, 50.f);

    p = PopupResolvedCorner(PopupAnchor::TopRight, kTrigger);
    utassertnear(p.x, 180.f);
    utassertnear(p.y, 50.f);
}

static void TheBottomAnchorsTakeTheBottomEdge() {
    Point p = PopupResolvedCorner(PopupAnchor::BottomLeft, kTrigger);
    utassertnear(p.x, 100.f);
    utassertnear(p.y, 70.f);

    p = PopupResolvedCorner(PopupAnchor::BottomCenter, kTrigger);
    utassertnear(p.x, 140.f);
    utassertnear(p.y, 70.f);

    p = PopupResolvedCorner(PopupAnchor::BottomRight, kTrigger);
    utassertnear(p.x, 180.f);
    utassertnear(p.y, 70.f);
}

static void TheSideAnchorsFallBackToTheOrigin() {
    // Rust hands back the origin for both: a popup anchored sideways is
    // placed by the positioner rather than by a corner.
    Point p = PopupResolvedCorner(PopupAnchor::LeftCenter, kTrigger);
    utassertnear(p.x, 100.f);
    utassertnear(p.y, 50.f);

    p = PopupResolvedCorner(PopupAnchor::RightCenter, kTrigger);
    utassertnear(p.x, 100.f);
    utassertnear(p.y, 50.f);
}

void TestPopup() {
    TestSuite("popup");
    TheTopAnchorsTakeTheTopEdge();
    TheBottomAnchorsTakeTheBottomEdge();
    TheSideAnchorsFallBackToTheOrigin();
}
