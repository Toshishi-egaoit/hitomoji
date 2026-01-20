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
    ~ChmEngine();
    
    // キーをIMEで処理すべきか判定
    BOOL IsKeyEaten(WPARAM wp);
    
    // IMEのON/OFF
    void ToggleIME() { _isON = !_isON; }
    BOOL IsON() const { return _isON; }
	std::wstring GetCompositionStr() ;

	void UpdateComposition(const ChmKeyEvent& keyEvent);
	void PostUpdateComposition();
	void ResetStatus() ;

private:
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
        bool   endComposition;   // compositionを閉じるアクションか？
        char   ch;   // CharInput のときのみ有効
    };

    // --- utility ---
	const std::wstring dump() const { 
		wchar_t buff[64];
		wsprintf(buff, L"[_type:%d,_ch:%c(%x)]", static_cast<int>(_type), static_cast<int>(GetChar()),static_cast<int>(GetChar()));
		return buff ;
	}

    // --- ctor ---
    ChmKeyEvent(WPARAM wp, LPARAM lp);

    // --- accessors ---
    bool ShouldEndComposition() const {return _endComp ;};
    Type GetType() const { return _type; }
    char GetChar() const { return _ch; }
    bool IsShift() const { return _shift; }

private:
    void _TranslateByTable();

    WPARAM _wp;
    bool   _shift = false;
    bool _endComp = false;

    Type _type = Type::None;
    char _ch   = 0;
};

