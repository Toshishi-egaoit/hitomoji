// ChmL3Kanji.cpp
// v0.4 Layer3（単漢字選択エンジン）

#include <vector>
#include <string>
#include <algorithm>

struct YomiEntry {
    char16_t yomi[6];
    uint16_t count;
    uint32_t offset;
};

class ChmL3Kanji {
public:
    ChmL3Kanji()
        : _active(false), _list(nullptr), _count(0), _page(0) {}

    // 変換開始
    bool Start(const char16_t key[6],
               const std::vector<YomiEntry>& entries,
               const std::vector<uint32_t>& kanjiList)
    {
        const YomiEntry* e = findYomi(key, entries);
        if (!e) {
            _active = false;
            return false;
        }

        _list = &kanjiList[e->offset];
        _count = e->count;
        _page = 0;
        _active = true;
        return true;
    }

    void Cancel()
    {
        _active = false;
        _list = nullptr;
        _count = 0;
        _page = 0;
    }

    bool IsActive() const { return _active; }

    void NextPage(size_t pageSize)
    {
        if (!_active) return;
        size_t maxPage = (_count + pageSize - 1) / pageSize;
        if (_page + 1 < maxPage) _page++;
    }

    void PrevPage()
    {
        if (!_active) return;
        if (_page > 0) _page--;
    }

    uint32_t SelectByIndex(size_t index, size_t pageSize)
    {
        if (!_active) return 0;

        size_t actual = (_page * pageSize) + index;
        if (actual >= _count) return 0;

        _active = false;
        return _list[actual];
    }

    const uint32_t* GetList() const { return _list; }
    uint16_t GetCount() const { return _count; }
    size_t GetPage() const { return _page; }

private:
    const YomiEntry* findYomi(const char16_t* key,
                             const std::vector<YomiEntry>& entries)
    {
        int left = 0;
        int right = (int)entries.size() - 1;

        while (left <= right) {
            int mid = (left + right) / 2;
            const auto& e = entries[mid];

            int cmp = std::memcmp(key, e.yomi, 6 * sizeof(char16_t));

            if (cmp == 0) return &e;
            if (cmp < 0) right = mid - 1;
            else left = mid + 1;
        }
        return nullptr;
    }

private:
    bool _active;
    const uint32_t* _list;
    uint16_t _count;
    size_t _page;
};


// =========================
// ChmLayer3Helper (keymap / config対応)
// =========================

class ChmLayer3Helper {
public:
    ChmLayer3Helper()
    {
        InitDefault();
    }

	// デフォルトの選択キー定義
    void InitDefault()
    {
        std::string map = "dkfj ei sla; ruwoqpty  c,vmx.z/bn 3847291056";
        BuildTable(map);
    }

	// ChmConfigで使うため（未実装)
    void SetKeymap(const std::string& map)
    {
        BuildTable(map);
    }

    int KeyToIndex(unsigned char key) const
    {
        return _keyToIndex[key];
    }

    size_t GetPageSize() const
    {
        return _pageSize;
    }

private:
    void BuildTable(const std::string& raw)
    {
        std::fill(std::begin(_keyToIndex), std::end(_keyToIndex), -1);

        std::string map;
        for (char c : raw) {
            if (!isspace((unsigned char)c)) map.push_back(c);
        }

        _pageSize = map.size();

        for (size_t i = 0; i < map.size(); ++i) {
            _keyToIndex[(unsigned char)map[i]] = (int)i;
        }
    }

private:
    int _keyToIndex[256];
    size_t _pageSize;
};
#pragma once
