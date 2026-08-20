/* Themed progress — crates/ui/src/progress */

#include "ui/sizing.h"
#include "base/motion.h"

namespace gpui {

namespace component {

struct Progress {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    float value = 0;
    float w = 200;
    float h = 8;
    // `loading`: the indeterminate bar, which sweeps rather than filling.
    bool loading = false;
    // The bar's own name, so two of them on a page transition apart.
    Str id = {};

    static Progress* New(Ctx* cx);
    Progress* Value(float v);
    Progress* W(float v);
    Progress* H(float v);
    Progress* Loading(bool v);
    Progress* Id(Str v);
    El* IntoEl();
};

struct ProgressCircle {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    float value = 0;
    float size = 48;
    // Where the arc starts, which only the indeterminate one moves off zero.
    float startValue = 0;
    Rgba color = {};
    bool hasColor = false;
    bool showLabel = true;
    bool loading = false;
    Str id = {};

    static ProgressCircle* New(Ctx* cx);
    ProgressCircle* Loading(bool v);
    ProgressCircle* Id(Str v);
    ProgressCircle* Value(float v);
    ProgressCircle* Size(float v);
    ProgressCircle* Color(Rgba c);
    ProgressCircle* Label(bool v);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
