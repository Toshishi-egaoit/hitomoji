// Controller.cpp 
// Copyright (C) 2026 Hitomoji Project. All rights reserved.
#include "Controller.h"
#include <cctype>

CController::CController() : m_isON(FALSE) {}

BOOL CController::IsKeyEaten(WPARAM wp) {
    if (!m_isON) return FALSE;

	return ChmKeyEvent::IsKeyEaten(wp);
}

// ---- キー定義テーブル ----
static const ChmKeyEvent::KeyDef g_keyTable[] = {
    { VK_RETURN, false, ChmKeyEvent::Type::CommitKana,     0 },
    { VK_RETURN, true,  ChmKeyEvent::Type::CommitKatakana, 0 },
    { VK_ESCAPE, false, ChmKeyEvent::Type::Cancel,         0 },

    // CharInput
    { VK_OEM_MINUS, false, ChmKeyEvent::Type::CharInput, '-' },
};

bool ChmKeyEvent::IsKeyEaten(WPARAM wp)
{
    if ((wp >= 'A' && wp <= 'Z') || (wp >= 'a' && wp <= 'z')) return true;
	if (wp == VK_SHIFT || wp == VK_CONTROL || wp == VK_MENU) return false;
    for (auto& k : g_keyTable) {
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
    for (auto& k : g_keyTable) {
        if (k.wp == _wp && k.needShift == _shift) {
            _type = k.type;
            _ch   = k.ch;
            return;
        }
    }

    // 英字は共通処理
    if ((_wp >= 'A' && _wp <= 'Z') || (_wp >= 'a' && _wp <= 'z')) {
        _type = Type::CharInput;
        _ch = (char)std::tolower((unsigned char)_wp);
        return;
    }

    _type = Type::None;
}

