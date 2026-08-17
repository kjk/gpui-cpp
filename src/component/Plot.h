/* Plot helpers — crates/ui/src/plot
   Scale/shape math used by Chart; AreaChart is the render entry. */

#pragma once

#include "component/Chart.h"

namespace component {

struct PlotScale {
    float min = 0;
    float max = 1;
    float Map(float v) const;
};

} // namespace component
