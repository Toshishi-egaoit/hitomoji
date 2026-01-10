// Controller.h
// Copyright (C) 2026 by Hitomoji Project. All rights reserved.
#pragma once
#include <windows.h>

class CController {
public:
    CController();
    
    // ƒL[‚ğIME‚Åˆ—‚·‚×‚«‚©”»’è
    BOOL IsKeyEaten(WPARAM wp);
    
    // IME‚ÌON/OFF
    void ToggleIME() { m_isON = !m_isON; }
    BOOL IsON() const { return m_isON; }

private:
    BOOL m_isON;
};
