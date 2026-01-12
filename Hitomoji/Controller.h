// Controller.h
// Copyright (C) 2026 by Hitomoji Project. All rights reserved.
#pragma once
#include <windows.h>
#include <cstdint>

class ChmKeyEvent;

class CController {
public:
    CController();
    
    // キーをIMEで処理すべきか判定
    BOOL IsKeyEaten(WPARAM wp);
    
    // IMEのON/OFF
    void ToggleIME() { m_isON = !m_isON; }
    BOOL IsON() const { return m_isON; }

private:
    BOOL m_isON;
};

// v0.1.1: キー定義をテーブル駆動にする

class ChmKeyEvent {
public:
    enum class Type {
        None,
        CharInput,      // 通常文字入力
        CommitKana,     // Enter
        CommitKatakana, // Shift+Enter
        Cancel,         // ESC
        Backspace,      // BS（将来）
    };

    struct KeyDef {
        WPARAM wp;
        bool   needShift;
        Type   type;
        char   ch;   // CharInput のときのみ有効
    };

    // --- utility ---
    static bool IsKeyEaten(WPARAM wp);

    // --- ctor ---
    ChmKeyEvent(WPARAM wp, LPARAM lp);

    // --- accessors ---
    bool IsCommit() const { return (GetType() == Type::CommitKana || GetType() == Type::CommitKatakana); }
    Type GetType() const { return _type; }
    char GetChar() const { return _ch; }
    bool IsShift() const { return _shift; }

private:
    void _TranslateByTable();

    WPARAM _wp;
    bool   _shift;

    Type _type = Type::None;
    char _ch   = 0;
};

