/* Size-focused implementation of markdown/markdown.h.

   The public types live in markdown/ so the full and mini parsers produce the
   exact same mdast. A build selects the implementation with
   GPUI_MARKDOWN=full|mini; including this header is equivalent to including
   markdown/markdown.h. */

#ifndef GPUI_MARKDOWN_MINI_MARKDOWN_H_
#define GPUI_MARKDOWN_MINI_MARKDOWN_H_

#include "markdown/markdown.h"

#endif // GPUI_MARKDOWN_MINI_MARKDOWN_H_
