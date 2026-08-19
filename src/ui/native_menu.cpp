#include "ui/native_menu.h"

namespace gpui {

namespace component {

El* NativeMenu::New(Ctx* cx, Menu* menu) {
    Arena* a = cx->a;
    return menu ? menu->IntoEl() : Div(a);
}

} // namespace component
} // namespace gpui
