/* Themed charts — crates/ui/src/chart */

#include "component/Common.h"

namespace gpui {

namespace component {

struct AreaChart {
    Arena* a = nullptr;
    const float* ys = nullptr;
    int n = 0;
    Rgba stroke = {};
    Rgba fill = {};

    static AreaChart* New(Arena* a, const float* ys, int n);
    AreaChart* Stroke(Rgba c);
    AreaChart* Fill(Rgba c);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
