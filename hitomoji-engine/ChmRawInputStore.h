// ChmRawInputStore.h
#pragma once
#include <string>
#include <vector>
#include "ChmRomajiConverter.h"

// rawInput を唯一の真実として保持するクラス
class ChmRawInputStore {
public:
    // ASCII 1文字入力
    void push(wchar_t c) {
        rawInput_.push_back(c);
        _RefreshProvisionalBoundaries();
    }

    // UnFinish 用のバックアップ
    void backup() {
        backupInput_ = rawInput_;
    }

    // UnFinish 時に戻す
    void restore() {
        rawInput_ = backupInput_;
        _provisionalBoundaries.clear();
    }

    bool hasBackup() const {
        return !backupInput_.empty();
    }

    void clearBackup() {
        backupInput_.clear();
    }

    // rawInput 全消去（確定・ESC 用）
    void clear() {
        rawInput_.clear();
        _provisionalBoundaries.clear();
    }

    std::wstring get() const {
        return rawInput_.substr(_ProvisionalRawLength());
    }

    const std::wstring& GetAllStr() const {
        return rawInput_;
    }

    size_t GetProvisionalLength() const {
        return _provisionalBoundaries.empty() ? 0 : _provisionalBoundaries.back().displayLength;
    }

    std::wstring GetProvisionalStr() const {
        if (_provisionalBoundaries.empty()) return {};
        std::wstring converted, pending;
        ChmRomajiConverter::convert(rawInput_, converted, pending, true);
        return converted.substr(0, GetProvisionalLength());
    }

    bool AddProvisionalOne() {
        Boundary next{};
        if (!_TryGetNextBoundary(_ProvisionalRawLength(), GetProvisionalLength(), next)) return false;
        _provisionalBoundaries.push_back(next);
        return true;
    }

    void RemoveProvisionalOne() {
        if (!_provisionalBoundaries.empty()) _provisionalBoundaries.pop_back();
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
        _RefreshProvisionalBoundaries();
        return true;
    }

private:
    struct Boundary {
        size_t rawLength;
        size_t displayLength;
    };

    size_t _ProvisionalRawLength() const {
        return _provisionalBoundaries.empty() ? 0 : _provisionalBoundaries.back().rawLength;
    }

    bool _TryGetNextBoundary(size_t rawLength, size_t displayLength, Boundary& next) const {
        ChmRomajiConverter::Unit unit;
        if (!ChmRomajiConverter::TryConvertOneForProvisionalFinish(rawInput_, rawLength, unit)) return false;
        next = {rawLength + unit.rawLength, displayLength + unit.output.size()};
        return true;
    }

    void _RefreshProvisionalBoundaries() {
        Boundary previous{};
        size_t count = 0;
        for (const auto& saved : _provisionalBoundaries) {
            Boundary next{};
            if (!_TryGetNextBoundary(previous.rawLength, previous.displayLength, next)
                || next.rawLength != saved.rawLength || next.displayLength != saved.displayLength) {
                break;
            }
            previous = next;
            ++count;
        }
        _provisionalBoundaries.resize(count);
    }

    std::wstring rawInput_;
    std::wstring backupInput_;
    std::vector<Boundary> _provisionalBoundaries;
};


