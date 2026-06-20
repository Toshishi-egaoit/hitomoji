#include <Windows.h>
#include <WinUser.h>
#include "ChmKeyEvent.h"
#include "ChmConfig.h"
#include "ChmFunctionKeyMap.h"

// ---- キーボードレイアウト（CharInput 専用） ----
class ChmKeyLayout {
public:
    struct KeyDef {
        WPARAM  vk;
        unsigned char normal;
        unsigned char shift;
    };

    static bool Translate(WPARAM vk, bool shift, bool caps, unsigned char& out)
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
        { '0', '0', ')' },
        { '1', '1', '!' },
        { '2', '2', '@' },
        { '3', '3', '#' },
        { '4', '4', '$' },
        { '5', '5', '%' },
        { '6', '6', '^' },
        { '7', '7', '&' },
        { '8', '8', '*' },
        { '9', '9', '(' },

        // letters (A–Z)
        { 'A', 'a', 'A' }, { 'B', 'b', 'B' }, { 'C', 'c', 'C' },
        { 'D', 'd', 'D' }, { 'E', 'e', 'E' }, { 'F', 'f', 'F' },
        { 'G', 'g', 'G' }, { 'H', 'h', 'H' }, { 'I', 'i', 'I' },
        { 'J', 'j', 'J' }, { 'K', 'k', 'K' }, { 'L', 'l', 'L' },
        { 'M', 'm', 'M' }, { 'N', 'n', 'N' }, { 'O', 'o', 'O' },
        { 'P', 'p', 'P' }, { 'Q', 'q', 'Q' }, { 'R', 'r', 'R' },
        { 'S', 's', 'S' }, { 'T', 't', 'T' }, { 'U', 'u', 'U' },
        { 'V', 'v', 'V' }, { 'W', 'w', 'W' }, { 'X', 'x', 'X' },
        { 'Y', 'y', 'Y' }, { 'Z', 'z', 'Z' },

        // basic symbols (US keyboard)
        { VK_SPACE,          ' ',  ' ' },
        { VK_OEM_MINUS,      '-',  '_' },
        { VK_OEM_PLUS,       '=',  '+' },
        { VK_OEM_4,          '[',  '{' },
        { VK_OEM_6,          ']',  '}' },
        { VK_OEM_5,          '\\', '|' },
        { VK_OEM_1,          ';',  ':' },
        { VK_OEM_7,          '\'', '"' },
        { VK_OEM_COMMA,      ',',  '<' },
        { VK_OEM_PERIOD,     '.',  '>' },
        { VK_OEM_2,          '/',  '?' },
        { VK_OEM_3,          '`',  '~' },
    };
};

// ---- FunctionKeyTableのていぎとしょきかしょり ----
static ChmFunctionKeyMap s_functionKeyMap;


void ChmKeyEvent::ClearFunctionKey()
{
    s_functionKeyMap.Clear();
}

void ChmKeyEvent::InitFunctionKey()
{
    s_functionKeyMap.InitDefault();
}

void ChmKeyEvent::SetFunctionKeyMap(const ChmFunctionKeyMap& functionKeyMap)
{
    s_functionKeyMap = functionKeyMap;
}

BOOL ChmKeyEvent::ParseFunctionKey(const std::wstring& key,
                                   const std::wstring& value,
                                   ChmConfig::ParseResult& errorMsg)
{
    return s_functionKeyMap.ParseFunctionKey(key, value, errorMsg);
}

std::wstring ChmKeyEvent::Dump()
{
    return s_functionKeyMap.Dump();
}
// テストプログラムようのstaticメソッド。
//   Win32::GetKeyStateをテストで、さしかえできるようにした。
ChmKeyEvent::KeyStateProvider  ChmKeyEvent::pFunc_keyStateProvider = ::GetKeyState;
void ChmKeyEvent::SetKeyStateProvider(ChmKeyEvent::KeyStateProvider pFunc)
{
    pFunc_keyStateProvider  = pFunc ;
}

ChmKeyEvent::ChmKeyEvent(WPARAM wp, LPARAM /*lp*/, ChmEngine::State isSelect)
	: _wp(wp), _type(ChmFuncType::None), _state(isSelect)
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

void ChmKeyEvent::_TranslateByTable()
{
    ChmFuncType funcType = s_functionKeyMap.Resolve(_wp, _shift, _control, _alt, _state);
    if (funcType != ChmFuncType::None)
    {
        _type = funcType;
        return;
    }

    if (IsNavigationKey())
    {
        _type = ChmFuncType::CompFinish;
		_isNavigationFinish = true;
        return;
    }

    if (_control || _alt)
    {
        _type = ChmFuncType::None;
        return;
    }

	// 機能キーでなかった場合は、文字入力とみなす
    unsigned char ch = 0;
    if (ChmKeyLayout::Translate(_wp, _shift, _caps, ch))
    {
		// select中の文字入力は確定キーとして扱う
		if (_state == ChmEngine::State::Selecting) {
			_type = ChmFuncType::SelectInput;
		}
		else {
			_type = ChmFuncType::CharInput;
		}
        return;
    }

    _type = ChmFuncType::None;
}

unsigned char ChmKeyEvent::GetChar() const {
	unsigned char ch;
	if (ChmKeyLayout::Translate(_wp, _shift, _caps, ch)) {
		return ch;
	} else {
		return  '\0' ;
	}
}

bool ChmKeyEvent::IsUnFinishKey() const {
	return s_functionKeyMap.IsUnFinishKey(_wp, _shift, _control, _alt);
}
