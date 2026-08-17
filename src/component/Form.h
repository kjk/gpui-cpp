/* Themed form — crates/ui/src/form */

#include "component/Common.h"

namespace gpui {

namespace component {

struct FormField {
    Str label = {};
    El* control = nullptr;
};

struct Form {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    FormField fields[8] = {};
    int n = 0;

    static Form* New(Ctx* cx);
    Form* Field(Str label, El* control);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
