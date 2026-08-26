/* Interactive element extensions — crates/base/src/event.rs

   El owns the listeners directly in C++: OnClick/OnMouseDown/OnScroll and
   the click count on ClickEvent are the trait's implementation. */

#include "gpui/gpui.h"

namespace gpui {

inline bool IsDoubleClick(const ClickEvent* ev) {
    return ev && ev->clickCount == 2;
}

} // namespace gpui
