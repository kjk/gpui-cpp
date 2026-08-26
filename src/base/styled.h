/* StyledExt — crates/base/src/styled.rs

   Fluent style methods are El methods here. These two constructors are the
   public h_flex/v_flex helpers Rust re-exports. */

#include "gpui/gpui.h"

namespace gpui {

inline El* HFlex(Arena* a) {
    return Div(a)->FlexRow()->ItemsCenter();
}

inline El* VFlex(Arena* a) {
    return Div(a)->FlexCol();
}

} // namespace gpui
