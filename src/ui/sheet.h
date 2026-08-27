#ifndef GPUI_SRC_UI_SHEET_H_
#define GPUI_SRC_UI_SHEET_H_
/* Themed sheet — crates/ui/src/sheet.rs */

#include "ui/sizing.h"
#include "ui/sheet_settings.h"

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
    El* titleEl = nullptr;
    bool open = false;
    // 350px along the placement axis, as in crates/ui/src/sheet.rs.
    float size = 350;
    SheetPlacement placement = SheetPlacement::Right;
    // Retained because it is a public Rust builder even though the pinned
    // renderer does not currently read the field either.
    bool resizable = true;
    bool overlay = true;
    bool overlayClosable = true;
    El* body = nullptr;
    ArenaVec<El*> children;
    // footer(): a row under the body, ruled off from it.
    El* footer = nullptr;
    // overflow_y_scrollbar() on the body: the offset is the caller's, the way
    // any scrolling box's is here.
    float scrollY = 0;
    int scrollId = 0;
    Listener onScroll;
    Listener onClose;
    Style style = {};
    uint32_t styleSet = 0;

    static Sheet* New(Ctx* cx);
    Sheet* Title(Str s);
    Sheet* Title(El* e);
    Sheet* Placement(SheetPlacement p);
    Sheet* Size(float px);
    Sheet* Resizable(bool v = true);
    Sheet* Overlay(bool v);
    Sheet* OverlayClosable(bool v = true);
    Sheet* Open(bool v);
    Sheet* Body(El* e);
    Sheet* Child(El* e);
    Sheet* Footer(El* e);
    Sheet* Refine(const Style& style, uint32_t fields);
    Sheet* Scroll(int id, float y, Listener fn);
    Sheet* OnClose(Listener fn);
    El* IntoEl(WinSize size);
};

} // namespace component
} // namespace gpui
#endif // GPUI_SRC_UI_SHEET_H_
