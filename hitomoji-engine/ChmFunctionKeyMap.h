#pragma once
// Copyright (C) 2026 by Hitomoji Project. All rights reserved.
#include <windows.h>
#include <map>
#include <optional>
#include <string>
#include "hitomoji.h"
#include "ChmEngine.h"
#include "ChmConfigUtils.h"

class ChmFunctionKeyMap {
public:
    struct KeyChord {
        UINT vk;
        bool shift;
        bool ctrl;
        bool alt;
    };

    using ParseLevel = ChmConfigParse::Level;
    using ParseResult = ChmConfigParse::Result;

    ChmFunctionKeyMap();

    void Clear();
    void InitDefault();
    // 読み込みに成功した定義だけを上書きでマージする。
    void MergeFrom(const ChmFunctionKeyMap& definitions);
    BOOL ParseFunctionKey(const std::wstring& key,
                          const std::wstring& value,
                          ParseResult& result);
    ChmFuncType Resolve(WPARAM wp,
                        bool shift,
                        bool ctrl,
                        bool alt,
                        ChmEngine::State state) const;
    bool IsUnFinishKey(WPARAM wp,
                       bool shift,
                       bool ctrl,
                       bool alt) const;
    std::wstring Dump() const;
    KeyChord GetImeToggleKey() const { return _imeToggleKey.value_or(KeyChord{VK_SPACE, false, true, false}); }

private:
    struct KeySignature {
        WPARAM wp;
        bool shift;
        bool ctrl;
        bool alt;
        ChmEngine::State state;

        bool operator<(const KeySignature& rhs) const;
    };
    static bool ResolveActionName(const std::wstring& name, ChmFuncType& outType);
    static bool ResolveKeyName(const std::wstring& name, UINT& outVk);
    void SetBinding(const KeySignature& key, ChmFuncType type);

    std::map<KeySignature, ChmFuncType> _keyTable;
    std::optional<KeyChord> _imeToggleKey;
};
