/* Themed sheet — crates/ui/src/sheet.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// Which edge the sheet slides in from; Rust defaults to the right.
enum class SheetPlacement : uint8_t {
    Left,
    Top,
    Right,
    Bottom
};

struct Sheet {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str title = {};
    bool open = false;
    // 350px along the placement axis, as in crates/ui/src/sheet.rs.
    float size = 350;
    SheetPlacement placement = SheetPlacement::Right;
    bool overlay = true;
    El* body = nullptr;
    Listener onClose;

    static Sheet* New(Ctx* cx);
    Sheet* Title(Str s);
    Sheet* Placement(SheetPlacement p);
    Sheet* Size(float px);
    Sheet* Overlay(bool v);
    Sheet* Open(bool v);
    Sheet* Body(El* e);
    Sheet* OnClose(Listener fn);
    El* IntoEl(WinSize size);
};

} // namespace component
} // namespace gpui
