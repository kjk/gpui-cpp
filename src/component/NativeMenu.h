/* Native menu façade — crates/ui/src/native_menu
   Windows: application builds a Menu and can assign it as a context menu. */

#include "component/Menu.h"

namespace gpui {

namespace component {

struct NativeMenu {
    static El* New(Arena* a, Menu* menu);
};

} // namespace component
} // namespace gpui
