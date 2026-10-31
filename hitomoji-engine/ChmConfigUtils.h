#pragma once
// Copyright (C) 2026 by Hitomoji Project. All rights reserved.
#include <string>

namespace ChmStringUtil {
    std::wstring Trim(const std::wstring& s);
    std::wstring Canonize(const std::wstring& s);
}

namespace ChmConfigParse {
    // Only INI syntax characters are escaped. Other backslashes remain literal.
    size_t FindUnescaped(const std::wstring& s, const std::wstring& characters);
    std::wstring Unescape(const std::wstring& s);

    enum class Level {
        None,
        Info,
        Error
    };

    struct Result {
        Level level = Level::None;
        std::wstring message;
    };

    void SetError(Result& r, const std::wstring& msg);
    void SetInfo(Result& r, const std::wstring& msg);
}
