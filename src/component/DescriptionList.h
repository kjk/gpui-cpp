/* Themed description list — crates/ui/src/description_list.rs */

#include "component/Common.h"

namespace gpui {

namespace component {

struct DescriptionList {
    Arena* a = nullptr;
    Str keys[12] = {};
    Str vals[12] = {};
    int n = 0;

    static DescriptionList* New(Arena* a);
    DescriptionList* Item(Str key, Str val);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
