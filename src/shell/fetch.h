#ifndef GPUI_SHELL_FETCH_H_
#define GPUI_SHELL_FETCH_H_

#include "shell/capability.h"
#include "sys/http.h"

namespace gpui::shell {

constexpr int kFetchMaxBody = 8 * 1024 * 1024;
constexpr int kFetchMaxRedirects = 10;

struct FetchResult {
    int status = 0;
    Str url;
    Str body;
    Str error;

    void Free();
};

bool FetchAuthorizeGet(Str url, const Capabilities& capabilities,
                       Str* error = nullptr);
bool FetchGet(Str url, const Capabilities& capabilities, FetchResult* out);

using FetchHttpGet = bool (*)(Str url, HttpRsp* out);
void FetchSetHttpGetForTests(FetchHttpGet get);

} // namespace gpui::shell
#endif // GPUI_SHELL_FETCH_H_
