/* Themed charts — crates/ui/src/chart */

#include "component/Common.h"

namespace gpui {

namespace component {

struct AreaChart {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const float* ys = nullptr;
    int n = 0;
    Rgba stroke = {};
    Rgba fill = {};

    static AreaChart* New(Ctx* cx, const float* ys, int n);
    AreaChart* Stroke(Rgba c);
    AreaChart* Fill(Rgba c);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
