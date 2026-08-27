/* crates/ui/src/sizing.rs: Size values and StyleSized projections. */

#include "Test.h"

static void NamedAndCustomSizesPreserveTheirValues() {
    utassert(UiSize() == UiSize::Medium);
    utassert(UiSize::Custom(15) == UiSize::Custom(15));
    utassert(UiSize::Custom(15) != UiSize::Custom(16));
    utassert(base::StrEq(UiSizeAsStr(UiSize::XSmall), "xs"));
    utassert(base::StrEq(UiSizeAsStr(UiSize::Small), "sm"));
    utassert(base::StrEq(UiSizeAsStr(UiSize::Medium), "md"));
    utassert(base::StrEq(UiSizeAsStr(UiSize::Large), "lg"));
    utassert(base::StrEq(UiSizeAsStr(UiSize::Custom(15)), "custom"));

    utassert(UiSizeFromStr(StrL("xs")) == UiSize::XSmall);
    utassert(UiSizeFromStr(StrL("xsmall")) == UiSize::XSmall);
    utassert(UiSizeFromStr(StrL("SMALL")) == UiSize::Small);
    utassert(UiSizeFromStr(StrL("Md")) == UiSize::Medium);
    utassert(UiSizeFromStr(StrL("medium")) == UiSize::Medium);
    utassert(UiSizeFromStr(StrL("LG")) == UiSize::Large);
    utassert(UiSizeFromStr(StrL("large")) == UiSize::Large);
    utassert(UiSizeFromStr(StrL("unknown")) == UiSize::Medium);
}

static void MinMaxAndStepsMatchThePinnedDirection() {
    utassert(UiSizeMin(UiSize::Small, UiSize::XSmall) == UiSize::Small);
    utassert(UiSizeMin(UiSize::XSmall, UiSize::Small) == UiSize::Small);
    utassert(UiSizeMin(UiSize::Small, UiSize::Medium) == UiSize::Medium);
    utassert(UiSizeMin(UiSize::Medium, UiSize::Large) == UiSize::Large);
    utassert(UiSizeMin(UiSize::Large, UiSize::Small) == UiSize::Large);
    utassert(UiSizeMin(UiSize::Custom(10), UiSize::Custom(20)) ==
             UiSize::Custom(20));

    utassert(UiSizeMax(UiSize::Small, UiSize::XSmall) == UiSize::XSmall);
    utassert(UiSizeMax(UiSize::XSmall, UiSize::Small) == UiSize::XSmall);
    utassert(UiSizeMax(UiSize::Small, UiSize::Medium) == UiSize::Small);
    utassert(UiSizeMax(UiSize::Medium, UiSize::Large) == UiSize::Medium);
    utassert(UiSizeMax(UiSize::Large, UiSize::Small) == UiSize::Small);
    utassert(UiSizeMax(UiSize::Custom(10), UiSize::Custom(20)) ==
             UiSize::Custom(10));

    utassert(UiSizeSmaller(UiSize::XSmall) == UiSize::XSmall);
    utassert(UiSizeSmaller(UiSize::Large) == UiSize::Medium);
    utassert(UiSizeSmaller(UiSize::Custom(50)) == UiSize::Custom(10));
    utassert(UiSizeLarger(UiSize::XSmall) == UiSize::Small);
    utassert(UiSizeLarger(UiSize::Large) == UiSize::Large);
    UiSize larger = UiSizeLarger(UiSize::Custom(50));
    utassert(larger.kind == UiSize::Kind::Size);
    utassertnear(larger.pixels, 60);
}

static void TableAndInputConstantsAreExact() {
    utassertnear(UiTableRowHeight(UiSize::XSmall), 26);
    utassertnear(UiTableRowHeight(UiSize::Small), 30);
    utassertnear(UiTableRowHeight(UiSize::Medium), 32);
    utassertnear(UiTableRowHeight(UiSize::Large), 40);
    utassertnear(UiTableRowHeight(UiSize::Custom(48)), 48);

    Edges xs = UiTableCellPadding(UiSize::XSmall);
    Edges sm = UiTableCellPadding(UiSize::Small);
    Edges md = UiTableCellPadding(UiSize::Medium);
    Edges lg = UiTableCellPadding(UiSize::Large);
    utassert(xs == Edges::New(4, 4, 2, 2));
    utassert(sm == Edges::New(6, 6, 3, 3));
    utassert(md == Edges::New(8, 8, 4, 4));
    utassert(lg == Edges::New(12, 12, 8, 8));

    utassertnear(UiInputPadX(UiSize::Large), 12);
    utassertnear(UiInputPadY(UiSize::Large), 10);
    utassertnear(UiInputHeight(UiSize::XSmall), 20);
    utassertnear(UiInputHeight(UiSize::Custom(70)), 24);
    utassertnear(UiInputFontPx(UiSize::XSmall), 12);
    utassertnear(UiInputFontPx(UiSize::Custom(40)), 35);
    utassertnear(UiSizeWithPx(UiSize::Large), 44);
    utassertnear(UiSizeWithPx(UiSize::Custom(37)), 37);
}

static void StyleSizedHelpersRefineTheElement() {
    Arena* a = ArenaNew();
    El* input = UiInputSize(Div(a), UiSize::Small);
    utassertnear(input->style.pad.left, 8);
    utassertnear(input->style.pad.right, 8);
    utassertnear(input->style.pad.top, 2);
    utassertnear(input->style.pad.bottom, 2);
    utassertnear(input->style.height, 24);

    El* list = UiListSize(Div(a), UiSize::Large);
    utassertnear(list->style.pad.left, 12);
    utassertnear(list->style.pad.top, 8);
    utassertnear(list->style.fontSize, 16);

    El* custom = UiSizeWith(Div(a), UiSize::Custom(37));
    utassertnear(custom->style.width, 37);
    utassertnear(custom->style.height, 37);

    El* cell = UiTableCellSize(Div(a), UiSize::XSmall);
    utassertnear(cell->style.fontSize, 14);
    utassert(cell->style.pad == Edges::New(4, 4, 2, 2));
    El* button = UiButtonTextSize(Div(a), UiSize::Small);
    utassertnear(button->style.fontSize, 14);
    ArenaDelete(a);
}

void TestSizing() {
    TestSuite("sizing");
    NamedAndCustomSizesPreserveTheirValues();
    MinMaxAndStepsMatchThePinnedDirection();
    TableAndInputConstantsAreExact();
    StyleSizedHelpersRefineTheElement();
}
