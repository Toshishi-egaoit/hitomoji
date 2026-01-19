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

	ChmKeyEvent ev(wp,0) ;

	// Compositionがない場合は通常文字だけ食う。（特殊キーは処理しない）
	if (!_hasComposition) {
		return  (ev.GetType() == ChmKeyEvent::Type::CharInput);
	}
	// IMEが無視するもの以外は全て食う
	return  (ev.GetType() != ChmKeyEvent::Type::None);
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
		case ChmKeyEvent::Type::CommitKana:		// ひらがな変換
			_hasComposition = FALSE;
			break;
		case ChmKeyEvent::Type::CharInput:
			// 通常の文字入力
			if (!_hasComposition ) {
				// 文字入力でComposition開始
				_pRawInputStore->clear();
				_hasComposition = TRUE;
			}
			_pRawInputStore->push((char)std::tolower((unsigned char)keyEvent.GetChar()));
			ChmRomajiConverter::convert(_pRawInputStore->get(), _converted, _pending);
			break;
		case ChmKeyEvent::Type::Cancel:
			_pRawInputStore->clear();
			_converted = L"";
			_pending = "";
			_hasComposition = FALSE;
			break;
		// TODO: 再変換(確定取消）キー処理(v0.3以降)
		// TODO: BSキー処理(v0.3以降)
		default:
			// その他のキーは無視
			OutputDebugStringWithInt(L"   > Invalid Key:%d",(int)_type);
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
    _hasComposition = FALSE;
    _pRawInputStore->clear();
    _converted = L"";
    _pending = "";
}

std::wstring ChmEngine::GetCompositionStr(){
	if ( _pRawInputStore == nullptr ) return L"";

	// 既に変換済みの文字列を連結して返却
	return _converted + std::wstring(_pending.begin(), _pending.end());
}


// ---- 機能キー定義テーブル ----
static const ChmKeyEvent::KeyDef g_functionKeyTable[] = {
	// WPARAM    SHIFT  Type                               endComp ch
    { VK_RETURN, false, ChmKeyEvent::Type::CommitKana,     true,   0 },
    { VK_RETURN, true,  ChmKeyEvent::Type::CommitKatakana, true,   0 },
    { VK_ESCAPE, false, ChmKeyEvent::Type::Cancel,         true,   0 },
};

// ---- 通常キー定義テーブル ----
static const ChmKeyEvent::KeyDef g_normalKeyTable[] = {
    // CharInput
	// WPARAM       SHIFT  Type                          endComp ch
    { VK_OEM_MINUS, false, ChmKeyEvent::Type::CharInput, false,  '-' },
};

ChmKeyEvent::ChmKeyEvent(WPARAM wp, LPARAM /*lp*/)
	: _wp(wp), _shift(false), _type(Type::None), _ch(0)
{
    _shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    _TranslateByTable();
}

void ChmKeyEvent::_TranslateByTable()
{
    // まずテーブル照合
    for (auto& k : g_functionKeyTable) {
        if (k.wp == _wp && k.needShift == _shift) {
            _type = k.type;
            _ch   = k.ch;
            _endComp = k.endComposition;
            return;
        }
    }

    // 英字は共通処理
    if ((_wp >= 'A' && _wp <= 'Z') || (_wp >= 'a' && _wp <= 'z')) {
        _type = Type::CharInput;
        _ch = (char)std::tolower((unsigned char)_wp);
		return ;
	}

    // その他の通常文字
    for (auto& k : g_normalKeyTable) {
		if (_wp == k.wp) {
			_type = Type::CharInput;
			_ch = (char)std::tolower((unsigned char)_wp);
			return ;
		}
    }

	// それ以外は処理対象外
    _type = Type::None;
}

