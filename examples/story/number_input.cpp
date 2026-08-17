#include "Story.h"

static void IncNum(StoryApp* app) {
    int v = 0;
    sscanf_s(app->field.buf, "%d", &v);
    v++;
    _snprintf_s(app->field.buf, _TRUNCATE, "%d", v);
    app->field.len = (int)strlen(app->field.buf);
}
static void DecNum(StoryApp* app) {
    int v = 0;
    sscanf_s(app->field.buf, "%d", &v);
    v--;
    _snprintf_s(app->field.buf, _TRUNCATE, "%d", v);
    app->field.len = (int)strlen(app->field.buf);
}

El* NumberInputRender(StoryApp* app, Arena* a) {
    if (app->field.len == 0 || app->field.buf[0] < '0' || app->field.buf[0] > '9') {
        strncpy_s(app->field.buf, "12", _TRUNCATE);
        app->field.len = 2;
    }
    El* page = Div(a)->FlexCol()->Gap(24)->W(kFill);
    El* sec = StorySection(a, "Default", "Numeric input with increment and decrement controls.");
    StorySectionAdd(sec, component::NumberInput::New(a, &app->field)
                             ->OnInc(MkFunc0(&IncNum, app))
                             ->OnDec(MkFunc0(&DecNum, app))
                             ->IntoEl());
    page->Child(sec);
    return page;
}

void NumberInputClick(StoryApp* app, int id) {
    (void)app;
    (void)id;
}

STORY_PAGE(StoryNumberInput, NumberInputRender, NumberInputClick);
