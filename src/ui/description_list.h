/* Themed description list — crates/ui/src/description_list.rs */

#include "ui/sizing.h"

namespace gpui {

namespace component {

// One row cell. A separator item ends the row it sits in, the way
// DescriptionItem::Separator does.
struct DescriptionItem {
    Str label = {};
    El* value = nullptr;
    int span = 1;
    bool separator = false;
};

struct DescriptionList {
    Arena* a = nullptr;
    Ctx* cx = nullptr;
    ArenaVec<DescriptionItem> items;
    int columns = 3;
    float labelWidth = 120;
    bool bordered = true;
    // DescriptionList::layout: Horizontal puts the label beside the value,
    // Vertical stacks it above.
    bool vertical = false;
    UiSize size = UiSize::Medium;

    static DescriptionList* New(Ctx* cx);
    // The value is text; Value() takes a built element instead.
    DescriptionList* Item(Str label, Str value, int span = 1);
    DescriptionList* ItemEl(Str label, El* value, int span = 1);
    DescriptionList* Separator();
    DescriptionList* Columns(int n);
    DescriptionList* LabelWidth(float w);
    DescriptionList* Bordered(bool v);
    DescriptionList* Vertical(bool v = true);
    DescriptionList* WithSize(UiSize s);
    El* IntoEl();
};

} // namespace component
} // namespace gpui
