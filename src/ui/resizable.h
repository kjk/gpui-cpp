/* Themed resizable panels — crates/base/src/resizable

   Rust has no `ui/resizable.rs`: the state, the panels, the handle and the
   drag are all `crates/base`, because the only thing a theme has to say about
   a resizable group is what colour the hairline over each boundary is. So
   this is that, and nothing else — the group itself is `base/resizable.h`.
*/

#include "base/resizable.h"
#include "ui/sizing.h"

namespace gpui {

namespace component {

using ResizableState = gpui::ResizableState;

struct Resizable {
    // The base group with the theme's border on its handles. The chain that
    // follows — `W`, `Panel`, `Grow`, `Flex`, `Visible`, `IntoEl` — is the
    // base group's own.
    static gpui::Resizable* New(Ctx* cx, Str id, Entity<ResizableState> state,
                                Axis axis = Axis::Horizontal);
};

} // namespace component
} // namespace gpui
