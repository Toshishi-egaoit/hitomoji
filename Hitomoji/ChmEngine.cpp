// ChmEngine.cpp 
// Copyright (C) 2026 Hitomoji Project. All rights reserved.
#include <cctype>
#include <assert.h>
#include "ChmEngine.h"
#include "ChmRomajiConverter.h"
#include "ChmRawInputStore.h"

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

    // GetTypeで意味づけされたキーのみIMEが食う
    return (ev.GetType() != ChmKeyEvent::Type::None);
}

void ChmEngine::UpdateComposition(const ChmKeyEvent& keyEvent){
	OutputDebugStringWithString(
		L"[Hitomoji] UpdateComposition: keyEvent=%s", 
		keyEvent.dump().c_str()
	);
	ChmKeyEvent::Type _type = keyEvent.GetType();

	// 確定キー
	switch (_type) {
        case ChmKeyEvent::Type::CommitKatakana: // カタカナ変換
            _converted = ChmRomajiConverter::HiraganaToKatakana(_converted);
            // [[fallthrough]]
        case ChmKeyEvent::Type::CommitKana:     // ひらがな変換
            _hasComposition = FALSE;
            break;
        case ChmKeyEvent::Type::CommitAscii:    // ASCII確定
            _converted = std::wstring(_pRawInputStore->get().begin(), _pRawInputStore->get().end());
			_pending.clear();
            _hasComposition = FALSE;
            break;
        case ChmKeyEvent::Type::CommitAsciiWide: // 全角ASCII確定
            _converted = AsciiToWide(_pRawInputStore->get());
			_pending.clear();
            _hasComposition = FALSE;
            break;
        case ChmKeyEvent::Type::CharInput:
            // 通常の文字入力（v0.1.2.3 相当）
            if (!_hasComposition) {
                // 文字入力でComposition開始
                _pRawInputStore->clear();
                _hasComposition = TRUE;
            }
            // 英字は既に ChmKeyEvent 側で大小決定済み
            _pRawInputStore->push(keyEvent.GetChar());
            ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending);
            break;
        default:
            // その他のキーは何もしない
            break;
	}

	return ;
}

void ChmEngine::PostUpdateComposition(){
	// 変換中でなくなった場合は、残りかすを処分
	if (!_hasComposition) {
		_pRawInputStore->clear();
		_converted = L"";
		_pending = "";
	}
	return;
}

void ChmEngine::ResetStatus() {
	OutputDebugString(L"ResetStatus");
    _hasComposition = FALSE;
    _pRawInputStore->clear();
    _converted = L"";
    _pending = "";
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


// ---- 機能キー定義テーブル ----
static const ChmKeyEvent::FuncKeyDef g_functionKeyTable[] = {
    // WPARAM      SHIFT  CTRL  ALT   Type                                   endComp
    { VK_RETURN,   false, false, false, ChmKeyEvent::Type::CommitKana,         true },
    { VK_RETURN,   true,  false, false, ChmKeyEvent::Type::CommitKatakana,     true },
    { VK_TAB,      false, false, false, ChmKeyEvent::Type::CommitAscii,        true },
    { VK_TAB,      true,  false, false, ChmKeyEvent::Type::CommitAsciiWide,    true },
    { VK_ESCAPE,   false, false, false, ChmKeyEvent::Type::Cancel,             true },
    // 検証用: Ctrl+M = Enter
    { 'M',         false, true,  false, ChmKeyEvent::Type::CommitKana,         true },
};

// ---- 通常キー定義テーブル ----
static const ChmKeyEvent::CharKeyDef g_charKeyTable[] = {
    // WPARAM        SHIFT  ch
    { VK_OEM_MINUS,  false, '-' },

    // --- digits (ASCII keyboard) ---
    // no-shift
    { '0', false, '0' },
    { '1', false, '1' },
    { '2', false, '2' },
    { '3', false, '3' },
    { '4', false, '4' },
    { '5', false, '5' },
    { '6', false, '6' },
    { '7', false, '7' },
    { '8', false, '8' },
    { '9', false, '9' },

    // shift (US-ASCII)
    { '1', true,  '!' },
    { '2', true,  '@' },
    { '3', true,  '#' },
    { '4', true,  '$' },
    { '5', true,  '%' },
    { '6', true,  '^' },
    { '7', true,  '&' },
    { '8', true,  '*' },
    { '9', true,  '(' },
    { '0', true,  ')' },
};

ChmKeyEvent::ChmKeyEvent(WPARAM wp, LPARAM /*lp*/)
    : _wp(wp), _shift(false), _control(false), _alt(false), _type(Type::None), _ch(0)
{
    _shift   = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;
    _control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    _alt     = (GetKeyState(VK_MENU)    & 0x8000) != 0;
    _TranslateByTable();
}


void ChmKeyEvent::_TranslateByTable()
{
    // ① 機能キー定義テーブル
    for (auto& k : g_functionKeyTable) {
        if (k.wp == _wp &&
            k.needShift == _shift &&
            k.needCtrl  == _control &&
            k.needAlt   == _alt) {
            _type = k.type;
            _endComp = k.endComposition;
            return;
        }
    }

    // ② CTRL / ALT 修飾中は通常文字にしない
    if (_control || _alt) {
        _type = Type::None;
        return;
    }

    // ③ 英字
    if ((_wp >= 'A' && _wp <= 'Z') || (_wp >= 'a' && _wp <= 'z')) {
        _type = Type::CharInput;
        // shift 状態に応じて大小文字を決定（CAPSLOCK は将来対応）
        if (_shift) {
            _ch = (char)std::toupper((unsigned char)_wp);
        } else {
            _ch = (char)std::tolower((unsigned char)_wp);
        }
        return;
    }

    // ④ CharKeyTable
    for (auto& k : g_charKeyTable) {
        if (k.wp == _wp && k.needShift == _shift) {
            _type = Type::CharInput;
            _ch = k.ch;
            return;
        }
    }

    _type = Type::None;
}