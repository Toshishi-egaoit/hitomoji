#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <tuple>
#include <vector>
#include "ChmFunctionKeyMap.h"

namespace {

struct FuncKeyDef {
    WPARAM wp;
    bool needShift;
    bool needCtrl;
    bool needAlt;
    ChmEngine::State state;
    ChmFuncType type;
};

const FuncKeyDef g_functionKeyTable[] = {
    // WPARAM      SHIFT  CTRL   ALT    State                        Type
    { VK_SPACE,    false, false, false, ChmEngine::State::None,      ChmFuncType::CharInputSpace },
    { VK_RETURN,   false, false, false, ChmEngine::State::Inputing,  ChmFuncType::CompFinish     },
    { VK_RETURN,   false, false, true,  ChmEngine::State::Inputing,  ChmFuncType::CompFinishHiragana },
    { VK_RETURN,   true,  false, false, ChmEngine::State::Inputing,  ChmFuncType::CompFinishKatakana },
    { VK_TAB,      false, false, false, ChmEngine::State::Inputing,  ChmFuncType::CompFinishKey  },
    { VK_TAB,      true,  false, false, ChmEngine::State::Inputing,  ChmFuncType::CompFinishKeyWide},
    { VK_SPACE,    false, false, false, ChmEngine::State::Inputing,  ChmFuncType::CompSelect     },
    { VK_BACK,     false, false, false, ChmEngine::State::Inputing,  ChmFuncType::Backspace      },
    { VK_ESCAPE,   false, false, false, ChmEngine::State::Inputing,  ChmFuncType::Cancel         },
    { VK_SPACE,    false, false, false, ChmEngine::State::Selecting, ChmFuncType::SelectNextPage },
    { VK_BACK,     false, false, false, ChmEngine::State::Selecting, ChmFuncType::SelectPrevPage },
    { VK_ESCAPE,   false, false, false, ChmEngine::State::Selecting, ChmFuncType::SelectCancel   },
    // CTRL+*
    { 'Z',         false, true,  false, ChmEngine::State::None,      ChmFuncType::UnFinish       },
    { 'H',         false, true,  false, ChmEngine::State::Selecting, ChmFuncType::Backspace      },
    { 'I',         false, true,  false, ChmEngine::State::Selecting, ChmFuncType::CompFinishKey  },
    { 'I',         true,  true,  false, ChmEngine::State::Selecting, ChmFuncType::CompFinishKeyWide},
    { 'M',         false, true,  false, ChmEngine::State::Selecting, ChmFuncType::CompFinish     },
#ifdef _DEBUG
    { 'V',         true,  true,  false, ChmEngine::State::None,      ChmFuncType::VersionInfo    },
    { 'R',         true,  true,  false, ChmEngine::State::None,      ChmFuncType::ReloadIni      },
#endif
};

const std::map<std::wstring, ChmFuncType> s_actionNameMap = {
    { L"finish",             ChmFuncType::CompFinish },
    { L"finish-hiragana",    ChmFuncType::CompFinishHiragana },
    { L"finish-katakana",    ChmFuncType::CompFinishKatakana },
    { L"finish-raw",         ChmFuncType::CompFinishKey },
    { L"finish-raw-wide",    ChmFuncType::CompFinishKeyWide },
    { L"backspace",          ChmFuncType::Backspace },
    { L"cancel",             ChmFuncType::Cancel },
    { L"cancel-finish",      ChmFuncType::UnFinish },
    { L"select-start",       ChmFuncType::CompSelect },
    { L"next-page" ,         ChmFuncType::SelectNextPage },
    { L"prev-page" ,         ChmFuncType::SelectPrevPage },
    { L"cancel-select" ,     ChmFuncType::SelectCancel },
    { L"select-kanji" ,      ChmFuncType::SelectInput },
    { L"space-char",         ChmFuncType::CharInputSpace },
#ifdef _DEBUG
    { L"version-info",       ChmFuncType::VersionInfo },
    { L"reload-ini",         ChmFuncType::ReloadIni },
#endif
};

const std::map<std::wstring, UINT> s_keyNameMap = {
    { L"enter",    VK_RETURN },
    { L"return",   VK_RETURN },
    { L"tab",      VK_TAB },
    { L"backspace",VK_BACK },
    { L"esc",      VK_ESCAPE },
    { L"escape",   VK_ESCAPE },
    { L"space",    VK_SPACE },
    { L"left",     VK_LEFT },
    { L"right",    VK_RIGHT },
    { L"up",       VK_UP },
    { L"down",     VK_DOWN },
    { L"home",     VK_HOME },
    { L"end",      VK_END },
    { L"pageup",   VK_PRIOR },
    { L"pagedown", VK_NEXT },
    { L"del",      VK_DELETE },
    { L"delete",   VK_DELETE },
    { L"ins",      VK_INSERT },
    { L"insert",   VK_INSERT },
    { L"f1",  VK_F1 }, { L"f2",  VK_F2 }, { L"f3",  VK_F3 },
    { L"f4",  VK_F4 }, { L"f5",  VK_F5 }, { L"f6",  VK_F6 },
    { L"f7",  VK_F7 }, { L"f8",  VK_F8 }, { L"f9",  VK_F9 },
    { L"f10", VK_F10 },{ L"f11", VK_F11 },{ L"f12", VK_F12 },
};

} // namespace

bool ChmFunctionKeyMap::KeySignature::operator<(const KeySignature& rhs) const
{
    return std::tie(wp, shift, ctrl, alt, state)
         < std::tie(rhs.wp, rhs.shift, rhs.ctrl, rhs.alt, rhs.state);
}

ChmFunctionKeyMap::ChmFunctionKeyMap()
{
    InitDefault();
}

void ChmFunctionKeyMap::Clear()
{
    _keyTable.clear();
}

void ChmFunctionKeyMap::InitDefault()
{
    Clear();
    for (const auto& k : g_functionKeyTable)
    {
        KeySignature sig{ k.wp, k.needShift, k.needCtrl, k.needAlt, k.state };
        _keyTable[sig] = k.type;
    }
}

bool ChmFunctionKeyMap::ResolveActionName(const std::wstring& name, ChmFuncType& outType)
{
    auto it = s_actionNameMap.find(name);
    if (it != s_actionNameMap.end())
    {
        outType = it->second;
        return true;
    }
    return false;
}

bool ChmFunctionKeyMap::ResolveKeyName(const std::wstring& name, UINT& outVk)
{
    auto it = s_keyNameMap.find(name);
    if (it != s_keyNameMap.end())
    {
        outVk = it->second;
        return true;
    }
    return false;
}

BOOL ChmFunctionKeyMap::ParseFunctionKey(const std::wstring& key,
                                         const std::wstring& value,
                                         ParseResult& result)
{
    std::wstring keySpec = ChmStringUtil::Canonize(key);
    if (keySpec.empty())
    {
        ChmConfigParse::SetError(result, L"invalid function-key definition");
        return FALSE;
    }

    std::wstring actionName = ChmStringUtil::Canonize(value);
    ChmFuncType actionType;
    if (!ResolveActionName(actionName, actionType))
    {
        ChmConfigParse::SetError(result, L"unknown function-key action: " + actionName);
        return FALSE;
    }

    std::vector<std::wstring> tokens;
    size_t start = 0;
    while (start <= keySpec.size())
    {
        size_t pos = keySpec.find(L'+', start);
        std::wstring token;
        if (pos == std::wstring::npos)
        {
            token = keySpec.substr(start);
            start = keySpec.size() + 1;
        }
        else
        {
            token = keySpec.substr(start, pos - start);
            start = pos + 1;
        }
        token = ChmStringUtil::Trim(token);
        std::transform(token.begin(), token.end(), token.begin(),
            [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
        if (!token.empty())
            tokens.push_back(token);
    }

    if (tokens.empty())
    {
        ChmConfigParse::SetError(result, L"empty key definition");
        return FALSE;
    }

    bool needShift = false;
    bool needCtrl = false;
    bool needAlt = false;

    if (tokens.size() > 1)
    {
        for (size_t i = 0; i < tokens.size() - 1; ++i)
        {
            const std::wstring& t = tokens[i];
            if (t == L"shift") needShift = true;
            else if (t == L"ctrl" || t == L"control") needCtrl = true;
            else if (t == L"alt") needAlt = true;
            else
            {
                ChmConfigParse::SetError(result, L"invalid modifier: " + t);
                return FALSE;
            }
        }
    }

    const std::wstring& keyToken = tokens.back();
    UINT vk = 0;

    if (ResolveKeyName(keyToken, vk))
    {
        // OK
    }
    else if (keyToken.length() == 1)
    {
        wchar_t ch = keyToken[0];
        if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z'))
        {
            vk = static_cast<UINT>(towupper(ch));
        }
        else if (ch >= L'0' && ch <= L'9')
        {
            vk = static_cast<UINT>(ch);
        }
        if (vk != 0 && !needCtrl && !needAlt)
        {
            ChmConfigParse::SetError(result, L"alphabet/digit requires CTRL or ALT modifier");
            return FALSE;
        }
    }
    if (vk == 0)
    {
        ChmConfigParse::SetError(result, L"invalid key name: " + keyToken);
        return FALSE;
    }

    ChmEngine::State state = actionType == ChmFuncType::UnFinish
        ? ChmEngine::State::None
        : ChmEngine::State::Inputing;
    KeySignature sig{ vk, needShift, needCtrl, needAlt, state };

    auto it = _keyTable.find(sig);
    if (it != _keyTable.end())
    {
        ChmConfigParse::SetInfo(result, L"duplicate function-key definition: " + keySpec);
    }

    _keyTable[sig] = actionType;

    return TRUE;
}

ChmFuncType ChmFunctionKeyMap::Resolve(WPARAM wp,
                                       bool shift,
                                       bool ctrl,
                                       bool alt,
                                       ChmEngine::State state) const
{
    KeySignature sig{ wp, shift, ctrl, alt, state };
    auto it = _keyTable.find(sig);
    if (it != _keyTable.end())
    {
        return it->second;
    }
    return ChmFuncType::None;
}

bool ChmFunctionKeyMap::IsUnFinishKey(WPARAM wp,
                                      bool shift,
                                      bool ctrl,
                                      bool alt) const
{
    return Resolve(wp, shift, ctrl, alt, ChmEngine::State::None) == ChmFuncType::UnFinish;
}

std::wstring ChmFunctionKeyMap::Dump() const
{
    std::wstring result;
    for (const auto& pair : _keyTable) {
        std::wstring keyName = L"?";
        {
            unsigned char ch = static_cast<unsigned char>(pair.first.wp);
            if ((ch >= L'A' && ch <= L'Z') ||
                (ch >= L'0' && ch <= L'9'))
            {
                keyName = std::wstring(1, ch);
            }
            else {
                for (const auto& p : s_keyNameMap)
                {
                    if (p.second == ch)
                    {
                        keyName = p.first;
                        std::transform(keyName.begin(), keyName.end(), keyName.begin(),
                            [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
                    }
                }
            }
        }
        std::wstring functionName = L"?";
        {
            ChmFuncType funcType = pair.second;
            for (const auto& p : s_actionNameMap)
            {
                if (p.second == funcType)
                    functionName = p.first;
            }
        }
#ifdef _DEBUG
        // デバッグビルドのときは、ぜんていぎをひょうじ
#else
        if (pair.first.state != ChmEngine::State::Inputing) continue;
#endif
        result += (std::wstring)(pair.first.shift ? L" Shift+" : L"") +
                  (pair.first.ctrl  ? L" Ctrl+"  : L"") +
                  (pair.first.alt   ? L" Alt+"   : L"") +
                  keyName +
#ifdef _DEBUG
                  (pair.first.state == ChmEngine::State::Selecting ? L" (Selecting)" :
                   pair.first.state == ChmEngine::State::None ? L" (noInputing)" : L"") +
#endif
                  L" = <" + functionName + L">\n";
    }
    return result;
}
