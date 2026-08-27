#ifndef GPUI_BASE_MEASURE_H_
#define GPUI_BASE_MEASURE_H_
/* Measurement logging — crates/base/src/measure.rs */

#include "gpui/gpui.h"

namespace gpui {

bool MeasurementEnabled();

struct Measure {
    Str name = {};
    double started = 0;
    bool active = false;
};

Measure MeasureBegin(Str name);
void MeasureEnd(Measure* measure);

using MeasureFn = Func0;
void MeasureRun(Str name, MeasureFn fn);
void MeasureRunIf(Str name, bool enabled, MeasureFn fn);

} // namespace gpui
#endif // GPUI_BASE_MEASURE_H_
