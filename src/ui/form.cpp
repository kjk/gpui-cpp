#include "ui/form.h"

namespace gpui {

namespace component {

Form* Form::New(Ctx* cx) {
    Arena* a = cx->a;
    Form* f = ArenaNew<Form>(a);
    f->a = a;
    f->cx = cx;
    return f;
}
Form* Form::Field(Str label, El* control) {
    if (n < 12) {
        fields[n].label = label;
        fields[n].control = control;
        n++;
    }
    return this;
}

// Both apply to the field added last, the way the Rust builder chains onto
// the field it is building.
Form* Form::Required(bool v) {
    if (n > 0) {
        fields[n - 1].required = v;
    }
    return this;
}
Form* Form::Description(Str s) {
    if (n > 0) {
        fields[n - 1].description = s;
    }
    return this;
}
Form* Form::SpanAll(bool v) {
    if (n > 0) {
        fields[n - 1].spanAll = v;
    }
    return this;
}

Form* Form::Horizontal(bool v) {
    horizontal = v;
    return this;
}
Form* Form::Columns(int c) {
    columns = c < 1 ? 1 : c;
    return this;
}
Form* Form::LabelWidth(float w) {
    labelWidth = w;
    return this;
}

El* Form::IntoEl() {
    const Theme& th = cx->theme();
    // Medium: the form gaps by 8 (24 between columns), a field by 4.
    const float kGap = 8;
    const float kFieldGap = 4;
    float inner = horizontal ? kFieldGap : kFieldGap * 0.5f;
    float lw = labelWidth > 0 ? labelWidth : (columns > 1 ? 100.f : 140.f);

    El* col = Div(a)->FlexCol()->W(kFill)->Gap(kGap);
    El* row = nullptr; // the row being filled, when the form has columns
    int inRow = 0;
    for (int i = 0; i < n; i++) {
        const FormField& fld = fields[i];
        El* f = Div(a)->FlexCol()->W(kFill)->Gap(kFieldGap * 0.5f);

        El* head = Div(a)->W(kFill)->Gap(inner);
        if (horizontal) {
            head->FlexRow()->ItemsCenter();
        } else {
            head->FlexCol();
        }
        if (fld.label.s) {
            El* label = Div(a)->FlexRow()->W(lw)->Gap(4)->ItemsCenter();
            label->Child(
                TextEl(a, fld.label)->Font(14)->Medium()->Fg(th.foreground));
            if (fld.required) {
                label->Child(TextEl(a, StrL("*"))->Font(14)->Fg(th.danger));
            }
            head->Child(label);
        }
        El* control = Div(a)->W(kFill)->Grow();
        if (fld.control) {
            control->Child(fld.control);
        }
        head->Child(control);
        f->Child(head);

        // Rust always adds the description row, so a field without one is
        // still a half-gap taller.
        El* desc = Div(a)->FlexRow()->W(kFill)->Gap(inner);
        if (fld.description.s) {
            // Horizontal, the description lines up under the control.
            if (horizontal && fld.label.s) {
                desc->Child(Div(a)->W(lw));
            }
            desc->Child(TextEl(a, fld.description)->Font(12)->Fg(th.mutedFg));
        }
        f->Child(desc);

        if (columns <= 1) {
            col->Child(f);
            continue;
        }
        // A grid of `columns` cells per row, with the wide fields on their own.
        if (fld.spanAll) {
            row = nullptr;
            inRow = 0;
            col->Child(f);
            continue;
        }
        if (!row) {
            row = Div(a)->FlexRow()->W(kFill)->Gap(kGap * 3)->ItemsStart();
            col->Child(row);
            inRow = 0;
        }
        row->Child(Div(a)->Grow()->Child(f));
        inRow++;
        if (inRow >= columns) {
            row = nullptr;
        }
    }
    // Pad the last row so a lone field keeps its column width.
    if (row) {
        for (int i = inRow; i < columns; i++) {
            row->Child(Div(a)->Grow());
        }
    }
    return col;
}

} // namespace component
} // namespace gpui
