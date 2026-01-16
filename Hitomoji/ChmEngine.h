// Controller.h
// Copyright (C) 2026 by Hitomoji Project. All rights reserved.
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

class ChmKeyEvent;
class ChmRawInputStore;

class ChmEngine {
public:
	enum compositionStatus {
		COMP_NONE = 0,	// 初期状態
		COMP_ONGOING,	// Compositionが存在
		COMP_COMMITTING,// 確定キーの処理中
		COMP_RECALL,	// 再変換キーの処理中(v0.3以降で実装予定)
	};
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
	void _InitComposition(const ChmKeyEvent& keyEvent);
	void _UpdateComposition(const ChmKeyEvent& keyEvent);
    BOOL _isON;
	ChmRawInputStore* _pRawInputStore;
	std::wstring _compositionStr ;
	enum compositionStatus _compositionStatus; // v0.3以降でサブクラス化を予定
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
    static bool IsNormalKey(WPARAM wp);

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

