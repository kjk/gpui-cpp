/* Themed form — crates/ui/src/form */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// items_start / items_center / items_end: how the label and the control line
// up against each other in a horizontal field.
enum class FieldAlign : uint8_t {
    Center,
    Start,
    End
};

struct FormField {
    Str label = {};
    // label_fn: a label the caller built, in place of the text one.
    El* labelEl = nullptr;
    El* control = nullptr;
    // description sits under the control; required draws the danger asterisk
    // next to the label.
    Str description = {};
    El* descriptionEl = nullptr;
    bool required = false;
    // col_span(2): the field takes a whole row of a multi-column form.
    bool spanAll = false;
    // visible(false): the field is built and then left out, which is what
    // lets a form keep its shape while a field comes and goes.
    bool visible = true;
    // label_indent(false): no label column at all, so the control starts at
    // the form's edge.
    bool labelIndent = true;
    FieldAlign align = FieldAlign::Center;
};

struct Form {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    FormField fields[12] = {};
    int n = 0;
    bool horizontal = false;
    int columns = 1;
    float labelWidth = 0; // 0: 140, or 100 in a multi-column form
    // The size the gaps come from: Large gaps by eight, everything else by
    // four, and a vertical field halves it between its parts.
    UiSize size = UiSize::Medium;
    // label_text_size. 0 keeps text_sm.
    float labelTextSize = 0;

    static Form* New(Ctx* cx);
    // A label-less field spans the row on its own, the way Rust's
    // `field().label_indent(false)` does.
    Form* Field(Str label, El* control);
    // A field with a label the caller built rather than a string.
    Form* FieldEl(El* label, El* control);
    Form* Required(bool v = true);
    Form* Description(Str s);
    Form* DescriptionEl(El* e);
    Form* SpanAll(bool v = true);
    Form* Visible(bool v);
    Form* LabelIndent(bool v);
    Form* Align(FieldAlign v);
    Form* Horizontal(bool v = true);
    Form* Columns(int n);
    Form* LabelWidth(float w);
    Form* WithSize(UiSize v);
    Form* LabelTextSize(float px);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
