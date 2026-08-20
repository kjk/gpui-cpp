/* Themed charts — crates/ui/src/chart */

#include "ui/sizing.h"

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

// LineChart: the same run of points as an area chart, with nothing filled
// under it.
struct LineChart {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const float* ys = nullptr;
    int n = 0;
    const char* const* labels = nullptr;
    int tickMargin = 15;
    Rgba stroke = {};
    float domainMin = 0;
    float domainMax = 0;

    static LineChart* New(Ctx* cx, const float* ys, int n);
    LineChart* Stroke(Rgba c);
    LineChart* Labels(const char* const* l);
    LineChart* TickMargin(int n);
    LineChart* Domain(float lo, float hi);
    El* IntoEl();
};

// BarChart: a band per value, the bars rounded at the top.
struct BarChart {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const float* ys = nullptr;
    int n = 0;
    const char* const* labels = nullptr;
    int tickMargin = 1;
    Rgba fill = {};
    // ScaleBand's inner padding: how much of a band the gap beside it takes.
    float padding = 0.2f;
    float radius = 4;
    float domainMin = 0;
    float domainMax = 0;

    static BarChart* New(Ctx* cx, const float* ys, int n);
    BarChart* Fill(Rgba c);
    BarChart* Labels(const char* const* l);
    BarChart* TickMargin(int n);
    BarChart* Padding(float v);
    BarChart* Radius(float v);
    BarChart* Domain(float lo, float hi);
    El* IntoEl();
};

// CandlestickChart: open, high, low and close per band, the body colored by
// which way it closed.
struct CandlestickChart {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const float* opens = nullptr;
    const float* highs = nullptr;
    const float* lows = nullptr;
    const float* closes = nullptr;
    int n = 0;
    const char* const* labels = nullptr;
    int tickMargin = 1;
    Rgba up = {};
    Rgba down = {};
    float padding = 0.3f;

    static CandlestickChart* New(Ctx* cx, const float* opens,
                                 const float* highs, const float* lows,
                                 const float* closes, int n);
    CandlestickChart* Colors(Rgba up, Rgba down);
    CandlestickChart* Labels(const char* const* l);
    CandlestickChart* TickMargin(int n);
    CandlestickChart* Padding(float v);
    El* IntoEl();
};

// RadarChart: one value per axis, plotted on rings around a centre.
struct RadarChart {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    const float* values = nullptr;
    int n = 0;
    const char* const* labels = nullptr;
    Rgba stroke = {};
    Rgba fill = {};
    float domainMin = 0;
    float domainMax = 0;

    static RadarChart* New(Ctx* cx, const float* values, int n);
    RadarChart* Stroke(Rgba c);
    RadarChart* Fill(Rgba c);
    RadarChart* Labels(const char* const* l);
    RadarChart* Domain(float lo, float hi);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
