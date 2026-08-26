#include "base/measure.h"

#include <stdio.h>
#include <stdlib.h>

namespace gpui {

bool MeasurementEnabled() {
    return getenv("ZED_MEASUREMENTS") != nullptr ||
           getenv("GPUI_MEASUREMENTS") != nullptr;
}

Measure MeasureBegin(Str name) {
    Measure m;
    m.name = name;
    m.started = TimeNow();
    m.active = MeasurementEnabled();
    return m;
}

void MeasureEnd(Measure* measure) {
    if (!measure || !measure->active) {
        return;
    }
    double elapsedMs = (TimeNow() - measure->started) * 1000.0;
    printf("%.*s in %.3f ms\n", measure->name.len,
           measure->name.s ? measure->name.s : "", elapsedMs);
    measure->active = false;
}

void MeasureRunIf(Str name, bool enabled, MeasureFn fn, void* user) {
    if (!fn) {
        return;
    }
    if (!enabled || !MeasurementEnabled()) {
        fn(user);
        return;
    }
    Measure m = MeasureBegin(name);
    fn(user);
    MeasureEnd(&m);
}

void MeasureRun(Str name, MeasureFn fn, void* user) {
    MeasureRunIf(name, true, fn, user);
}

} // namespace gpui
