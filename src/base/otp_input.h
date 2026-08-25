/* Unstyled OTP input — crates/base/src/otp_input.rs */

#include "gpui/gpui.h"

namespace gpui {

// Rust's OtpState. A one-time-code field is not a text editor: there is no
// caret to move and no selection, only a run of digits that grows from the end
// and shrinks with backspace. The value is UTF-8 but every character in it is
// an ASCII digit, so `len` counts characters and bytes alike.
struct OtpState {
    char value[16] = {};
    int len = 0;
    int length = 6;
    bool masked = false;
    bool disabled = false;
    bool focused = false;
    // The caret's clock, the way InputState has one. Rust gives OtpState its
    // own Entity<BlinkCursor> for the same reason.
    EntityId blink = {};
    // otp_input.rs's `focus_handle`. The row is focusable *as* this, so
    // whether the field has focus is asked of the handle rather than by
    // hashing the element's name a second time and comparing.
    FocusHandle focus = {};
};

// to_digit_char: an ASCII digit, or a full-width one folded onto it. Answers 0
// for anything else, which is what Rust's Option<char> None means here.
char OtpDigitChar(uint32_t c);

// edit_value. `key` is a Key* code so backspace is recognisable, and `ch` is
// the character it produced, if any. Answers false where Rust answers None —
// a key that is not a digit, or a field already full — and the state is left
// untouched then.
bool OtpEditValue(OtpState* s, int key, uint32_t ch);

// on_focus / on_blur, which start and stop the caret's clock.
void OtpFocus(OtpState* s, App* app, Window* win);
void OtpBlur(OtpState* s, App* app, Window* win);
bool OtpCursorVisible(OtpState* s, App* app);

// The state's own key handler, which is what `El::OnKeyDown` hands the
// keystroke to: a digit or a backspace edits the value, and everything else
// propagates. Rust hangs `on_key_down` off the OtpInput element the same way.
void OtpKeyDown(OtpState* self, Ctx* cx, const KeyEvent* ev);
// The press that focuses the field, which is `on_mouse_down` in Rust.
void OtpClick(OtpState* self, Ctx* cx, const ClickEvent* ev);

struct OtpInput {
    static El* New(Ctx* cx, Str id = {});
    // The same, bound to a state: the element takes focus, hears the keys and
    // edits the value.
    static El* New(Ctx* cx, Str id, Entity<OtpState> state);
};
} // namespace gpui
