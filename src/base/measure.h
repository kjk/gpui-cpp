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

using MeasureFn = void (*)(void* user);
void MeasureRun(Str name, MeasureFn fn, void* user);
void MeasureRunIf(Str name, bool enabled, MeasureFn fn, void* user);

} // namespace gpui
