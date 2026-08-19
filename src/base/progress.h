/* Unstyled progress — crates/base/src/progress.rs */

#include "gpui/gpui.h"

namespace gpui {

// value.clamp(0., 100.), which Rust applies in `Progress::value` so every part
// of the control agrees on what the value is. That clamp is the only behavior
// in the module: `value` and `indeterminate` otherwise exist to fill the
// accessibility node, and the caller sizes the track and the indicator itself.
// So the number comes through here and the elements stay identity, rather than
// carrying two parameters that would go nowhere.
float ProgressClampValue(float value);

struct Progress {
    static El* New(Ctx* cx, Str id);
};

struct ProgressTrack {
    static El* New(Ctx* cx);
};

struct ProgressIndicator {
    static El* New(Ctx* cx);
};
} // namespace gpui
