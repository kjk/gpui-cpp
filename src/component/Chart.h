/* Themed charts — crates/ui/src/chart */

#include "component/Common.h"

namespace gpui {

namespace component {

// A pie or donut: each slice is a value and a color, drawn clockwise from
// twelve o'clock (crates/ui/src/chart/pie_chart.rs).
struct PieSlice {
    float value = 0;
    Rgba color = {};
    // outer_radius_fn lets a slice pull in from the rim.
    float outerInset = 0;
};

struct PieChart {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    PieSlice slices[12] = {};
    int n = 0;
    float outerRadius = 100;
    float innerRadius = 0;
    float padAngle = 0;

    static PieChart* New(Ctx* cx);
    PieChart* Slice(float value, Rgba color, float outerInset = 0);
    PieChart* OuterRadius(float r);
    PieChart* InnerRadius(float r);
    PieChart* PadAngle(float radians);
    El* IntoEl();
};

struct AreaChart {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const float* ys = nullptr;
    int n = 0;
    const char* const* labels = nullptr;
    int tickMargin = 15;
    // A stacked chart draws its second series over the first one's grid.
    bool overlay = false;
    Rgba stroke = {};
    Rgba fill = {};

    static AreaChart* New(Ctx* cx, const float* ys, int n);
    AreaChart* Stroke(Rgba c);
    AreaChart* Fill(Rgba c);
    AreaChart* Labels(const char* const* l);
    AreaChart* TickMargin(int n);
    AreaChart* Overlay(bool v = true);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
