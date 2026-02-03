// ChmEngine.cpp 
// Copyright (C) 2026 Hitomoji Project. All rights reserved.
#include <cctype>
#include <assert.h>
#include "ChmRawInputStore.h"
#include "ChmEngine.h"
#include "ChmRomajiConverter.h"
#include "ChmConfig.h"

ChmEngine::ChmEngine() 
	: _isON(FALSE), _hasComposition(FALSE),_converted(L""), _pending("") {
	_pRawInputStore = new ChmRawInputStore();
}

ChmEngine::~ChmEngine() {
	if (_pRawInputStore) delete _pRawInputStore;
}

BOOL ChmEngine::IsKeyEaten(WPARAM wp) {
    // IMEがOFFなら全てfalse
    if (!_isON) return FALSE;

    ChmKeyEvent ev(wp, 0);

	// 文字入力なら、常にIMEが食う
	if (ev.GetType() == ChmKeyEvent::Type::CharInput) return TRUE;

	// Compositonが存在する状態の特殊キーはIMEが食う
	if (_hasComposition && ev.GetType() != ChmKeyEvent::Type::None ) return TRUE;

	// それ以外は食わない
    return FALSE;
}

void ChmEngine::UpdateComposition(const ChmKeyEvent& keyEvent, bool& pEndComposition){
	OutputDebugStringWithString(
		L"[Hitomoji] UpdateComposition: keyEvent=%s", 
		keyEvent.toString().c_str()
	);
	ChmKeyEvent::Type _type = keyEvent.GetType();

	// 確定キー
	switch (_type) {
        case ChmKeyEvent::Type::CommitNonConvert: // 無変換確定
			// 未変換部分も含めて全確定
            _hasComposition = FALSE;
			break ;
        case ChmKeyEvent::Type::CommitKatakana: // カタカナ変換
			ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending, 
				true, 
				(g_config.backspaceUnit == ChmConfigStore::BackspaceUnit::Symbol));
            _converted = ChmRomajiConverter::HiraganaToKatakana(_converted);
            _hasComposition = FALSE;
            break ;
        case ChmKeyEvent::Type::CommitKana:     // ひらがな変換
			ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending,
				true,
				(g_config.backspaceUnit == ChmConfigStore::BackspaceUnit::Symbol));
            _hasComposition = FALSE;
            break;
        case ChmKeyEvent::Type::CommitAscii:    // ASCII確定
            _converted = std::wstring(_pRawInputStore->get().begin(), _pRawInputStore->get().end());
			_pending = "";
            _hasComposition = FALSE;
            break;
        case ChmKeyEvent::Type::CommitAsciiWide: // 全角ASCII確定
            _converted = AsciiToWide(_pRawInputStore->get());
			_pending = "";
            _hasComposition = FALSE;
            break;
        case ChmKeyEvent::Type::Cancel:         // ESCキャンセル
			_converted = L"";
			_pending = "";
			_hasComposition = FALSE;
			break;
         case ChmKeyEvent::Type::CharInput:
            // 通常の文字入力
            if (!_hasComposition) {
                _pRawInputStore->clear();
                _hasComposition = TRUE;
            }
            _pRawInputStore->push(keyEvent.GetChar());
            ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending, 
				(g_config.displayMode == ChmConfigStore::DisplayMode::Kana),
				(g_config.backspaceUnit == ChmConfigStore::BackspaceUnit::Symbol));
				;
            break;
        case ChmKeyEvent::Type::Backspace:
            {
				if (!_hasComposition) break;
                size_t len = _pRawInputStore->get().size();
                size_t del = ChmRomajiConverter::GetLastRawUnitLength();

                // Backspace の単位設定を考慮（Char / Unit）
                if (g_config.backspaceUnit == ChmConfigStore::BackspaceUnit::Ascii) {
                    del = 1;
                }

                // 念のためのガード：過剰な削除要求は全消去
                if (del > len) del = len;

                _pRawInputStore->pop(del);
				OutputDebugStringWithInt(L"[Hitomoji] Backspace %d chars",(ULONG)del);

                // 削除の結果文字がなくなったらCompositionを削除
                if (_pRawInputStore->get().empty()) {
                    _converted = L"";
                    _pending = "";
                    _hasComposition = FALSE;
                } else {
                    ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending,
						(g_config.displayMode == ChmConfigStore::DisplayMode::Kana),
						(g_config.backspaceUnit == ChmConfigStore::BackspaceUnit::Symbol));
                }
            }
            break;
        default:
            // その他のキーは何もしない
            break;
	}
	pEndComposition = !_hasComposition;

	return ;
}

void ChmEngine::PostUpdateComposition(){
	OutputDebugString(L"[Hitomoji] PostUpdateComposition");
	// 変換中でなくなった場合は、残りかすを処分
	if (!_hasComposition) {
		ResetStatus();
	}
	return;
}

void ChmEngine::ResetStatus() {
	OutputDebugString(L"[Hitomoji] ResetStatus");
    _hasComposition = FALSE;
    _pRawInputStore->clear();
    _converted = L"";
    _pending = "";
	return;
}

std::wstring ChmEngine::AsciiToWide(const std::string& src)
{
    std::wstring out;
    out.reserve(src.size());

    for (unsigned char c : src) {
        // 英数字・基本記号のみ対象
        if (c >= 0x21 && c <= 0x7E) {
            // 0x21–0x7E は Fullwidth に +0xFEE0
            out.push_back(static_cast<wchar_t>(c + 0xFEE0));
        } else {
            // それ以外はそのまま（保険）
            out.push_back(static_cast<wchar_t>(c));
        }
    }
    return out;
}

std::wstring ChmEngine::GetCompositionStr(){
	if ( _pRawInputStore == nullptr ) return L"";

	// 既に変換済みの文字列を連結して返却
	return _converted + std::wstring(_pending.begin(), _pending.end());
}


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
static const ChmKeyEvent::FuncKeyDef g_functionKeyTable[] = {
    // WPARAM      SHIFT  CTRL  ALT   Type                                   endComp
    { VK_RETURN,   false, false, false, ChmKeyEvent::Type::CommitKana     },
    { VK_RETURN,   true,  false, false, ChmKeyEvent::Type::CommitKatakana },
    { VK_TAB,      false, false, false, ChmKeyEvent::Type::CommitAscii    },
    { VK_TAB,      true,  false, false, ChmKeyEvent::Type::CommitAsciiWide},
    { VK_BACK,     false, false, false, ChmKeyEvent::Type::Backspace      },
    { VK_ESCAPE,   false, false, false, ChmKeyEvent::Type::Cancel         },
    // 検証用: Ctrl+M = Enter
    { 'M',         false, true,  false, ChmKeyEvent::Type::CommitKana     },
};

ChmKeyEvent::ChmKeyEvent(WPARAM wp, LPARAM /*lp*/)
    : _wp(wp), _type(Type::None), _ch(0)
{
    _shift   = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
    _control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    _alt     = (GetKeyState(VK_MENU)    & 0x8000) != 0;
	_caps    = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;

    _TranslateByTable();
}

// 特殊用途コンストラクタ（マウスクリックなど）
ChmKeyEvent::ChmKeyEvent(ChmKeyEvent::Type type)
    : _wp(VK_HITOMOJI), _shift(false), _control(false), _alt(false), _type(type), _ch(0)
{
	_caps    = false;
}

void ChmKeyEvent::_TranslateByTable()
{
    // ① 機能キー
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
		_ch = VK_HITOMOJI; // dummy
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
        _ch = (char)ch;
        return;
    }

    _type = Type::None;
}
