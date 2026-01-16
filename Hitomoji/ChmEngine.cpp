// Controller.cpp 
// Copyright (C) 2026 Hitomoji Project. All rights reserved.
#include <cctype>
#include <assert.h>
#include "ChmEngine.h"
#include "ChmRomajiConverter.h"
#include "ChmRawInputStore.h"

ChmEngine::ChmEngine() : _isON(FALSE) {}

BOOL ChmEngine::IsKeyEaten(WPARAM wp) {
	// IMEがOFFなら全てfalse
    if (!_isON) return FALSE;

	// Compositionがない状態では通常文字キーのみ処理
	if (_compositionStatus == COMP_NONE && (! ChmKeyEvent::IsNormalKey(wp) )) return false;

	return ChmKeyEvent::IsKeyEaten(wp);
}

void ChmEngine::UpdateComposition(const ChmKeyEvent& keyEvent){
	ChmKeyEvent::Type _type = keyEvent.GetType();
	// 最初の文字入力
	switch (_compositionStatus) {
		case COMP_NONE:
			// Compositionを今から開始
			if (_type == ChmKeyEvent::Type::CharInput ) {
				// 文字入力でComposition開始
				_InitComposition(keyEvent);
			}
			else {
				// それ以外がくることはないはず。（Keyをアプリに返しているはず）
				OutputDebugString(L"[Hitomoji] Invalid Key");
				return;
			}
			break;
		case COMP_ONGOING:
			break;
		case COMP_COMMITTING:
			// 確定キー処理
			// ここでは何もしない
			break;
		case COMP_RECALL:
			// 未実装
			OutputDebugString(L"[Hitomoji] unimpremented: RECALL state\n");
			return;
	}
	_UpdateComposition(keyEvent);

	return ;
};

void ChmEngine::PostUpdateComposition(){
	// キー処理が完了した時点では、COMP_NONEかCOMP_ONGOINGに必ずなる
	if (_compositionStatus == COMP_COMMITTING) {
		// 確定キー、キャンセル、BSなどで Composition消滅済
		_compositionStatus = COMP_NONE;
		delete _pRawInputStore;
		_pRawInputStore = nullptr;
	} else {
		// 入力中
		_compositionStatus = COMP_ONGOING;
	}
}

void ChmEngine::_InitComposition(const ChmKeyEvent& keyEvent) {
	// 既にCompositionが存在するのはおかしい
	// ASSERT(_pRawInputStore == nullptr);
	//	OutputDebugString(L"[Hitomoji] _InitComposition: Composition already exists\n");

	_pRawInputStore = new ChmRawInputStore();

	return ;
}

void ChmEngine::_UpdateComposition(const ChmKeyEvent& keyEvent) {
	// ASSERT(_pRawInputStore != nullptr);

	std::wstring converted;
	std::string pending;

	switch (keyEvent.GetType()) {
		case ChmKeyEvent::Type::CharInput:
			// TODO: lowercase変換はChmKeyEvent側でやる
			_pRawInputStore->push((char)std::tolower((unsigned char)keyEvent.GetChar()));
			break;

		case ChmKeyEvent::Type::CommitKana:
			// rawInput -> ひらがな
			ChmRomajiConverter::convert(_pRawInputStore->get(), converted, pending);
			_compositionStr = converted + std::wstring(pending.begin(), pending.end());
			break;
		case ChmKeyEvent::Type::CommitKatakana:
			// rawInput -> ひらがな -> カタカナ
			ChmRomajiConverter::convert(_pRawInputStore->get(), converted, pending);
			_compositionStr = ChmRomajiConverter::HiraganaToKatakana(converted)
				+ std::wstring(pending.begin(), pending.end());
			break;
		case ChmKeyEvent::Type::Cancel:
			_pRawInputStore->clear();
			_compositionStr = L"";
			break;
		default:
			// その他のキーは無視
			break;
	}

	return ;
}

std::wstring ChmEngine::GetCompositionStr(){
	if ( _pRawInputStore == nullptr ) return L"";

	// rawInput -> ひらがな
	return _compositionStr;
}


// ---- 機能キー定義テーブル ----
static const ChmKeyEvent::KeyDef g_functionKeyTable[] = {
    { VK_RETURN, false, ChmKeyEvent::Type::CommitKana,     0 },
    { VK_RETURN, true,  ChmKeyEvent::Type::CommitKatakana, 0 },
    { VK_ESCAPE, false, ChmKeyEvent::Type::Cancel,         0 },
};

// ---- 通常キー定義テーブル ----
static const ChmKeyEvent::KeyDef g_normalKeyTable[] = {
    // CharInput
    { VK_OEM_MINUS, false, ChmKeyEvent::Type::CharInput, '-' },
};

bool ChmKeyEvent::IsNormalKey(WPARAM wp) {
	// TODO: モディファイアキーの考慮が必要
    if ((wp >= 'A' && wp <= 'Z') || (wp >= 'a' && wp <= 'z')) return true;
    for (auto& k : g_normalKeyTable) {
        if (k.wp == wp) return true;
    }
	return false ;
}

bool ChmKeyEvent::IsKeyEaten(WPARAM wp)
{
	// TODO: Composition状態によって変える必要がある
    if (IsNormalKey(wp)) return true;
	if (wp == VK_SHIFT || wp == VK_CONTROL || wp == VK_MENU) return false;
    for (auto& k : g_functionKeyTable) {
        if (k.wp == wp) return true;
    }
    return false;
}

ChmKeyEvent::ChmKeyEvent(WPARAM wp, LPARAM /*lp*/)
    : _wp(wp)
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
            return;
        }
    }

    // 英字は共通処理
    if (IsNormalKey(_wp)) {
        _type = Type::CharInput;
        _ch = (char)std::tolower((unsigned char)_wp);
        return;
    }

    _type = Type::None;
}

