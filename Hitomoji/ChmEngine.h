// ChmEngine.h
// Copyright (C) 2026 by Hitomoji Project. All rights reserved.
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include "utils.h"

class ChmRawInputStore;

class ChmKeyEvent {
public:
    enum class Type {
        None = 0,
        CharInput,          // 通常文字入力
        CommitKana,         // Enter
        CommitKatakana,     // Shift+Enter
        CommitAscii,        // TAB
        CommitAsciiWide,    // Shift+TAB
        CommitNonConvert,   // 無変換確定（カーソルキーなど）
        Cancel,             // ESC
        Backspace,          // BS
        Uncommit,           // 確定取消（将来）
    };

    struct FuncKeyDef {
        WPARAM wp;
        bool   needShift;
        bool   needCtrl;
        bool   needAlt;
        Type   type;
		// bool   endComposition;   // これは静的には決まらないので廃止
    };

    struct CharKeyDef {
        WPARAM wp;
        bool   needShift;
        char   ch;               // CharInput 用
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
//    bool ShouldEndComposition() const {return _endComp ;};
    Type GetType() const { return _type; }
    char GetChar() const { return _ch; }
    bool IsShift() const { return _shift; }
	// ナビゲーションキーか？(処理はしないが、確定処理が必要なキーの判定用)
    bool IsNavigationKey() const { 
		switch (_wp) {
			case VK_LEFT:
			case VK_RIGHT:
			case VK_UP:
			case VK_DOWN:
			case VK_HOME:
			case VK_END:
			case VK_PRIOR: // PageUp
			case VK_NEXT:  // PageDown
				return true;
		}
		return false;
	}

private:
    void _TranslateByTable();

    WPARAM _wp;
    bool   _shift   = false;
    bool   _control = false;
    bool   _alt     = false;
    bool   _caps    = false;
    bool   _endComp = false;

    Type _type = Type::None;
    char _ch   = 0;
};

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

	void UpdateComposition(const ChmKeyEvent& keyEvent, bool& pEndComposition);
	void PostUpdateComposition();
	void ResetStatus() ;

private:
    // ASCII -> 全角 変換（v0.1.3 簡易実装）
    static std::wstring AsciiToWide(const std::string& src);

    BOOL _isON;
	BOOL _hasComposition;
	ChmRawInputStore* _pRawInputStore; // 入力されたローマ字列
	std::wstring _converted; // かな変換できた部分
	std::string _pending; // かなに変換できていない部分（残り）
};

