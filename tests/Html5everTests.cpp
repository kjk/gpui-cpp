/* End-to-end tests for the public surface shared by src/html5ever and
   src/html5ever-mini. The full-only cases pin the HTML5 tree-construction
   rules which are deliberately outside the mini contract. */

#define GPUI_INCLUDE_PRIVATE_API 1
#include "Test.h"

using namespace html5ever;

static Node* ElementChild(Node* parent, Str name, int occurrence = 0) {
    for (Node* node = parent ? parent->first : nullptr; node;
         node = node->next) {
        if (node->kind == NodeKind::Element && base::StrEqI(node->name, name)) {
            if (occurrence-- == 0) return node;
        }
    }
    return nullptr;
}

static void TestSharedSurface(Arena* a) {
    Node* doc =
        ParseFragment(a, StrL("<p id='a&amp;b'>one<br>two &lt; three</p>"));
    Node* p = ElementChild(doc, StrL("p"));
    utassert(p != nullptr);
    utassert(base::StrEq(AttrValue(p, StrL("id")), "a&b"));
    utassert(p->first && base::StrEq(p->first->data, "one"));
    utassert(ElementChild(p, StrL("br")) != nullptr);

    Str serialized = Serialize(a, doc);
    utassert(base::StrContains(serialized, StrL("<p id=\"a&amp;b\">")));
    utassert(base::StrContains(serialized, StrL("two &lt; three")));
}

#if GPUI_HTML5EVER_FULL

static Node* DocumentBody(Node* doc) {
    Node* html = ElementChild(doc, StrL("html"));
    return ElementChild(html, StrL("body"));
}

static void TestDocumentAndImpliedEnds(Arena* a) {
    Node* doc = ParseDocument(a, StrL("<!doctype html><p>one<p>two"));
    utassert(doc->first && doc->first->kind == NodeKind::Doctype);
    Node* body = DocumentBody(doc);
    utassert(body != nullptr);
    Node* first = ElementChild(body, StrL("p"));
    Node* second = ElementChild(body, StrL("p"), 1);
    utassert(first && second);
    utassert(first->first && base::StrEq(first->first->data, "one"));
    utassert(second->first && base::StrEq(second->first->data, "two"));
}

static void TestTableModes(Arena* a) {
    Node* doc =
        ParseDocument(a, StrL("<table>before<tr><td>x<td>y</table>after"));
    Node* body = DocumentBody(doc);
    utassert(body && body->first && body->first->kind == NodeKind::Text);
    utassert(base::StrEq(body->first->data, "before"));
    Node* table = ElementChild(body, StrL("table"));
    Node* tbody = ElementChild(table, StrL("tbody"));
    utassert(tbody && tbody->implicit);
    Node* row = ElementChild(tbody, StrL("tr"));
    utassert(row != nullptr);
    utassert(ElementChild(row, StrL("td")) != nullptr);
}

static void TestForeignContent(Arena* a) {
    Node* doc =
        ParseDocument(a, StrL("<svg><g></g></svg><math><mi>x</mi></math>"));
    Node* body = DocumentBody(doc);
    Node* svg = ElementChild(body, StrL("svg"));
    Node* math = ElementChild(body, StrL("math"));
    utassert(svg && svg->ns == Namespace::Svg);
    utassert(ElementChild(svg, StrL("g"))->ns == Namespace::Svg);
    utassert(math && math->ns == Namespace::MathMl);
}

#endif

void TestHtml5ever() {
    TestSuite("html5ever");
    Arena* a = ArenaNew();
    TestSharedSurface(a);
#if GPUI_HTML5EVER_FULL
    TestDocumentAndImpliedEnds(a);
    TestTableModes(a);
    TestForeignContent(a);
#endif
    ArenaDelete(a);
}
