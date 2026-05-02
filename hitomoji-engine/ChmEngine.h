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
class ChmL3KanjiDict ;
class ChmL3KanjiSelect ;
class ChmL3Helper ;

#define VK_HITOMOJI 0 // 仮想キーコード: マウスイベントなどの特殊用途

class ChmEngine {
public:
	enum class State {
		None = 1,
		Inputing ,
		Selecting,
	} ;

	ChmEngine();
	~ChmEngine();

	// Activate/Deactivate
	void Activate();
	void Deactivate();

	// ChmConfigの処理ヘルパ
	void InitConfig();
	std::wstring GetConfigFile() ; 
	std::wstring GetConfigPath() ; 
	
	// キーをIMEで処理すべきか判定
	BOOL IsKeyEaten(WPARAM wp);
	
	// IMEのON/OFF
	void ToggleIME() { _isON = !_isON; }

	BOOL IsON() const { return _isON; }
	BOOL IsSelecting() const { return _state == State::Selecting ; }
	BOOL HasComposition() { return (_state == State::Selecting || _state == State::Inputing); }
	BOOL CanUnFinish() const;
	BOOL UseUndoEditSession() const { return _useUndoEditSession; }
	LONG GetUndoDeleteLength() const { return _undoDeleteLength; }
	BOOL IsDirectInput(ChmFuncType tp) { return (tp == ChmFuncType::CharInputSpace) ; }
	State GetState() { return _state ;}
	std::wstring GetCompositionStr() ;

	BOOL UpdateComposition(const ChmKeyEvent& keyEvent, bool& pEndComposition);
	void PostUpdateComposition();
	void ResetStatus() ;

private:
	// Activate/Deactivateでの初期処理
	void _initEnv();
	void _initLayer3();
	void _initLayer2();

	// ASCII -> 全角 変換（v0.1.3 簡易実装）
	static std::wstring AsciiToWide(const std::wstring& src);
	static LONG CountUndoDeleteLength(const std::wstring& src);

	void SetError(void) ;
	void _PrepareUnFinish();
	void _ClearUnFinish();

	// --- static members ---
	ChmConfig* _pConfig;

	State _state ;
	BOOL _isON;
	ChmRawInputStore* _pRawInputStore; // 入力されたローマ字列
	ChmL3KanjiDict* _pL3KanjiDict; // かな漢字変換辞書
	ChmL3KanjiSelect* _pL3KanjiSelect; // かな漢字変換中
	ChmL3Helper* _pL3Helper; // かな漢字変換の選択キー定義

	std::wstring _converted; // かな変換できた部分
	std::wstring _pending; // かなに変換できていない部分（残り）
	BOOL _useUndoEditSession;
	LONG _undoDeleteLength;
};
