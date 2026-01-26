// ChmConfig.h
#pragma once

// Hitomoji v0.1.5 config store definition
// 設定は immutable（変更不可）で、参照専用とする

struct ChmConfigStore {
    // BS で削除する単位
    enum class BackspaceUnit {
        Ascii,   // ASCII 1文字単位
        Symbol   // 意味単位（かな・記号など）
    };

    // 表示モード
    enum class DisplayMode {
        Kana,        // かな表示
        Alphabet     // ローマ字表示
    };

    const BackspaceUnit backspaceUnit;
    const DisplayMode displayMode;

    // コンストラクタ（v0.1.5 では固定値のみを想定）
    ChmConfigStore(BackspaceUnit bs, DisplayMode dm)
        : backspaceUnit(bs), displayMode(dm) {
    }
};

// グローバル設定インスタンス（実体は ChmConfig.cpp に定義）
extern const ChmConfigStore g_config;
