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
    FormField field = {};
    field.label = label;
    field.control = control;
    fields.Append(a, field);
    return this;
}
Form* Form::FieldEl(El* label, El* control) {
    FormField field = {};
    field.labelEl = label;
    field.control = control;
    fields.Append(a, field);
    return this;
}

// Both apply to the field added last, the way the Rust builder chains onto
// the field it is building.
Form* Form::Required(bool v) {
    if (fields.len > 0) {
        fields[fields.len - 1].required = v;
    }
    return this;
}
Form* Form::Description(Str s) {
    if (fields.len > 0) {
        fields[fields.len - 1].description = s;
    }
    return this;
}
Form* Form::DescriptionEl(El* e) {
    if (fields.len > 0) {
        fields[fields.len - 1].descriptionEl = e;
    }
    return this;
}
Form* Form::SpanAll(bool v) {
    if (fields.len > 0) {
        fields[fields.len - 1].spanAll = v;
    }
    return this;
}
Form* Form::Visible(bool v) {
    if (fields.len > 0) {
        fields[fields.len - 1].visible = v;
    }
    return this;
}
Form* Form::LabelIndent(bool v) {
    if (fields.len > 0) {
        fields[fields.len - 1].labelIndent = v;
    }
    return this;
}
Form* Form::Align(FieldAlign v) {
    if (fields.len > 0) {
        fields[fields.len - 1].align = v;
    }
    return this;
}
Form* Form::WithSize(UiSize v) {
    size = v;
    return this;
}
Form* Form::LabelTextSize(float px) {
    labelTextSize = px;
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
    const Theme& th = ThemeNow(cx->app);
    // The gap comes from the size: eight at Large, four otherwise, and a
    // vertical field halves it between the label and the control.
    const float kGap = 8;
    float fieldGap = size == UiSize::Large ? 8.f : 4.f;
    float inner = horizontal ? fieldGap : fieldGap * 0.5f;
    float labelFont = labelTextSize > 0 ? labelTextSize : 14.f;
    float lw = labelWidth > 0 ? labelWidth : (columns > 1 ? 100.f : 140.f);

    El* col = Div(a)->FlexCol()->W(kFill)->Gap(kGap);
    El* row = nullptr; // the row being filled, when the form has columns
    int inRow = 0;
    for (int i = 0; i < fields.len; i++) {
        const FormField& fld = fields[i];
        // visible(false): the field is left out of the form entirely.
        if (!fld.visible) {
            continue;
        }
        El* f = Div(a)->FlexCol()->W(kFill)->Gap(fieldGap * 0.5f);

        El* head = Div(a)->W(kFill)->Gap(inner);
        if (horizontal) {
            head->FlexRow();
            if (fld.align == FieldAlign::Start) {
                head->ItemsStart();
            } else if (fld.align == FieldAlign::End) {
                head->ItemsEnd();
            } else {
                head->ItemsCenter();
            }
        } else {
            head->FlexCol();
        }
        bool hasLabel = fld.labelIndent && (fld.label.s || fld.labelEl);
        if (hasLabel) {
            El* label = Div(a)->FlexRow()->W(lw)->Gap(4)->ItemsCenter();
            if (fld.labelEl) {
                label->Child(fld.labelEl);
            } else {
                label->Child(TextEl(a, fld.label)
                                 ->Font(labelFont)
                                 ->Medium()
                                 ->Fg(th.foreground));
            }
            if (fld.required) {
                label->Child(
                    TextEl(a, StrL("*"))->Font(labelFont)->Fg(th.danger));
            }
            head->Child(label);
        }
        El* control = Div(a)->W(kFill)->Flex1();
        if (fld.control) {
            control->Child(fld.control);
        }
        head->Child(control);
        f->Child(head);

        // Rust always adds the description row, so a field without one is
        // still a half-gap taller.
        El* desc = Div(a)->FlexRow()->W(kFill)->Gap(inner);
        if (fld.description.s || fld.descriptionEl) {
            // Horizontal, the description lines up under the control.
            if (horizontal && hasLabel) {
                desc->Child(Div(a)->W(lw));
            }
            if (fld.descriptionEl) {
                desc->Child(fld.descriptionEl);
            } else {
                desc->Child(
                    TextEl(a, fld.description)->Font(12)->Fg(th.mutedFg));
            }
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
        row->Child(Div(a)->Flex1()->Child(f));
        inRow++;
        if (inRow >= columns) {
            row = nullptr;
        }
    }
    // Pad the last row so a lone field keeps its column width.
    if (row) {
        for (int i = inRow; i < columns; i++) {
            row->Child(Div(a)->Flex1());
        }
    }
    return col;
}

} // namespace component
} // namespace gpui
