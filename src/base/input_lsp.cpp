/* crates/base/src/input/editor/lsp provider and overlay facades. */

#include "base/input_lsp.h"

namespace gpui {

void CompletionProvider::Install(InputState* state,
                                 CompletionMenuOptions options) const {
    if (!state) {
        return;
    }
    state->completionProvider = completions;
    state->completionData = data;
    state->inlineCompletionProvider = inlineCompletion;
    state->inlineCompletionData = data;
    state->completionTrigger = isCompletionTrigger;
    state->completionResolve = resolveCompletions;
    state->inlineCompletionDebounceMs =
        std::max(0.f, inlineCompletionDebounceMs);
    state->completionMenuMaxW = std::max(0.f, options.maxWidth);
}

void CodeActionProvider::Install(InputState* state) const {
    if (state && codeActions) {
        InputAddCodeActionProvider(state, codeActions, data,
                                   performCodeAction);
    }
}

void DefinitionProvider::Install(InputState* state) const {
    if (state) {
        state->definitionProvider = definitions;
        state->definitionData = data;
    }
}

void DocumentColorProvider::Install(InputState* state) const {
    if (state) {
        state->documentColorProvider = documentColors;
        state->documentColorData = data;
        state->documentColorsDirty = true;
    }
}

void HoverProvider::Install(InputState* state) const {
    if (state) {
        state->hoverProvider = hover;
        state->hoverData = data;
    }
}

void DocumentRangeSemanticTokensProvider::Install(InputState* state) const {
    if (state) {
        state->semanticTokensProvider = semanticTokens;
        state->semanticTokensData = data;
        state->semanticLegend = legend;
        state->nSemanticLegend = std::max(0, nLegend);
        state->semanticTokensDirty = true;
    }
}

bool ShowDocumentHandler::Show(Str uri, bool external,
                               Selection selection) const {
    return show && show(data, uri, external, selection);
}

void ShowDocumentHandler::Install(InputState* state) const {
    if (state) {
        state->showDocument = show;
        state->showDocumentData = data;
    }
}

CompletionMenuState CompletionMenuState::Of(const InputState* state) {
    CompletionMenuState result;
    if (!state) {
        return result;
    }
    result.open = state->completion.open;
    result.triggerStartOffset = state->completion.triggerStart;
    result.query = state->completion.query;
    result.items = state->completion.items.els;
    result.nItems = state->completion.items.len;
    result.selected = state->completion.selected;
    result.revision = state->completion.revision;
    return result;
}

CodeActionMenuState CodeActionMenuState::Of(const InputState* state) {
    CodeActionMenuState result;
    if (!state) {
        return result;
    }
    result.open = state->codeActions.open;
    result.items = state->codeActions.items.els;
    result.nItems = state->codeActions.items.len;
    result.selected = state->codeActions.selected;
    result.revision = state->codeActions.revision;
    return result;
}

HoverPopoverState HoverPopoverState::Of(const InputState* state) {
    HoverPopoverState result;
    if (!state) {
        return result;
    }
    result.symbolRange = state->hoverRange;
    result.hover = state->hoverText;
    result.open = state->hoverText.len > 0;
    return result;
}

Lsp& Lsp::Completion(const CompletionProvider& provider) {
    completionProvider = provider;
    return *this;
}

Lsp& Lsp::AddCodeAction(const CodeActionProvider& provider) {
    if (provider.IsValid()) {
        VecAppend(codeActionProviders, provider);
    }
    return *this;
}

Lsp& Lsp::Hover(const gpui::HoverProvider& provider) {
    hoverProvider = provider;
    return *this;
}

Lsp& Lsp::Definition(const DefinitionProvider& provider) {
    definitionProvider = provider;
    return *this;
}

Lsp& Lsp::DocumentColors(const DocumentColorProvider& provider) {
    documentColorProvider = provider;
    return *this;
}

Lsp& Lsp::SemanticTokens(
    const DocumentRangeSemanticTokensProvider& provider) {
    semanticTokensProvider = provider;
    return *this;
}

Lsp& Lsp::ShowDocuments(const ShowDocumentHandler& handler) {
    showDocument = handler;
    return *this;
}

Lsp& Lsp::CompletionMenu(const CompletionMenuOptions& options) {
    completionMenu = options;
    return *this;
}

void Lsp::Install(InputState* input) {
    state = input;
    if (!state) {
        return;
    }
    // Reinstalling capabilities replaces the server's provider list.
    VecClear(state->codeActionProviders);
    state->codeActionProvider = nullptr;
    state->codeActionData = nullptr;
    completionProvider.Install(state, completionMenu);
    hoverProvider.Install(state);
    definitionProvider.Install(state);
    documentColorProvider.Install(state);
    semanticTokensProvider.Install(state);
    showDocument.Install(state);
    for (int i = 0; i < codeActionProviders.len; i++) {
        codeActionProviders[i].Install(state);
    }
}

void Lsp::Update() {
    InputLspUpdate(state);
}

void Lsp::Reset() {
    InputLspReset(state);
}

int Lsp::DocumentColorsForRange(Selection visible, DocumentColor* out,
                                int cap) const {
    if (!state) {
        return 0;
    }
    int total = 0;
    for (int i = 0; i < state->documentColors.len; i++) {
        const DocumentColor& color = state->documentColors[i];
        if (color.range.start >= visible.end || color.range.end <= visible.start ||
            color.range.start >= color.range.end) {
            continue;
        }
        if (out && total < cap) {
            out[total] = color;
        }
        total++;
    }
    return total;
}

int Lsp::SemanticTokensForRange(
    Selection visible, const HighlightStyleResolver& resolver, TextSpan* out,
    int cap) const {
    if (!state || state->semanticTokens.len <= 0) {
        return 0;
    }
    Vec<SemanticRange> ranges;
    if (!VecReserve(ranges, state->semanticTokens.len)) {
        return 0;
    }
    int n = gpui::SemanticTokensForRange(
        state->semanticTokens.els, state->semanticTokens.len, InputValue(state),
        visible, ranges.els, ranges.cap);
    int total = 0;
    for (int i = 0; i < n; i++) {
        TextSpan span;
        if (!resolver.Style(ranges[i].name, &span)) {
            continue;
        }
        span.lo = ranges[i].range.start;
        span.hi = ranges[i].range.end;
        if (out && total < cap) {
            out[total] = span;
        }
        total++;
    }
    return total;
}

} // namespace gpui
