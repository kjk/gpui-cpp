/* Source-shaped LSP contracts from crates/base/src/input/editor/lsp.

   Rust's providers return Task<Result<T>>. Async tasks are outside this
   runtime's standing scope, so these trait objects are explicit function
   tables over the synchronous provider callbacks InputState already runs.
   The data, ownership, limits, menus, and cache/update boundaries remain the
   same. */

#include "base/input_editor.h"

namespace gpui {

struct CompletionMenuOptions {
    float maxWidth = kCompletionMenuMaxW;

    static CompletionMenuOptions Default() { return {}; }
};

struct CompletionProvider {
    void* data = nullptr;
    CompletionFn completions = nullptr;
    InlineCompletionFn inlineCompletion = nullptr;
    CompletionTriggerFn isCompletionTrigger = nullptr;
    CompletionResolveFn resolveCompletions = nullptr;
    float inlineCompletionDebounceMs = kInlineCompletionDebounceMs;

    bool IsValid() const { return completions != nullptr; }
    void Install(InputState* state,
                 CompletionMenuOptions options = {}) const;
};

struct CodeActionProvider {
    void* data = nullptr;
    Str (*id)(void* data) = nullptr;
    CodeActionFn codeActions = nullptr;
    CodeActionPerformFn performCodeAction = nullptr;

    bool IsValid() const { return codeActions != nullptr; }
    Str Id() const { return id ? id(data) : Str{}; }
    void Install(InputState* state) const;
};

struct DefinitionProvider {
    void* data = nullptr;
    DefinitionFn definitions = nullptr;

    bool IsValid() const { return definitions != nullptr; }
    void Install(InputState* state) const;
};

struct DocumentColorProvider {
    void* data = nullptr;
    DocumentColorFn documentColors = nullptr;

    bool IsValid() const { return documentColors != nullptr; }
    void Install(InputState* state) const;
};

struct HoverProvider {
    void* data = nullptr;
    HoverFn hover = nullptr;

    bool IsValid() const { return hover != nullptr; }
    void Install(InputState* state) const;
};

struct DocumentRangeSemanticTokensProvider {
    void* data = nullptr;
    const Str* legend = nullptr;
    int nLegend = 0;
    SemanticTokensFn semanticTokens = nullptr;

    bool IsValid() const { return semanticTokens != nullptr; }
    void Install(InputState* state) const;
};

// Rc<dyn Fn> in Rust: function plus explicit capture here.
struct ShowDocumentHandler {
    void* data = nullptr;
    ShowDocumentFn show = nullptr;

    bool IsValid() const { return show != nullptr; }
    bool Show(Str uri, bool external, Selection selection) const;
    void Install(InputState* state) const;
};

// Borrowed views of the retained overlay records. `Of` is the equivalent of
// Rust returning `&CompletionMenuState` / `&CodeActionMenuState`.
struct CompletionMenuState {
    bool open = false;
    int triggerStartOffset = -1;
    Str query = {};
    const CompletionItem* items = nullptr;
    int nItems = 0;
    int selected = 0;
    uint64_t revision = 0;

    static CompletionMenuState Of(const InputState* state);
    bool IsOpen() const { return open; }
    uint64_t Revision() const { return revision; }
};

struct CodeActionMenuState {
    bool open = false;
    const CodeActionItem* items = nullptr;
    int nItems = 0;
    int selected = 0;
    uint64_t revision = 0;

    static CodeActionMenuState Of(const InputState* state);
    bool IsOpen() const { return open; }
    uint64_t Revision() const { return revision; }
};

struct HoverPopoverState {
    Selection symbolRange = {};
    Str hover = {};
    bool open = false;

    static HoverPopoverState Of(const InputState* state);
};

// ServerCapabilities. Provider records are retained here and installed into
// the flattened InputState in one operation, instead of requiring callers to
// populate runtime fields piecemeal.
struct Lsp {
    InputState* state = nullptr;
    CompletionProvider completionProvider = {};
    Vec<CodeActionProvider> codeActionProviders;
    HoverProvider hoverProvider = {};
    DefinitionProvider definitionProvider = {};
    DocumentColorProvider documentColorProvider = {};
    DocumentRangeSemanticTokensProvider semanticTokensProvider = {};
    ShowDocumentHandler showDocument = {};
    CompletionMenuOptions completionMenu = {};

    Lsp() = default;
    Lsp(const Lsp&) = delete;
    Lsp& operator=(const Lsp&) = delete;

    Lsp& Completion(const CompletionProvider& provider);
    Lsp& AddCodeAction(const CodeActionProvider& provider);
    Lsp& Hover(const HoverProvider& provider);
    Lsp& Definition(const DefinitionProvider& provider);
    Lsp& DocumentColors(const DocumentColorProvider& provider);
    Lsp& SemanticTokens(
        const DocumentRangeSemanticTokensProvider& provider);
    Lsp& ShowDocuments(const ShowDocumentHandler& handler);
    Lsp& CompletionMenu(const CompletionMenuOptions& options);
    void Install(InputState* input);
    void Update();
    void Reset();
    int DocumentColorsForRange(Selection visible, DocumentColor* out,
                               int cap) const;
    int SemanticTokensForRange(Selection visible,
                               const HighlightStyleResolver& resolver,
                               TextSpan* out, int cap) const;
};

} // namespace gpui
