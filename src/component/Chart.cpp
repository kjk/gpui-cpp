#include "component/Chart.h"

namespace component {

AreaChart* AreaChart::New(Arena* a, const float* ys, int n) {
    AreaChart* c = ::New<AreaChart>(a);
    c->a = a;
    c->ys = ys;
    c->n = n;
    c->stroke = ThemeNow().blue;
    c->fill = RgbaOpacity(ThemeNow().blue, 0.25f);
    return c;
}
AreaChart* AreaChart::Stroke(Rgba c) {
    stroke = c;
    return this;
}
AreaChart* AreaChart::Fill(Rgba c) {
    fill = c;
    return this;
}

El* AreaChart::IntoEl() {
    return ChartEl(a, ys, n, stroke, fill, RgbaOpacity(fill, 0.0f), 15);
}

} // namespace component
