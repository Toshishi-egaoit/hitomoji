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
		_rawInput.push_back(c);
	}

    // UnFinish用のバックアップ
    void backup() {
		_lastInput = _rawInput ;
        _rawInput.clear();
    }

    // UnFinish時に戻す処理
    void restore() {
		_rawInput = _lastInput ;
        _lastInput.clear();
    }

    // rawInput 全消去（確定・ESC 用）
    void clear() {
        _rawInput.clear();
    }

    const std::wstring& get() const {
        return _rawInput;
    }

    bool empty() const {
        return _rawInput.empty();
    }

    size_t size() const {
        return _rawInput.size();
    }

    // rawInput から指定数の文字を巻き戻す
    bool pop(size_t deleteRawLength) {
        if (deleteRawLength == 0) return false;
        if (deleteRawLength > _rawInput.size()) {
            _rawInput.clear();
        } else {
            _rawInput.erase(_rawInput.size() - deleteRawLength);
        }
        return true;
    }

private:
    std::wstring _rawInput;
    std::wstring _lastInput;
};
