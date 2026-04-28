// ChmL3Kanji.cpp
// v0.4 Layer3（単漢字選択エンジン）

#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include "ChmEnvironment.h"
#include "ChmConfig.h"


// かな漢字変換の辞書を管理するクラス(Activate/Deactivateが生存期間）
class ChmL3KanjiDict {

public:
	struct DictHeader {
		uint32_t magic;
		uint32_t kanjiCount;
		uint32_t yomiCount;
		uint32_t kanjiOffset;
		uint32_t yomiOffset;
		uint32_t listOffset;
	};

	struct YomiEntry {
		char16_t DictYomi[6];
		uint16_t count;
		uint32_t offset;
	};
	// データはファイルから読み込んでここに保持する設計
	std::vector<YomiEntry> DictYomi;
	std::vector<uint32_t> kanjiList;

	ChmL3KanjiDict()
	{
		LoadDict();
	};

	bool LoadDict()
	{
		Info(L"L3kanjiDict.LoadDict");
		std::wstring path = g_environment.GetBasePath() + L"hitomoji.dic";

		std::ifstream ifs(path, std::ios::binary);
		if (!ifs) {
			Error(Format(L"   > failed to open dic file:%s" , path));
			return false;
		}

		DictHeader header;
		ifs.read((char*)&header, sizeof(header));

		ifs.seekg(header.yomiOffset);

		DictYomi.resize(header.yomiCount);
		ifs.read((char*)DictYomi.data(), header.yomiCount * sizeof(YomiEntry));
		Info(Format(L"   > read yomiCount: %d", header.yomiCount));

		ifs.seekg(header.listOffset);
		auto cur = ifs.tellg();
		ifs.seekg(0, std::ios::end);
		size_t bytes = (size_t)(ifs.tellg() - cur);

		kanjiList.resize(bytes / sizeof(uint32_t));
		ifs.seekg(header.listOffset);
		ifs.read((char*)kanjiList.data(), bytes);
		Info(Format(L"   > read kanjiList: %d" ,bytes));

		return true;
	}
};


// １回のかな漢字変換セッションを管理するクラス
class ChmL3KanjiSelect {

public:
	ChmL3KanjiSelect(ChmL3KanjiDict* pDict, byte pageSize)
		: _pDict(pDict),_list(nullptr), _count(0), _page(0),_pageSize(pageSize) {
	};

	// 変換開始
	BOOL Start(const std::wstring& DictYomi)
	{
		char16_t key[6];
		_makeYomiKey(DictYomi, key);
		
		const ChmL3KanjiDict::YomiEntry* e = _findYomi(key);
		if (!e) {
			return FALSE;
		}
		Debug(Format(L"   > found yomi: %s, count: %d", DictYomi.c_str(), e->count));

		_list = &_pDict->kanjiList[e->offset];
		_count = e->count;
		_page = 0;
		return TRUE;
	}

	void Cancel()
	{
		_list = nullptr;
		_count = 0;
		_page = 0;
	}

	void NextPage()
	{
		byte maxPage = (_count + _pageSize - 1) / _pageSize;
		if (_page + 1 < maxPage) _page++;
	}

	void PrevPage()
	{
		byte maxPage = (_count + _pageSize - 1) / _pageSize;
		if (_page == 0 ) _page = maxPage - 1;
		else if (_page > 0) _page--;
	}

	uint32_t SelectByIndex(byte index)
	{
		size_t slotNo = (_page * _pageSize) + index;
		Debug(Format(L"SelectByIndex: index=%d, page=%d, slotNo=%d", index, _page, slotNo));
		if (slotNo >= _count) return 0;

		return _list[slotNo];
	}

	const uint32_t* GetList() const { return _list; }
	uint16_t GetCount() const { return _count; }
	byte GetPage() const { return _page; }

private:
	// wstringから最大5文字までのchar16_t配列に変換
	void _makeYomiKey(const std::wstring& src, char16_t out[6])
	{
		// 0クリア
		std::fill(out, out + 6, 0);

		// コピー（最大5文字まで）
		size_t len = std::min<size_t>(src.size(), 5);
		for (size_t i = 0; i < len; ++i) {
			out[i] = (char16_t)src[i];
		}
		Debug(Format(L"   Search:>>%s<<", out));
	}

	const ChmL3KanjiDict::YomiEntry* _findYomi(const char16_t* key)
	{
		int left = 0;
		int right = (int)_pDict->DictYomi.size() - 1;

		while (left <= right) {
			int mid = (left + right) / 2;
			const auto& e = _pDict->DictYomi[mid];

			int cmp = std::memcmp(key, e.DictYomi, 6 * sizeof(char16_t));
			Debug(Format(L"   B-tree:%s cmp=%d", e.DictYomi, cmp ));

			if (cmp == 0) return &e;
			if (cmp < 0) right = mid - 1;
			else left = mid + 1;
		}
		return nullptr;
	}

private:
	ChmL3KanjiDict* _pDict;
	const uint32_t* _list;
	uint16_t _count;
	byte _page;
	byte _pageSize;
};


// =========================
// ChmLayer3Helper (keymap / config対応)
// =========================

class ChmL3Helper {
public:
	ChmL3Helper(ChmConfig* pConfig)
	{
		InitDefault(pConfig);
	}

	// デフォルトの選択キー定義
	void InitDefault(ChmConfig* pConfig)
	{
		std::wstring map = pConfig->GetString(L"UI", L"select_keymap");
		if (map.empty()) {
			map = L"dk fj sl gh ei ru a; wo qp ty c, vm x. z/ bn 38 47 29 10 56";
		}
		BuildTable(ToNarrow(map));
	}

	// ChmConfigで使うため（未実装)
	void SetKeymap(const std::string& map)
	{
		BuildTable(map);
	}

	int KeyToIndex(unsigned char key) const
	{
		Debug(Format(L"KeyToIndex: key=%c(%02X) -> index=%d", key, key, _keyToIndex[key]));
		return _keyToIndex[key];
	}

	size_t GetPageSize() const
	{
		return _pageSize;
	}

	// UTF32コードポイントをwstringに変換するユーティリティ（選択候補の表示に使う）
	static std::wstring Utf32ToWString(uint32_t cp)
	{
		std::wstring result;

		if (cp <= 0xFFFF) {
			// BMP（そのまま）
			result.push_back((wchar_t)cp);
		}
		else {
			// サロゲートペア
			cp -= 0x10000;
			wchar_t high = 0xD800 + (cp >> 10);
			wchar_t low  = 0xDC00 + (cp & 0x3FF);

			result.push_back(high);
			result.push_back(low);
		}

		return result;
	}

private:
	void BuildTable(const std::string& raw)
	{
		std::fill(std::begin(_keyToIndex), std::end(_keyToIndex), -1);

		std::string map;
		for (char c : raw) {
			if (!isspace((unsigned char)c)) map.push_back(c);
		}

		_pageSize = (byte)map.size();

		for (size_t i = 0; i < map.size(); ++i) {
			_keyToIndex[(unsigned char)map[i]] = (int)i;
		}
#ifdef _DEBUG
		OutputDebugString(L"[Hitomoji] L3Helper::BuildTable\n1234567890123456789012345678901234567890");
		OutputDebugStringA(map.c_str());
#endif
	};

	int _keyToIndex[256];
	byte _pageSize;
};
