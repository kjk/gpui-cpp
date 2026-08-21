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
    // content() in Rust can replace the title/body/footer composition with a
    // complete DialogContent tree. Surface is that tree; the Dialog still
    // owns the modal host, panel, overlay, and close button.
    El* surface = nullptr;
    // onClose is the x and the backdrop; onCancel is the Cancel button, which
    // Rust lets refuse to close on its own (on_cancel returning false).
    Listener onClose;
    Listener onCancel;
    Listener onOk;

    // AlertDialog::w.
    float width = 448;
    float height = 0;
    // DialogProps::overlay. The alert story's dialogs never tint the page.
    bool overlay = true;
    bool overlayClosable = true;
    // close_on_escape. Rust hangs the key context off it, so turning it off
    // takes Enter with it: the two bindings live in the one context, and a
    // dialog that does not declare it has neither.
    bool keyboard = true;
    // Root assigns one layer index per active dialog. Each successive layer
    // sits 16px lower and owns a distinct focus trap.
    int layerIx = 0;
    float radius = 0;
    Background background = {};
    Rgba foreground = {};
    bool hasBackground = false;
    bool hasForeground = false;
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
    // DialogButtonProps::default(): no Cancel unless something asks. That is
    // what AlertDialog::confirm() does.
    bool showCancel = false;
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
    Dialog* Surface(El* e);
    Dialog* W(float px);
    Dialog* H(float px);
    Dialog* Overlay(bool v);
    Dialog* OverlayClosable(bool v);
    Dialog* Keyboard(bool v);
    Dialog* Layer(int ix);
    Dialog* Radius(float px);
    Dialog* Bg(Background color);
    Dialog* Fg(Rgba color);
    Dialog* Icon(IconName n, Rgba color, float size = 16);
    Dialog* HeaderCentered(bool v = true);
    Dialog* OkText(Str s);
    Dialog* CancelText(Str s);
    Dialog* OkVariant(ButtonVariant v, bool outline = false);
    Dialog* ShowCancel(bool v);
    // AlertDialog::confirm(): the standard OK / Cancel pair.
    Dialog* Confirm();
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
