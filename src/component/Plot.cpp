#include "component/Plot.h"

namespace component {

float PlotScale::Map(float v) const {
    float d = max - min;
    if (d == 0) {
        return 0;
    }
    return (v - min) / d;
}

} // namespace component
