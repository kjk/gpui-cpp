/* Component traits — crates/base/src/component_traits.rs

   Rust uses traits to require selected, disabled, focus-ring and collapsed
   builder methods. C++ components expose those methods directly; this named
   module records that public contract and gives generic code the common
   state values without introducing inheritance or RTTI. */

#include "gpui/gpui.h"

namespace gpui {

struct ComponentStateFlags {
    bool selected = false;
    bool secondarySelected = false;
    bool disabled = false;
    bool focusRing = true;
    bool collapsed = false;
};

} // namespace gpui
