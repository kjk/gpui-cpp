#include "component/NativeMenu.h"

namespace gpui {

namespace component {

El* NativeMenu::New(Arena* a, Menu* menu) {
    return menu ? menu->IntoEl() : Div(a);
}

} // namespace component
} // namespace gpui
