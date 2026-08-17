#include "ui/OtpInput.h"
#include "ui/Primitive.h"

namespace gpui {

El* OtpInput::New(Arena* a, int clickId) {
    return UiRoot(a, StrL("example-otp"), clickId);
}
} // namespace gpui
