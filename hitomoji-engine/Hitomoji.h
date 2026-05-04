#pragma once

#define HM_VERSION L"0.5pre.1"

enum class ChmFuncType {
	None = 0,
	CharInput,          // 通常文字入力
	CharInputSpace,     // 空白文字の入力(Compositionがない場合用)
	CompFinish,         // 見たまま確定(ENTER)
	CompFinishHiragana, // ひらがな確定(Alt+Enter)
	CompFinishKatakana, // カタカナ確定(Shift+Enter)
	CompFinishKey,      // キーどおり確定(TAB)
	CompFinishKeyWide,  // キーどおり確定ワイド(Shift+TAB)
	CompSelect,         // 選択開始(かな漢字変換)
	SelectInput,        // 選択
	SelectNextPage,     // 選択中の次ページ
	SelectPrevPage,     // 選択中の次ページ
	SelectCancel,       // 選択中のキャンセル
	Cancel,             // キャンセル(ESC)
	Backspace,          // 後退(BS)
	UnFinish,           // 確定取消（将来）(CTRL+Z)
	VersionInfo,        // バージョン表示
	ReloadIni,          // iniファイルのリロード
};
