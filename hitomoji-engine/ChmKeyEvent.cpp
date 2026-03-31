#include <Windows.h>
#include <WinUser.h>
#include <map>
#include <algorithm>
#include "ChmKeyEvent.h"
#include "ChmConfig.h"

// ---- キーボードレイアウト（CharInput 専用） ----
class ChmKeyLayout {
public:
    struct KeyDef {
        WPARAM  vk;
        wchar_t normal;
        wchar_t shift;
    };

    static bool Translate(WPARAM vk, bool shift, bool caps, wchar_t& out)
    {
        bool logicalShift = shift;
        if (vk >= 'A' && vk <= 'Z') {
            logicalShift = shift ^ caps;
        }
        for (auto& k : g_keyTable) {
            if (k.vk == vk) {
                out = logicalShift ? k.shift : k.normal;
                return out != 0;
            }
        }
        return false;
    }

    static bool Translate(WPARAM vk, bool shift, wchar_t& out)
    {
        for (auto& k : g_keyTable) {
            if (k.vk == vk) {
                out = shift ? k.shift : k.normal;
                return out != 0;
            }
        }
        return false;
    }

private:
	static void _TranslateByTable();
    static constexpr KeyDef g_keyTable[] = {
        // digits
        { '0', L'0', L')' },
        { '1', L'1', L'!' },
        { '2', L'2', L'@' },
        { '3', L'3', L'#' },
        { '4', L'4', L'$' },
        { '5', L'5', L'%' },
        { '6', L'6', L'^' },
        { '7', L'7', L'&' },
        { '8', L'8', L'*' },
        { '9', L'9', L'(' },

        // letters (A–Z)
        { 'A', L'a', L'A' }, { 'B', L'b', L'B' }, { 'C', L'c', L'C' },
        { 'D', L'd', L'D' }, { 'E', L'e', L'E' }, { 'F', L'f', L'F' },
        { 'G', L'g', L'G' }, { 'H', L'h', L'H' }, { 'I', L'i', L'I' },
        { 'J', L'j', L'J' }, { 'K', L'k', L'K' }, { 'L', L'l', L'L' },
        { 'M', L'm', L'M' }, { 'N', L'n', L'N' }, { 'O', L'o', L'O' },
        { 'P', L'p', L'P' }, { 'Q', L'q', L'Q' }, { 'R', L'r', L'R' },
        { 'S', L's', L'S' }, { 'T', L't', L'T' }, { 'U', L'u', L'U' },
        { 'V', L'v', L'V' }, { 'W', L'w', L'W' }, { 'X', L'x', L'X' },
        { 'Y', L'y', L'Y' }, { 'Z', L'z', L'Z' },

        // basic symbols (US keyboard)
        { VK_SPACE,          L' ',  L' ' },
        { VK_OEM_MINUS,      L'-',  L'_' },
        { VK_OEM_PLUS,       L'=',  L'+' },
        { VK_OEM_4,          L'[',  L'{' },
        { VK_OEM_6,          L']',  L'}' },
        { VK_OEM_5,          L'\\', L'|' },
        { VK_OEM_1,          L';',  L':' },
        { VK_OEM_7,          L'\'', L'"' },
        { VK_OEM_COMMA,      L',',  L'<' },
        { VK_OEM_PERIOD,     L'.',  L'>' },
        { VK_OEM_2,          L'/',  L'?' },
        { VK_OEM_3,          L'`',  L'~' },
    };
};

// ---- 機能キー定義テーブル ----
// 内部専用構造体
struct FuncKeyDef {
    WPARAM wp;
    bool needShift;
    bool needCtrl;
    bool needAlt;
    ChmEngine::State _state;
    ChmFuncType type;
};

// デフォルト定義（ビルド時固定）
static const FuncKeyDef g_functionKeyTable[] = {
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
    { 'H',         false, true,  false, ChmEngine::State::Selecting, ChmFuncType::Backspace      },
    { 'I',         false, true,  false, ChmEngine::State::Selecting, ChmFuncType::CompFinishKey  },
    { 'I',         true,  true,  false, ChmEngine::State::Selecting, ChmFuncType::CompFinishKeyWide},
    { 'M',         false, true,  false, ChmEngine::State::Selecting, ChmFuncType::CompFinish     },
#ifdef _DEBUG
    { 'V',         true,  true,  false, ChmEngine::State::None,      ChmFuncType::VersionInfo    },
    { 'R',         true,  true,  false, ChmEngine::State::None,      ChmFuncType::ReloadIni      },
#endif
};

// ---- 実行時キー → Type テーブル ----
struct KeySignature {
    WPARAM wp;
    bool shift;
    bool ctrl;
    bool alt;
	ChmEngine::State state;

    bool operator<(const KeySignature& rhs) const {
        return std::tie(wp, shift, ctrl, alt, state)
             < std::tie(rhs.wp, rhs.shift, rhs.ctrl, rhs.alt, rhs.state);
    }
};

// ---- FunctionKeyTableのていぎとしょきかしょり ----
static std::map<KeySignature, ChmFuncType> s_currentKeyTable;

void ChmKeyEvent::ClearFunctionKey()
{
    s_currentKeyTable.clear();
}

void ChmKeyEvent::InitFunctionKey()
{
    ClearFunctionKey();
    for (const auto& k : g_functionKeyTable)
    {
		// TODO: v0.4で、configの指定方法を変更する必要がある
		KeySignature sig{ k.wp, k.needShift, k.needCtrl, k.needAlt, ChmEngine::State::Inputing};
		s_currentKeyTable[sig] = k.type;
    }
}

// ---- actionName → Type 変換テーブル ----
static const std::map<std::wstring, ChmFuncType> s_actionNameMap = {
    { L"finish",             ChmFuncType::CompFinish },
    { L"finish-hiragana",    ChmFuncType::CompFinishHiragana },
    { L"finish-katakana",    ChmFuncType::CompFinishKatakana },
    { L"finish-raw",         ChmFuncType::CompFinishKey },
    { L"finish-raw-wide",    ChmFuncType::CompFinishKeyWide },
    { L"select",             ChmFuncType::CompSelect },
    { L"backspace",          ChmFuncType::Backspace },
    { L"cancel",             ChmFuncType::Cancel },
    { L"cancel-finish",      ChmFuncType::UnFinish },
#ifdef _DEBUG
    { L"version-info",       ChmFuncType::VersionInfo },
    { L"reload-ini",         ChmFuncType::ReloadIni },
#endif
};

bool ChmKeyEvent::_ResolveActionName(const std::wstring& name, ChmFuncType& outType)
{
    auto it = s_actionNameMap.find(name);
    if (it != s_actionNameMap.end())
    {
        outType = it->second;
        return true;
    }
    return false;
}

// ---- key token → VK 変換テーブル ----
static const std::map<std::wstring, UINT> s_keyNameMap = {
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

bool ChmKeyEvent::_ResolveKeyName(const std::wstring& name, UINT& outVk)
{
    auto it = s_keyNameMap.find(name);
    if (it != s_keyNameMap.end())
    {
        outVk = it->second;
        return true;
    }

    // v0.2方針：ここでは物理キー名のみ解決する。
    // 単文字(A-Z / 0-9)は ParseFunctionKey 側で処理する。
    return false;
}


// テストプログラムようのstaticメソッド。
//   Win32::GetKeyStateをテストで、さしかえできるようにした。
ChmKeyEvent::KeyStateProvider  ChmKeyEvent::pFunc_keyStateProvider = ::GetKeyState;
void ChmKeyEvent::SetKeyStateProvider(ChmKeyEvent::KeyStateProvider pFunc)
{
    pFunc_keyStateProvider  = pFunc ;
}

ChmKeyEvent::ChmKeyEvent(WPARAM wp, LPARAM /*lp*/, ChmEngine::State isSelect)
    : _wp(wp), _type(ChmFuncType::None)
{
    _shift   = (pFunc_keyStateProvider(VK_SHIFT)   & 0x8000) != 0;
    _control = (pFunc_keyStateProvider(VK_CONTROL) & 0x8000) != 0;
    _alt     = (pFunc_keyStateProvider(VK_MENU)    & 0x8000) != 0;
	_caps    = (pFunc_keyStateProvider(VK_CAPITAL) & 0x0001) != 0;

    _TranslateByTable();
}

// 特殊用途コンストラクタ（マウスクリックなど）
ChmKeyEvent::ChmKeyEvent(ChmFuncType type)
    : _wp(VK_HITOMOJI), _shift(false), _control(false), _alt(false), _type(type)
{
	_caps    = false;
}

// ---- function-key 解析 ----
BOOL ChmKeyEvent::ParseFunctionKey(const std::wstring& key,
                                   const std::wstring& value,
                                   ChmConfig::ParseResult& errorMsg)
{
    // 左辺: 物理キー定義（例: ctrl+enter）
    std::wstring keySpec = ChmConfig::Canonize(key);
    if (keySpec.empty())
    {
		ChmConfig::SetError(errorMsg,L"invalid function-key definition");
        return FALSE;
    }

    // 右辺: action 名
    std::wstring actionName = ChmConfig::Canonize(value);
    ChmFuncType actionType;
    if (!_ResolveActionName(actionName, actionType))
    {
		ChmConfig::SetError(errorMsg,L"unknown function-key action: " + actionName);
        return FALSE;
    }

    // keySpec を '+' で分割
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
        token = ChmConfig::Trim(token);
        std::transform(token.begin(), token.end(), token.begin(),
            [](wchar_t ch) { return (wchar_t)towlower(ch); });
        if (!token.empty())
            tokens.push_back(token);
    }

    if (tokens.empty())
    {
		ChmConfig::SetError(errorMsg,L"empty key definition");
        return FALSE;
    }

    bool needShift = false;
    bool needCtrl  = false;
    bool needAlt   = false;

    // 最後以外は modifier
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
				ChmConfig::SetError(errorMsg,L"invalid modifier: " + t);
                return FALSE;
            }
        }
    }

    const std::wstring& keyToken = tokens.back();
    UINT vk = 0;

    if (_ResolveKeyName(keyToken, vk))
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
        // 英字・数字は CTRL または ALT を含まない場合はエラー
        if (vk !=0 && !needCtrl && !needAlt)
        {
			ChmConfig::SetError(errorMsg,L"alphabet/digit requires CTRL or ALT modifier");
            return FALSE;
        }    
    }
    if ( vk == 0) 
    {
		ChmConfig::SetError(errorMsg, L"invalid key name: " + keyToken);
        return FALSE;
    }
	// TODO: 本来はfunctionキーにisSelectのモードも指定させるべきだが、今はInputing固定
    KeySignature sig{ vk, needShift, needCtrl, needAlt, ChmEngine::State::Inputing};
    
    // duplicate 検出
    auto it = s_currentKeyTable.find(sig);
    if (it != s_currentKeyTable.end())
    {
		ChmConfig::SetInfo(errorMsg, L"duplicate function-key definition: " + keySpec);
        // でも後勝ちで上書きする
    }
    
    s_currentKeyTable[sig] = actionType;

    return TRUE;
}

std::wstring ChmKeyEvent::Dump()
{
	std::wstring result;
	for (const auto& pair : s_currentKeyTable) {
		std::wstring keyName = L"?";
		{
			wchar_t ch = pair.first.wp ;
			if ((ch >= L'A' && ch <= L'Z') ||
			    (ch >= L'0' && ch <= L'9'))
			{
				keyName = std::wstring(1,ch) ;
			}
			else {
				for (const auto& p : s_keyNameMap)
				{
					if (p.second == ch)
						keyName = p.first;
						std::transform(keyName.begin(), keyName.end(), keyName.begin(),
							[](wchar_t c) { return (wchar_t)towlower(c); });
				}
			}
		}
		std::wstring FunctionName = L"?";
		{
			ChmFuncType funcType = pair.second;
			for (const auto& p : s_actionNameMap )
			{
				if (p.second == funcType)
					FunctionName = p.first;
			}
		}
		result += (std::wstring)(pair.first.shift ? L" Shift+" : L"") +
				  (pair.first.ctrl  ? L" Ctrl+"  : L"") +
				  (pair.first.alt   ? L" Alt+"   : L"") +
				  keyName +
			L" = <" + FunctionName + L">\n";
	}
	return result;
}

void ChmKeyEvent::_TranslateByTable()
{
    KeySignature sig{ _wp, _shift, _control, _alt , _state};
    auto it = s_currentKeyTable.find(sig);
    if (it != s_currentKeyTable.end())
    {
        _type = it->second;
        return;
    }

    if (IsNavigationKey())
    {
        _type = ChmFuncType::CompFinish;
        return;
    }

    if (_control || _alt)
    {
        _type = ChmFuncType::None;
        return;
    }

	// 機能キーでなかった場合は、文字入力とみなす
    wchar_t ch = 0;
    if (ChmKeyLayout::Translate(_wp, _shift, _caps, ch))
    {
        _type = ChmFuncType::CharInput;
        return;
    }

    _type = ChmFuncType::None;
}

wchar_t ChmKeyEvent::GetChar() const {
	wchar_t ch;
	if (ChmKeyLayout::Translate(_wp, _shift, _caps, ch)) {
		return ch;
	} else {
		return  '\0' ;
	}
}
