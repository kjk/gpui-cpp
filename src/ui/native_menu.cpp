#include "ui/native_menu.h"

namespace gpui {

namespace component {

El* NativeMenu::New(Ctx* cx, PopupMenu* menu) {
    Arena* a = cx->a;
    return menu ? menu->IntoEl() : Div(a);
}

} // namespace component
} // namespace gpui
