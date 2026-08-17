#include "ui/OtpInput.h"
#include "ui/Primitive.h"

namespace gpui {

El* OtpInput::New(Ctx* cx, int clickId) {
    Arena* a = cx->a;
    return UiRoot(a, StrL("example-otp"), clickId);
}
} // namespace gpui
