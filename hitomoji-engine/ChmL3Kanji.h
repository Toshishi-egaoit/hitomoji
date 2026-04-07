// ChmL3Kanji.cpp
// v0.4 Layer3（単漢字選択エンジン）

#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include "ChmEnvironment.h"


class ChmL3Kanji {

private:
	struct DictHeader {
		uint32_t magic;
		uint32_t kanjiCount;
		uint32_t yomiCount;
		uint32_t kanjiOffset;
		uint32_t yomiOffset;
		uint32_t listOffset;
	};

	struct YomiEntry {
		char16_t yomi[6];
		uint16_t count;
		uint32_t offset;
	};
	// データはファイルから読み込んでここに保持する設計
	std::vector<YomiEntry> yomi;
	std::vector<uint32_t> kanjiList;

public:
	ChmL3Kanji()
		: _active(false), _list(nullptr), _count(0), _page(0) {
		LoadDict();
	};

	bool LoadDict()
	{
		ChmLogger::Info(L"[hitomoji] L3kanji.LoadDict");
		std::wstring path = g_environment.GetBasePath() + L"hitomoji.dicn";

		std::ifstream ifs(path, std::ios::binary);
		if (!ifs) {
			ChmLogger::Error(L"   > failed to open dic file:" + path);
			return false;
		}

		DictHeader header;
		ifs.read((char*)&header, sizeof(header));

		ifs.seekg(header.yomiOffset);

		yomi.resize(header.yomiCount);
		ifs.read((char*)yomi.data(), header.yomiCount * sizeof(YomiEntry));
		ChmLogger::Info(L"   > read yomiCount: " + header.yomiCount);

		ifs.seekg(header.listOffset);
		auto cur = ifs.tellg();
		ifs.seekg(0, std::ios::end);
		size_t bytes = (size_t)(ifs.tellg() - cur);

		kanjiList.resize(bytes / sizeof(uint32_t));
		ifs.seekg(header.listOffset);
		ifs.read((char*)kanjiList.data(), bytes);
		ChmLogger::Info(L"   > read kanjiList: " + (int)bytes);

		return true;
	}

	// wstringから最大5文字までのchar16_t配列に変換
	void MakeYomiKey(const std::wstring& src, char16_t out[6])
	{
		// 0クリア
		std::fill(out, out + 6, 0);

		// コピー（最大5文字まで）
		size_t len = std::min<size_t>(src.size(), 5);
		for (size_t i = 0; i < len; ++i) {
			out[i] = (char16_t)src[i];
		}
	}

	// 変換開始
	bool Start(const std::wstring& yomi)
	{
		char16_t key[6];
		MakeYomiKey(yomi, key);
		
		const YomiEntry* e = _findYomi(key);
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
	const YomiEntry* _findYomi(const char16_t* key)
	{
		int left = 0;
		int right = (int)yomi.size() - 1;

		while (left <= right) {
			int mid = (left + right) / 2;
			const auto& e = yomi[mid];

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

class ChmL3Helper {
public:
	ChmL3Helper()
	{
		InitDefault();
	}

	// デフォルトの選択キー定義
	void InitDefault()
	{
		std::string map = "dkfj ei sla; ruwoqpty c,vmx.z/bn 3847291056";
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
#ifdef _DEBUG
		OutputDebugString(L"[Hitomoji] L3Helper::BuidTable\n1234567890123456789012345678901234567890");
		OutputDebugStringA(map.c_str());
#endif
	};

	int _keyToIndex[256];
	size_t _pageSize;
};
