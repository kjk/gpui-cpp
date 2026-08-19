/* Themed dialog — crates/ui/src/dialog */

#include "ui/sizing.h"
#include "ui/button.h"

namespace gpui {

namespace component {

struct Dialog {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    Str title = {};
    Str description = {};
    bool open = false;
    El* body = nullptr;
    // onClose is the x and the backdrop; onCancel is the Cancel button, which
    // Rust lets refuse to close on its own (on_cancel returning false).
    Listener onClose;
    Listener onCancel;
    Listener onOk;

    // AlertDialog::w.
    float width = 448;
    // DialogProps::overlay. The alert story's dialogs never tint the page.
    bool overlay = true;
    // AlertDialog::icon sits inline before the title. A story that builds its
    // own DialogHeader can center the group instead and put a large glyph
    // above it.
    IconName icon = IconName::None;
    Rgba iconColor = {};
    bool hasIconColor = false;
    float iconSize = 16;
    bool headerCentered = false;

    // DialogButtonProps.
    Str okText = {};
    Str cancelText = {};
    ButtonVariant okVariant = ButtonVariant::Primary;
    bool okOutline = false;
    bool showCancel = true;
    // AlertDialog::close_button, the x in the corner.
    bool closeButton = false;

    // DialogFooter. `footer` replaces the action row outright; the flags
    // restyle the row the way the stories refine DialogFooter.
    El* footer = nullptr;
    bool footerVertical = false;
    // The buttons share the row (flex_1) rather than sitting at its end.
    bool footerStretch = false;
    bool footerMuted = false;
    bool footerDivider = false;

    static Dialog* New(Ctx* cx);
    Dialog* Title(Str s);
    Dialog* Description(Str s);
    Dialog* Open(bool v);
    Dialog* Body(El* e);
    Dialog* W(float px);
    Dialog* Overlay(bool v);
    Dialog* Icon(IconName n, Rgba color, float size = 16);
    Dialog* HeaderCentered(bool v = true);
    Dialog* OkText(Str s);
    Dialog* CancelText(Str s);
    Dialog* OkVariant(ButtonVariant v, bool outline = false);
    Dialog* ShowCancel(bool v);
    Dialog* CloseButton(bool v = true);
    Dialog* Footer(El* e);
    Dialog* FooterVertical(bool v = true);
    Dialog* FooterStretch(bool v = true);
    Dialog* FooterMuted(bool v = true);
    Dialog* FooterDivider(bool v = true);
    Dialog* OnClose(Listener fn);
    Dialog* OnCancel(Listener fn);
    Dialog* OnOk(Listener fn);
    El* IntoEl(WinSize size);

  private:
    El* Header();
    El* Actions();
};

} // namespace component
} // namespace gpui
