// ChmEngine.h
// Copyright (C) 2026 by Hitomoji Project. All rights reserved.
#pragma once
#include <windows.h>
#include <cstdint>
#include <string>
#include "utils.h"
#include "hitomoji.h"

class ChmRawInputStore;
class ChmConfig;
class ChmKeyEvent ;

#define VK_HITOMOJI 0 // 仮想キーコード: マウスイベントなどの特殊用途

class ChmEngine {
public:
    ChmEngine();
    ~ChmEngine();

    // g_configの初期化処理
	static void InitConfig();
    
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

	// --- static members ---
	static ChmConfig* _pConfig;

    BOOL _isON;
	BOOL _hasComposition;
	ChmRawInputStore* _pRawInputStore; // 入力されたローマ字列
	std::wstring _converted; // かな変換できた部分
	std::wstring _pending; // かなに変換できていない部分（残り）
};