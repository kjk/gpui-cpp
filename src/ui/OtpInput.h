/* Unstyled OTP input — crates/base/src/otp_input.rs */

#pragma once

#include "gpui/Gpui.h"

struct OtpInput {
    static El* New(Arena* a, int clickId = 0);
};
