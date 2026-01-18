// ChmEngine.h
// Copyright (C) 2026 by Hitomoji Project. All rights reserved.
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include "utils.h"

class ChmKeyEvent;
class ChmRawInputStore;

class ChmEngine {
public:
    ChmEngine();
    
    // キーをIMEで処理すべきか判定
    BOOL IsKeyEaten(WPARAM wp);
    
    // IMEのON/OFF
    void ToggleIME() { _isON = !_isON; }
    BOOL IsON() const { return _isON; }
	std::wstring GetCompositionStr() ;

	void UpdateComposition(const ChmKeyEvent& keyEvent);
	void PostUpdateComposition();

private:
	void _InitComposition();
    BOOL _isON;
	BOOL _hasComposition;
	ChmRawInputStore* _pRawInputStore; // 入力されたローマ字列
	std::wstring _converted; // かな変換できた部分
	std::string _pending; // かなに変換できていない部分（残り）
};

// v0.1.1: キー定義をテーブル駆動にする

class ChmKeyEvent {
public:
    enum class Type {
        None = 0,
        CharInput,      // 通常文字入力
        CommitKana,     // Enter
        CommitKatakana, // Shift+Enter
        Cancel,         // ESC
        Backspace,      // BS（将来）
        Uncommit,       // 確定取消（将来）
    };

    struct KeyDef {
        WPARAM wp;
        bool   needShift;
        Type   type;
        char   ch;   // CharInput のときのみ有効
    };

    // --- utility ---
    static bool IsNormalKey(WPARAM wp);

    // --- ctor ---
    ChmKeyEvent(WPARAM wp, LPARAM lp);

    // --- accessors ---
    bool IsCommit() const { return (GetType() == Type::CommitKana || GetType() == Type::CommitKatakana || GetType() == Type::Cancel); }
    Type GetType() const { return _type; }
    char GetChar() const { return _ch; }
    bool IsShift() const { return _shift; }
	const std::wstring dump() const { 
		wchar_t buff[64];
		wsprintf(buff, L"[_type:%d,_ch:%c(%x)]", static_cast<int>(_type), static_cast<int>(GetChar()),static_cast<int>(GetChar()));
		return buff ;
	}

private:
    void _TranslateByTable();

    WPARAM _wp;
    bool   _shift;

    Type _type = Type::None;
    char _ch   = 0;
};

