#include "component/NativeMenu.h"

namespace component {

El* NativeMenu::New(Arena* a, Menu* menu) {
    return menu ? menu->IntoEl() : Div(a);
}

} // namespace component
