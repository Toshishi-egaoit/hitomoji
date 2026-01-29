// ChmConfig.cpp
// Hitomoji v0.1.5 config store (fixed values)
#include "ChmConfig.h"

// グローバル設定インスタンス
// v0.1.5 では固定値のみ（永続化・変更なし）
const ChmConfigStore g_config(
    ChmConfigStore::BackspaceUnit::Symbol,
    ChmConfigStore::DisplayMode::Kana 
);
