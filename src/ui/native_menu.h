/* Native menu façade — crates/ui/src/native_menu
   Windows: application builds a Menu and can assign it as a context menu. */

#include "ui/menu.h"

namespace gpui {

namespace component {

struct NativeMenu {
    static El* New(Ctx* cx, Menu* menu);
};

} // namespace component
} // namespace gpui
