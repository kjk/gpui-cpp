/* Themed form — crates/ui/src/form */

#include "component/Common.h"

namespace gpui {

namespace component {

struct FormField {
    Str label = {};
    El* control = nullptr;
    // description sits under the control; required draws the danger asterisk
    // next to the label.
    Str description = {};
    bool required = false;
    // col_span(2): the field takes a whole row of a multi-column form.
    bool spanAll = false;
};

struct Form {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    FormField fields[12] = {};
    int n = 0;
    bool horizontal = false;
    int columns = 1;
    float labelWidth = 0; // 0: 140, or 100 in a multi-column form

    static Form* New(Ctx* cx);
    // A label-less field spans the row on its own, the way Rust's
    // `field().label_indent(false)` does.
    Form* Field(Str label, El* control);
    Form* Required(bool v = true);
    Form* Description(Str s);
    Form* SpanAll(bool v = true);
    Form* Horizontal(bool v = true);
    Form* Columns(int n);
    Form* LabelWidth(float w);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
