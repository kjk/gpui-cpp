#include "component/Chart.h"

namespace gpui {

namespace component {

AreaChart* AreaChart::New(Ctx* cx, const float* ys, int n) {
    Arena* a = cx->a;
    AreaChart* c = ArenaNew<AreaChart>(a);
    c->a = a;
    c->cx = cx;
    c->ys = ys;
    c->n = n;
    c->stroke = cx->theme().blue;
    c->fill = RgbaOpacity(cx->theme().blue, 0.25f);
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
} // namespace gpui
