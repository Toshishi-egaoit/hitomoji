// ChmEngine.h
// Copyright (C) 2026 by Hitomoji Project. All rights reserved.
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include "utils.h"

class ChmRawInputStore;

#define VK_HITOMOJI 0 // 仮想キーコード: マウスイベントなどの特殊用途

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
    };

    struct CharKeyDef {
        WPARAM wp;
        bool   needShift;
        char   ch;               // CharInput 用
    };

    // --- utility ---
	const std::wstring toString() const { 
		wchar_t buff[80];
		wchar_t ch = GetChar();
		wsprintf(buff, L"Type:%d ch=%d(%c) %s %s %s",
			_type,
			ch , 
			((ch < 0x20) ? L'-' : ch & 0x7f),
			_shift ?   L"Shift"  : L"",
			_control ? L"Control": L"",
			_alt ?     L"Alt"    : L"");
		return buff ;
	}

    // --- ctor ---
    ChmKeyEvent(WPARAM wp, LPARAM lp);
	ChmKeyEvent(ChmKeyEvent::Type type); // マウスクリックなどの特殊用途（OnEndEditで使用）

    // --- accessors ---
    Type GetType() const { return _type; }
	wchar_t GetChar() const ;
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

    Type _type = Type::None;
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
	BOOL HasComposition() { return _hasComposition;} ;
	std::wstring GetCompositionStr() ;

	void UpdateComposition(const ChmKeyEvent& keyEvent, bool& pEndComposition);
	void PostUpdateComposition();
	void ResetStatus() ;

private:
    // ASCII -> 全角 変換（v0.1.3 簡易実装）
    static std::wstring AsciiToWide(const std::wstring& src);

    BOOL _isON;
	BOOL _hasComposition;
	ChmRawInputStore* _pRawInputStore; // 入力されたローマ字列
	std::wstring _converted; // かな変換できた部分
	std::wstring _pending; // かなに変換できていない部分（残り）
};

