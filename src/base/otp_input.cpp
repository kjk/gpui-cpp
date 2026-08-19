#include "base/otp_input.h"
#include "base/element_ext.h"

namespace gpui {

El* OtpInput::New(Ctx* cx, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("example-otp"), clickId);
}
} // namespace gpui
