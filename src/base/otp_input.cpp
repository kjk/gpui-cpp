#include "base/otp_input.h"

namespace gpui {

char OtpDigitChar(uint32_t c) {
    if (c >= '0' && c <= '9') {
        return (char)c;
    }
    // U+FF10..U+FF19, the full-width digits. Rust subtracts '０' and asks
    // char::from_digit for the plain one.
    if (c >= 0xFF10 && c <= 0xFF19) {
        return (char)('0' + (c - 0xFF10));
    }
    return 0;
}

bool OtpEditValue(OtpState* s, int key, uint32_t ch) {
    if (key == KeyBack) {
        if (s->len == 0) {
            return false;
        }
        s->len--;
        s->value[s->len] = 0;
        return true;
    }
    // Rust reads the keystroke's own name first — a digit key is called "4" —
    // and falls back to the character it produced. The Key* code for a digit
    // is that ASCII digit, so the same two tries in the same order.
    char digit = OtpDigitChar((uint32_t)key);
    if (!digit) {
        digit = OtpDigitChar(ch);
    }
    if (!digit) {
        return false;
    }
    // Rust returns None once the code is full, so a further digit is dropped
    // rather than shifting the run.
    int cap = s->length < (int)sizeof(s->value) - 1 ? s->length
                                                    : (int)sizeof(s->value) - 1;
    if (s->len >= cap) {
        return false;
    }
    s->value[s->len] = digit;
    s->len++;
    s->value[s->len] = 0;
    return true;
}

void OtpFocus(OtpState* s, App* app, Window* win) {
    if (s->disabled || s->focused) {
        return;
    }
    s->focused = true;
    BlinkStart(app, win, &s->blink);
}

void OtpBlur(OtpState* s, App* app, Window* win) {
    if (!s->focused) {
        return;
    }
    s->focused = false;
    BlinkStop(app, win, &s->blink);
}

bool OtpCursorVisible(OtpState* s, App* app) {
    return s->focused && BlinkVisible(app, s->blink);
}

El* OtpInput::New(Ctx* cx, Str id) {
    Arena* a = cx->a;
    El* e = Div(a);
    if (id.s) {
        e->Id(id)->Click(HashClickId(id))->FocusId(HashClickId(id));
    }
    return e;
}
} // namespace gpui
