/* App::global<T> ownership and isolation. */

#include "Test.h"

struct TestGlobalValue {
    static int destroyed;
    int value = 0;

    ~TestGlobalValue() {
        destroyed++;
    }
};

int TestGlobalValue::destroyed = 0;

struct OtherGlobalValue {
    int value = 0;
};

static void GlobalsBelongToOneApp() {
    App first;
    App second;
    TestGlobalValue* a = AppGlobalEnsure<TestGlobalValue>(&first);
    TestGlobalValue* b = AppGlobalEnsure<TestGlobalValue>(&second);
    utassert(a && b && a != b);
    a->value = 7;
    b->value = 11;
    utassert(AppGlobalGet<TestGlobalValue>(&first)->value == 7);
    utassert(AppGlobalGet<TestGlobalValue>(&second)->value == 11);
    utassert(AppGlobalEnsure<TestGlobalValue>(&first) == a);

    OtherGlobalValue* other = AppGlobalEnsure<OtherGlobalValue>(&first);
    other->value = 13;
    utassert(AppGlobalGet<OtherGlobalValue>(&first)->value == 13);
    utassert(AppGlobalGet<OtherGlobalValue>(&second) == nullptr);

    AppGlobalClear(&first);
    AppGlobalClear(&second);
}

static void BaseInitializationIsIdempotentAndIsolated() {
    App first;
    App second;
    BaseInit(&first);
    BaseInit(&first);
    BaseInit(&second);
    utassert(BaseThemeGlobal(&first) != nullptr);
    utassert(BaseThemeGlobal(&second) != nullptr);
    utassert(BaseThemeGlobal(&first) != BaseThemeGlobal(&second));
    utassert(BaseThemeGlobal(&first)->scrollbar.mode ==
             ScrollbarMode::Scrolling);
    utassert(BaseThemeGlobal(&first)->scrollbar.motion.enter == 0);

    BaseSuppressTextSelection(&first);
    utassert(BaseIsTextSelectionSuppressed(&first));
    utassert(!BaseIsTextSelectionSuppressed(&second));
    BaseResetTextSelectionSuppression(&first);
    utassert(!BaseIsTextSelectionSuppressed(&first));

    AppGlobalClear(&first);
    AppGlobalClear(&second);
}

static void UiInitializationIsIdempotentAndIsolated() {
    App first;
    App second;
    component::UiGlobalStateInit(&first);
    component::UiGlobalStateInit(&first);
    component::UiGlobalStateInit(&second);
    utassert(component::UiSelectionNextDocumentOrder(&first) == 1);
    utassert(component::UiSelectionNextDocumentOrder(&first) == 2);
    utassert(component::UiSelectionNextDocumentOrder(&second) == 1);

    EntityId view = {7, 3};
    component::UiTextViewStatePush(&first, view);
    utassert(component::UiTextViewStateCurrent(&first) == view);
    utassert(!component::UiTextViewStateCurrent(&second).IsValid());
    component::UiTextViewStatePop(&first);
    utassert(!component::UiTextViewStateCurrent(&first).IsValid());

    AppGlobalClear(&first);
    AppGlobalClear(&second);
}

static void ReplacingAndRemovingReleaseOwnership() {
    App app;
    TestGlobalValue::destroyed = 0;
    TestGlobalValue* first = new TestGlobalValue();
    AppGlobalSetRaw(&app, AppGlobalKey<TestGlobalValue>(), first,
                    &AppGlobalDelete<TestGlobalValue>);
    TestGlobalValue* second = new TestGlobalValue();
    AppGlobalSetRaw(&app, AppGlobalKey<TestGlobalValue>(), second,
                    &AppGlobalDelete<TestGlobalValue>);
    utassert(TestGlobalValue::destroyed == 1);
    utassert(AppGlobalRemove<TestGlobalValue>(&app));
    utassert(TestGlobalValue::destroyed == 2);
    utassert(!AppGlobalRemove<TestGlobalValue>(&app));
    utassert(AppGlobalGet<TestGlobalValue>(&app) == nullptr);
    AppGlobalClear(&app);
}

void TestAppGlobals() {
    TestSuite("app globals");
    GlobalsBelongToOneApp();
    ReplacingAndRemovingReleaseOwnership();
    BaseInitializationIsIdempotentAndIsolated();
    UiInitializationIsIdempotentAndIsolated();
}
