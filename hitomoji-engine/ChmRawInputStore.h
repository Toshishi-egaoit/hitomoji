// ChmRawInputStore.h
#pragma once
#include <string>
#include <algorithm>
#include <cctype>

// rawInput を唯一の真実として保持するクラス
class ChmRawInputStore {
public:
    // ASCII 1文字入力
    void push(wchar_t c) {
        rawInput_.push_back(c);
    }

    // rawInput 全消去（確定・ESC 用）
    void clear() {
        rawInput_.clear();
    }

    const std::wstring& get() const {
        return rawInput_;
    }

    bool empty() const {
        return rawInput_.empty();
    }

    size_t size() const {
        return rawInput_.size();
    }

    // rawInput から指定数の文字を巻き戻す
    bool pop(size_t deleteRawLength) {
        if (deleteRawLength == 0) return false;
        if (deleteRawLength > rawInput_.size()) {
            rawInput_.clear();
        } else {
            rawInput_.erase(rawInput_.size() - deleteRawLength);
        }
        return true;
    }

private:
    std::wstring rawInput_;
};


