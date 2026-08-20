/* Themed dock — crates/ui/src/dock

   DockArea renders a DockState: the centre item, the three Docks around it,
   and for every tab group a TabBar whose tabs can be dragged into another
   group or onto its edge to split it. */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// InvalidPanel::render: what stands in for a panel a saved layout names and
// the registry cannot build. Hand it to DockLoad; it is given the
// DockInvalidPanel holding the name that was asked for.
El* DockInvalidPanelRender(Ctx* cx, void* data);

// The tab bar of a tab group (TabPanel::render_title_bar).
const float kDockTabBarH = 30;
// resize_handle: the grab between two panels, and along a Dock's inner edge.
const float kDockHandleW = 4;

struct DockArea {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str id = {};
    Entity<DockState> state = {};

    static DockArea* New(Ctx* cx, Str id, Entity<DockState> state);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
