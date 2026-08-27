#ifndef GPUI_SRC_UI_STATUS_BAR_H_
#define GPUI_SRC_UI_STATUS_BAR_H_
/* Themed status bar — crates/ui/src/status_bar.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// Three regions, each holding any number of elements: `left` and `right` pin
// to their ends and the center takes what is left. Where the center lands
// follows which ends are pinned — centered with both, end-aligned with only a
// left, start-aligned otherwise, which is what makes a bar with only children
// read like a plain container.
struct StatusBar {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<El*> left;
    ArenaVec<El*> center;
    ArenaVec<El*> right;

    static StatusBar* New(Ctx* cx);
    StatusBar* Left(El* e);
    StatusBar* Left(Str s);
    // The center region is the bar's child; where it lands depends on which
    // sides are filled.
    StatusBar* Center(El* e);
    StatusBar* Center(Str s);
    StatusBar* Right(El* e);
    StatusBar* Right(Str s);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
#endif // GPUI_SRC_UI_STATUS_BAR_H_
