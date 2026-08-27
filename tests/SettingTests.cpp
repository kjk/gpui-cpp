/* Ported from crates/ui/src/setting/item.rs and settings.rs.
 *
 * `SettingItem::is_match` is what the search box filters on: the title, the
 * description and the keywords, all lowercased. `filtered_pages` then drops a
 * group whose items all fell out and a page whose groups all did. */

#include "Test.h"

using namespace gpui::component;

static SettingItem Item(const char* title, const char* desc) {
    SettingItem it;
    it.title = Str(title);
    it.description = Str(desc);
    return it;
}

static void TheQueryMatchesTitleDescriptionAndKeywords() {
    Arena* a = ArenaNew();
    SettingItem it = Item("Dark Mode", "Switch between light and dark themes.");
    it.keywords.Append(a, StrL("appearance"));

    // An empty query matches everything, which is what an unfiltered list is.
    utassert(SettingItemMatches(&it, StrL("")));
    // The title, in any case.
    utassert(SettingItemMatches(&it, StrL("dark")));
    utassert(SettingItemMatches(&it, StrL("DARK")));
    utassert(SettingItemMatches(&it, StrL("Mode")));
    // The description.
    utassert(SettingItemMatches(&it, StrL("themes")));
    // And the keywords, which is the whole point of having them: nothing the
    // item shows says "appearance".
    utassert(SettingItemMatches(&it, StrL("APPEAR")));
    // Anything else does not.
    utassert(!SettingItemMatches(&it, StrL("font")));
    // A query longer than what it is matched against cannot be in it.
    utassert(!SettingItemMatches(&it, StrL("Dark Mode and then some")));
    ArenaDelete(a);
}

static void AGroupIsShownWhenAnythingInItIs() {
    Arena* a = ArenaNew();
    SettingGroup g;
    g.title = StrL("Appearance");
    g.items.Append(a, Item("Dark Mode", "Switch between themes."));
    g.items.Append(a, Item("Auto Switch", "Follow the system."));

    utassert(SettingGroupMatches(&g, StrL("")));
    utassert(SettingGroupMatches(&g, StrL("auto")));
    utassert(SettingGroupMatches(&g, StrL("system")));
    // Nothing in it matches, so the group goes — and Rust drops its header
    // and footer with it.
    utassert(!SettingGroupMatches(&g, StrL("font")));
    // An empty group has nothing to match.
    SettingGroup empty;
    utassert(!SettingGroupMatches(&empty, StrL("dark")));
    utassert(SettingGroupMatches(&empty, StrL("")));
    ArenaDelete(a);
}

static void APageIsShownWhenAnyGroupIs() {
    Arena* a = ArenaNew();
    SettingPage p;
    p.title = StrL("General");
    SettingGroup appearance;
    appearance.title = StrL("Appearance");
    appearance.items.Append(a, Item("Dark Mode", "Switch between themes."));
    SettingGroup font;
    font.title = StrL("Font");
    font.items.Append(a, Item("Font Size", "How big the text is."));
    p.groups.Append(a, appearance);
    p.groups.Append(a, font);

    utassert(SettingPageMatches(&p, StrL("dark")));
    utassert(SettingPageMatches(&p, StrL("font")));
    // The page's own title is not what a search matches on; the items are.
    utassert(!SettingPageMatches(&p, StrL("general")));
    utassert(!SettingPageMatches(&p, StrL("network")));
    ArenaDelete(a);
}

struct BoolSettingTarget {
    bool value = false;
    int sets = 0;
    bool customDirty = false;
    int customResets = 0;
};

static bool GetBoolSetting(void* user, const App*) {
    return ((BoolSettingTarget*)user)->value;
}

static void SetBoolSetting(void* user, bool value, App*) {
    BoolSettingTarget* target = (BoolSettingTarget*)user;
    target->value = value;
    target->sets++;
}

static bool IsCustomSettingDirty(void* user, const App*) {
    return ((BoolSettingTarget*)user)->customDirty;
}

static void ResetCustomSetting(void* user, Ctx*) {
    ((BoolSettingTarget*)user)->customResets++;
}

static void TypedFieldsRetainSourceResetSemanticsWithoutRtti() {
    BoolSettingTarget target;
    SettingField<bool> field = SettingField<bool>::New(
        SettingFieldType::Switch, &target, GetBoolSetting, SetBoolSetting);
    utassert(field.fieldType == SettingFieldType::Switch);
    utassert(!field.IsResettable(nullptr));

    field.DefaultValue(false);
    utassert(!field.IsResettable(nullptr));
    target.value = true;
    utassert(field.IsResettable(nullptr));

    AnySettingField any = EraseSettingField(&field);
    utassert(any.IsValid());
    utassert(any.typeId == SettingFieldTypeOf<bool>());
    utassert(any.fieldType == SettingFieldType::Switch);
    utassert(any.IsResettable(nullptr));
    any.Reset(nullptr);
    utassert(!target.value && target.sets == 1);

    field.OnReset(IsCustomSettingDirty, ResetCustomSetting);
    target.customDirty = false;
    utassert(!any.IsResettable(nullptr));
    target.customDirty = true;
    utassert(any.IsResettable(nullptr));
    any.Reset(nullptr);
    utassert(target.customResets == 1 && target.sets == 1);

    SettingField<Str> element = SettingField<Str>::New(
        SettingFieldType::Element, nullptr, nullptr, nullptr);
    element.DefaultValue(StrL("unused"));
    utassert(!element.IsResettable(nullptr));
    utassert(SettingFieldTypeOf<Str>() != SettingFieldTypeOf<bool>());
}

struct FieldElementCapture {
    RenderOptions options = {};
    int calls = 0;
};

static El* CaptureFieldOptions(void* user, const RenderOptions* options, Ctx*) {
    FieldElementCapture* capture = (FieldElementCapture*)user;
    capture->options = *options;
    capture->calls++;
    return nullptr;
}

static void RenderOptionsNarrowCopiesAndReachCustomFields() {
    RenderOptions base = RenderOptions::New();
    RenderOptions item = base.WithPageIx(2)
                             .WithGroupIx(3)
                             .WithItemIx(4)
                             .WithSize(UiSize::Large)
                             .WithGroupVariant(GroupBoxVariant::Outline)
                             .WithLayout(Axis::Vertical)
                             .WithDisabled(true);
    utassert(base.pageIx == 0 && base.layout == Axis::Horizontal &&
             !base.disabled && base.size == UiSize::Medium);
    utassert(item.pageIx == 2 && item.groupIx == 3 && item.itemIx == 4);
    utassert(item.size == UiSize::Large &&
             item.groupVariant == GroupBoxVariant::Outline);
    utassert(item.layout == Axis::Vertical && item.disabled);

    FieldElementCapture capture;
    SettingFieldElement element = {&capture, CaptureFieldOptions};
    utassert(element.IsValid());
    utassert(element.Render(&item, nullptr) == nullptr);
    utassert(capture.calls == 1 && capture.options.pageIx == 2 &&
             capture.options.itemIx == 4 && capture.options.disabled);

    SelectIndex selected;
    utassert(selected.pageIx == 0 && selected.groupIx == -1);
    selected = {3, 2};
    utassert(selected.pageIx == 3 && selected.groupIx == 2);
}

void TestSetting() {
    TestSuite("setting");
    TheQueryMatchesTitleDescriptionAndKeywords();
    AGroupIsShownWhenAnythingInItIs();
    APageIsShownWhenAnyGroupIs();
    TypedFieldsRetainSourceResetSemanticsWithoutRtti();
    RenderOptionsNarrowCopiesAndReachCustomFields();
}
