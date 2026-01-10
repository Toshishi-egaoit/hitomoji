// Controller.cpp 
// Copyright (C) 2026 Hitomoji Project. All rights reserved.
#include "Controller.h"

CController::CController() : m_isON(FALSE) {}

BOOL CController::IsKeyEaten(WPARAM wp) {
    if (!m_isON) return FALSE;

    // v0.1: A-Z ‚Æ Return ‚ðH‚×‚é
    if (wp >= 'A' && wp <= 'Z') return TRUE;
    if (wp == VK_RETURN) return TRUE;

    return FALSE;
}