/* Unstyled OTP input — crates/base/src/otp_input.rs */

#include "gpui/Gpui.h"

namespace gpui {

struct OtpInput {
    static El* New(Arena* a, int clickId = 0);
};
} // namespace gpui
