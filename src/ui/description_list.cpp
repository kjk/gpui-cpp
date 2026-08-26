#include "ui/description_list.h"

namespace gpui {

namespace component {

DescriptionList* DescriptionList::New(Ctx* cx) {
    Arena* a = cx->a;
    DescriptionList* d = ArenaNew<DescriptionList>(a);
    d->a = a;
    d->cx = cx;
    return d;
}

DescriptionList* DescriptionList::Item(Str label, Str value, int span) {
    return ItemEl(label, TextEl(a, value)->Font(14)->Wrap(), span);
}

DescriptionList* DescriptionList::ItemEl(Str label, El* value, int span) {
    DescriptionItem item = {};
    item.label = label;
    item.value = value;
    item.span = span < 1 ? 1 : span;
    items.Append(a, item);
    return this;
}

DescriptionList* DescriptionList::Separator() {
    DescriptionItem item = {};
    item.separator = true;
    items.Append(a, item);
    return this;
}

DescriptionList* DescriptionList::Columns(int v) {
    columns = v < 1 ? 1 : v;
    return this;
}
DescriptionList* DescriptionList::LabelWidth(float w) {
    labelWidth = w;
    return this;
}
DescriptionList* DescriptionList::Bordered(bool v) {
    bordered = v;
    return this;
}
DescriptionList* DescriptionList::Vertical(bool v) {
    vertical = v;
    return this;
}
DescriptionList* DescriptionList::WithSize(UiSize s) {
    size = s;
    return this;
}

El* DescriptionList::IntoEl() {
    const Theme& th = cx->theme();
    // Medium: px 8 / py 4 inside a bordered box; unbordered drops the padding
    // and spaces the rows by the base gap instead.
    float padX = 8, padY = 4;
    if (size == UiSize::Small || size == UiSize::XSmall) {
        padX = 4;
        padY = 2;
    } else if (size == UiSize::Large) {
        padX = 12;
        padY = 6;
    }
    float gap = bordered ? 0.f : padY;
    if (!bordered) {
        padX = 0;
        padY = 0;
    }

    El* root = Div(a)->FlexCol()->W(kFill)->Gap(gap)->ClipY();
    if (bordered) {
        root->Border(1, th.border)->Radius(padX);
    }

    // Pack the items into rows of `columns`, spans included; a separator ends
    // the row it lands in.
    int i = 0;
    while (i < items.len) {
        int used = 0;
        int count = 0;
        while (i < items.len && used < columns) {
            if (items[i].separator) {
                i++;
                break;
            }
            if (used + items[i].span > columns && count > 0) {
                break;
            }
            count++;
            used += items[i].span;
            i++;
        }
        if (count == 0) {
            continue;
        }
        int first = i - count;
        El* row = Div(a)->FlexRow()->W(kFill);
        bool last = i >= items.len;
        if (bordered && !last) {
            row->BorderB(1, th.border);
        }
        for (int k = 0; k < count; k++) {
            const DescriptionItem& it = items[first + k];
            El* cell = Div(a)->Grow((float)it.span);
            cell = vertical ? cell->FlexCol() : cell->FlexRow();
            El* label = Div(a)->Shrink0()->PadX(padX)->PadY(padY)->Child(
                TextEl(a, it.label)->Font(14)->Wrap()->Fg(th.descListLabelFg));
            if (!vertical) {
                label->W(labelWidth);
            } else {
                label->W(kFill);
            }
            if (bordered) {
                label->Bg(th.descListLabel)->Border(1, th.border);
            }
            El* value = Div(a)->Flex1()->PadX(padX)->PadY(padY)->ClipY();
            if (it.value) {
                value->Child(it.value);
            }
            cell->Child(label)->Child(value);
            row->Child(cell);
        }
        root->Child(row);
    }
    return root;
}

} // namespace component
} // namespace gpui
