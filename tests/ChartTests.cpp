/* crates/ui/src/chart/{radar,sankey}_chart.rs: public label values. */

#include "Test.h"

using namespace gpui::component;

static bool ChartColorEq(Rgba a, Rgba b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static void RadarLabelsRetainTextAndElements() {
    RadarLabel text = RadarLabel::Text(StrL("Sales"));
    El element = {};
    RadarLabel custom = RadarLabel::Element(&element);
    utassert(text.kind == RadarLabel::Kind::Text);
    utassert(base::StrEq(text.text, StrL("Sales")));
    utassert(text.element == nullptr);
    utassert(custom.kind == RadarLabel::Kind::Element);
    utassert(custom.element == &element);
    utassert(!custom.text.s);

    App app = {};
    component::Init(&app);
    Arena* a = ArenaNew();
    Ctx cx = {};
    cx.a = a;
    cx.app = &app;
    float values[3] = {1, 2, 3};
    El* labelElement =
        Div(a)->FlexCol()->Child(TextEl(a, StrL("custom label")));
    RadarLabel labels[3] = {RadarLabel::Text(StrL("one")),
                            RadarLabel::Element(labelElement),
                            RadarLabel::Text(StrL("three"))};
    Rgba red = RgbaHex(0xff0000);
    RadarChart* chart = RadarChart::New(&cx, values, 3)
                            ->Labels(labels)
                            ->LabelColor(red)
                            ->LabelGap(17)
                            ->GridLevels(0);
    El* root = chart->IntoEl();
    utassert(chart->labels == labels);
    utassert(chart->hasLabelColor);
    utassert(ChartColorEq(chart->labelColor, red));
    utassertnear(chart->labelGap, 17);
    utassert(chart->gridLevels == 1);
    utassert(root->customPaint != nullptr);
    utassert(root->customUser == chart);
    utassert(root->first == labelElement);
    utassert(labelElement->style.absolute);

    AppGlobalClear(&app);
    ArenaDelete(a);
}

static void PlainRadarLabelsProjectToTheTaggedValue() {
    App app = {};
    component::Init(&app);
    Arena* a = ArenaNew();
    Ctx cx = {};
    cx.a = a;
    cx.app = &app;
    float values[3] = {1, 2, 3};
    const char* names[3] = {"one", "two", "three"};
    RadarChart* chart = RadarChart::New(&cx, values, 3)->Labels(names);
    utassert(chart->labels != nullptr);
    for (int i = 0; i < 3; i++) {
        utassert(chart->labels[i].kind == RadarLabel::Kind::Text);
        utassert(base::StrEq(chart->labels[i].text, names[i]));
    }

    AppGlobalClear(&app);
    ArenaDelete(a);
}

static void SankeyLabelsCarryIndependentStylesAndDoNotCap() {
    Rgba red = RgbaHex(0xff0000);
    SankeyLabel plain = SankeyLabel::New(StrL("a"));
    SankeyLabel styled = SankeyLabel::New(StrL("b")).Color(red).FontSize(14);
    utassert(base::StrEq(plain.text, StrL("a")));
    utassert(!plain.hasColor);
    utassert(plain.fontSize == 0);
    // plot/label.rs: TEXT_SIZE 10 + TEXT_GAP 2.
    utassertnear(plain.LineHeight(), 12);
    utassert(styled.hasColor);
    utassert(ChartColorEq(styled.color, red));
    utassertnear(styled.LineHeight(), 16);

    App app = {};
    component::Init(&app);
    Arena* a = ArenaNew();
    Ctx cx = {};
    cx.a = a;
    cx.app = &app;
    SankeyChart* chart = SankeyChart::New(&cx)->Node(StrL("ignored"));
    for (int i = 0; i < 40; i++) {
        chart->CustomLabel(i & 1 ? styled : plain);
    }
    const SankeyChartNode& node = chart->nodes[0];
    utassert(node.hasCustomLabels);
    utassert(node.labels.len == 40);
    utassert(base::StrEq(node.labels[0].text, StrL("a")));
    utassert(base::StrEq(node.labels[39].text, StrL("b")));
    utassertnear(node.labels[39].fontSize, 14);

    AppGlobalClear(&app);
    ArenaDelete(a);
}

void TestChart() {
    TestSuite("chart labels");
    RadarLabelsRetainTextAndElements();
    PlainRadarLabelsProjectToTheTaggedValue();
    SankeyLabelsCarryIndependentStylesAndDoNotCap();
}
