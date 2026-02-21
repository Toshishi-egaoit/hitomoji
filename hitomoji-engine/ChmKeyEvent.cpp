#include <Windows.h>
#include <WinUser.h>
#include <map>
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
// デフォルト定義（ビルド時固定）

static const ChmKeyEvent::FuncKeyDef g_functionKeyTable[] = {
    // WPARAM      SHIFT  CTRL   ALT    Type
    { VK_RETURN,   false, false, false, ChmKeyEvent::Type::CommitKana     },
    { VK_RETURN,   true,  false, false, ChmKeyEvent::Type::CommitKatakana },
    { VK_TAB,      false, false, false, ChmKeyEvent::Type::CommitAscii    },
    { VK_TAB,      true,  false, false, ChmKeyEvent::Type::CommitAsciiWide},
    { VK_BACK,     false, false, false, ChmKeyEvent::Type::Backspace      },
    { VK_ESCAPE,   false, false, false, ChmKeyEvent::Type::Cancel         },
    // CTRL+*
    { 'H',         false, true,  false, ChmKeyEvent::Type::Backspace      },
    { 'I',         false, true,  false, ChmKeyEvent::Type::CommitAscii    },
    { 'I',         true,  true,  false, ChmKeyEvent::Type::CommitAsciiWide},
    { 'M',         false, true,  false, ChmKeyEvent::Type::CommitKana     },
#ifdef _DEBUG
    { 'V',         true,  true,  false, ChmKeyEvent::Type::VersionInfo    },
#endif
};

// ---- override テーブル（ini による上書き） ----
// key: Type（動作名）
// value: FuncKeyDef（物理キー定義）
static std::map<ChmKeyEvent::Type, ChmKeyEvent::FuncKeyDef> s_functionKeyOverride;

void ChmKeyEvent::ClearFunctionKeyOverride()
{
    s_functionKeyOverride.clear();
}

// ---- actionName → Type 変換テーブル ----
struct ActionNameDef {
    const wchar_t* name;
    ChmKeyEvent::Type type;
};

static const ActionNameDef g_actionNameTable[] = {
    { L"finish",             ChmKeyEvent::Type::CommitKana },
    { L"finish-katakana",    ChmKeyEvent::Type::CommitKatakana },
    { L"finish-raw",         ChmKeyEvent::Type::CommitAscii },
    { L"finish-raw-wide",    ChmKeyEvent::Type::CommitAsciiWide },
    { L"backspace",          ChmKeyEvent::Type::Backspace },
    { L"cancel",             ChmKeyEvent::Type::Cancel },
    { L"cancel-finish",      ChmKeyEvent::Type::Uncommit },
    { L"pass-through",       ChmKeyEvent::Type::PassThrough },
#ifdef _DEBUG
    { L"version-info",       ChmKeyEvent::Type::VersionInfo },
#endif
};

static bool _ResolveActionName(const std::wstring& name, ChmKeyEvent::Type& outType)
{
    for (const auto& def : g_actionNameTable)
    {
        if (name == def.name)
        {
            outType = def.type;
            return true;
        }
    }
    return false;
}

// ---- key token → VK 変換テーブル ----
struct KeyNameDef {
    const wchar_t* name;
    UINT vk;
};

static const KeyNameDef g_keyNameTable[] = {
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
    { L"delete",   VK_DELETE },
    { L"insert",   VK_INSERT },
    { L"f1",  VK_F1 }, { L"f2",  VK_F2 }, { L"f3",  VK_F3 },
    { L"f4",  VK_F4 }, { L"f5",  VK_F5 }, { L"f6",  VK_F6 },
    { L"f7",  VK_F7 }, { L"f8",  VK_F8 }, { L"f9",  VK_F9 },
    { L"f10", VK_F10 },{ L"f11", VK_F11 },{ L"f12", VK_F12 },
};

static bool _ResolveKeyName(const std::wstring& name, UINT& outVk)
{
    for (const auto& def : g_keyNameTable)
    {
        if (name == def.name)
        {
            outVk = def.vk;
            return true;
        }
    }

        // 1文字の表示可能ASCII (0x21-0x7e) はそのままVKコードへ
    if (name.size() == 1)
    {
        wchar_t ch = name[0];
        if (ch >= 0x21 && ch <= 0x7e)
        {
            // 英字は大文字に正規化（VKは大文字基準）
            if (ch >= L'a' && ch <= L'z')
                ch = static_cast<wchar_t>(towupper(ch));

            outVk = static_cast<UINT>(ch);
            return true;
        }
    }

    return false;
}



// ---- KeyState hook は通常はWin32::GetKeyState
extern SHORT GetKeyState(int);
SHORT (*ChmKeyEvent::s_getKeyStateFunc)(int) = ::GetKeyState;

void ChmKeyEvent::SetGetKeyStateFunc(SHORT (*func)(int))
{
    s_getKeyStateFunc = func ;
}

ChmKeyEvent::ChmKeyEvent(WPARAM wp, LPARAM /*lp*/)
    : _wp(wp), _type(Type::None)
{
    _shift   = (s_getKeyStateFunc(VK_SHIFT)   & 0x8000) != 0;
    _control = (s_getKeyStateFunc(VK_CONTROL) & 0x8000) != 0;
    _alt     = (s_getKeyStateFunc(VK_MENU)    & 0x8000) != 0;
	_caps    = (s_getKeyStateFunc(VK_CAPITAL) & 0x0001) != 0;

    _TranslateByTable();
}

// 特殊用途コンストラクタ（マウスクリックなど）
ChmKeyEvent::ChmKeyEvent(ChmKeyEvent::Type type)
    : _wp(VK_HITOMOJI), _shift(false), _control(false), _alt(false), _type(type)
{
	_caps    = false;
}

// ---- function-key 解析 ----
BOOL ChmKeyEvent::ParseFunctionKey(const std::wstring& key,
                                   const std::wstring& value,
                                   std::wstring& errorMsg)
{
    // 1. action 名解決
    std::wstring actionName = ChmConfig::Canonize(key);
    if (actionName.empty())
    {
        errorMsg = L"invalid function-key name";
        return FALSE;
    }

    ChmKeyEvent::Type actionType;
    if (!_ResolveActionName(actionName, actionType))
    {
        errorMsg = L"unknown function-key action: " + actionName;
        return FALSE;
    }

    // 2. value を '+' で分割
    std::vector<std::wstring> tokens;
    size_t start = 0;
    while (start <= value.size())
    {
        size_t pos = value.find(L'+', start);
        std::wstring token;
        if (pos == std::wstring::npos)
        {
            token = value.substr(start);
            start = value.size() + 1;
        }
        else
        {
            token = value.substr(start, pos - start);
            start = pos + 1;
        }

        token = ChmConfig::Canonize(ChmConfig::Trim(token));
        if (!token.empty())
            tokens.push_back(token);
    }

    if (tokens.empty())
    {
        errorMsg = L"empty function-key definition";
        return FALSE;
    }

    // 3. modifier + 最後が実キーというルールで解析
    FuncKeyDef def{};
    def.type = actionType;

    // modifier 部分（最後以外）
    if (tokens.size() > 1)
    {
        for (size_t i = 0; i < tokens.size() - 1; ++i)
        {
            const std::wstring& t = tokens[i];

            // 順序自由・重複許容
            if (t == L"shift")
            {
                def.needShift = true;
            }
            else if (t == L"ctrl" || t == L"control")
            {
                def.needCtrl = true;
            }
            else if (t == L"alt")
            {
                def.needAlt = true;
            }
            else
            {
                errorMsg = L"invalid modifier in function-key: " + t;
                return FALSE;
            }
        }
    }

    // 最後は必ず実キー
    const std::wstring& keyToken = tokens.back();
    UINT vk;
    if (!_ResolveKeyName(keyToken, vk))
    {
        errorMsg = L"invalid key name in function-key: " + keyToken;
        return FALSE;
    }
    def.wp = vk;
    // 4. duplicate（後勝ち）
    auto it = s_functionKeyOverride.find(actionType);
    if (it != s_functionKeyOverride.end())
    {
        errorMsg = L"duplicate function-key definition (overwritten): " + actionName;
    }

    s_functionKeyOverride[actionType] = def;

    return TRUE;
}


void ChmKeyEvent::_TranslateByTable()
{
    // ① override テーブル（ini 優先）
    for (auto& kv : s_functionKeyOverride)
    {
        const FuncKeyDef& k = kv.second;
        if (k.wp == _wp &&
            k.needShift == _shift &&
            k.needCtrl  == _control &&
            k.needAlt   == _alt)
        {
            _type = k.type;
            return;
        }
    }

    // ② デフォルト機能キー
    for (auto& k : g_functionKeyTable) {
        if (k.wp == _wp &&
            k.needShift == _shift &&
            k.needCtrl  == _control &&
            k.needAlt   == _alt) {
            _type = k.type;
            return;
        }
    }

	// ② ナビゲーションキー
	if (IsNavigationKey()) {
		_type = Type::CommitNonConvert;
		return ;
	}

    // ③ CTRL / ALT 中は CharInput にしない
    if (_control || _alt) {
        _type = Type::None;
        return;
    }

    // ④ キーボードレイアウトによる文字変換
    wchar_t ch = 0;
    if (ChmKeyLayout::Translate(_wp, _shift, _caps, ch)) {
        _type = Type::CharInput;
        //_ch = (char)ch;
        return;
    }

    _type = Type::None;
}

wchar_t ChmKeyEvent::GetChar() const {
	wchar_t ch;
	if (ChmKeyLayout::Translate(_wp, _shift, _caps, ch)) {
		return ch;
	} else {
		return  '\0' ;
	}
}

