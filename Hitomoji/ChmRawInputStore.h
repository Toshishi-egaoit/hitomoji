// ChmRawInputStore.h
#pragma once
#include <string>

// rawInput を唯一の真実として保持するクラス
// v0.1.1 では文字追加のみ実装
// BS / ESC 等は将来ここに集約する
class ChmRawInputStore {
public:
    // 文字入力（ASCII前提）
    void push(char c) {
        rawInput_.push_back(c);
    }

    // rawInput を全消去（確定・ESC用）
    void clear() {
        rawInput_.clear();
    }

    // 現在の rawInput を取得
    const std::string& get() const {
        return rawInput_;
    }

    // 長さ確認（TSF側の判断用）
    size_t size() const {
        return rawInput_.size();
    }

    bool empty() const {
        return rawInput_.empty();
    }

private:
    std::string rawInput_; // ryo-bi など
};
