/* Themed Root — crates/ui/src/root.rs

   Root is the window's outermost view: the page, and over it the layers the
   window owns — the notifications, the sheet, and the stack of dialogs. Rust
   holds each of those as an entity on the Root and renders it from there; the
   layers here are the caller's elements, and what Root carries is the rules
   about them: which dialog's overlay shows, and how far the notifications are
   pushed in by an open sheet. */

#include "ui/sizing.h"
#include "ui/sheet.h"
#include "ui/window_border.h"

namespace gpui {

namespace component {

// Which dialog shows the overlay: the last one that asked for one, so a stack
// of dialogs tints the page once, under the topmost of them. -1 when none of
// them wants one.
int RootDialogOverlayIndex(const bool* wantsOverlay, int n);

// render_notification_layer: the notifications fill the window, less the room
// an open sheet takes on its own edge — so a sheet on the right pushes them
// left rather than covering them.
Edges RootNotificationInsets(bool hasSheet, SheetPlacement placement,
                             float size);

struct Root {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    El* child = nullptr;
    bool bordered = true;
    // window_shadow_size: the padding a client-decorated window keeps around
    // its frame.
    float shadowSize = kWindowShadowSize;

    // The layers, in the order they draw over the page.
    El* notifications = nullptr;
    El* sheet = nullptr;
    bool hasSheet = false;
    SheetPlacement sheetPlacement = SheetPlacement::Right;
    float sheetSize = 0;
    // As many as the caller opens, which is Rust's Vec. They grow into the
    // frame arena the builder is on.
    ArenaVec<El*> dialogs;
    ArenaVec<bool> dialogOverlay;

    static Root* New(Ctx* cx);
    Root* Bordered(bool v);
    Root* ShadowSize(float v);
    Root* Child(El* e);
    // The notification list, which the sheet pushes in.
    Root* Notifications(El* e);
    // The one open sheet, and where it sits.
    Root* Sheet(El* e, SheetPlacement placement, float size);
    // open_dialog: one call per dialog, in the order they were opened.
    // `overlay` is the dialog's own has_overlay.
    Root* Dialog(El* e, bool overlay = true);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
