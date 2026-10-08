// ChmL3Kanji.cpp
// v0.4 Layer3（単漢字選択エンジン）

#include <vector>
#include <string>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <unordered_set>
#include "ChmEnvironment.h"
#include "ChmConfig.h"
#include "ChmCandidatePage.h"
#include "utils.h"

class ChmL3Helper;

// かな漢字変換の辞書を管理するクラス(Activate/Deactivateが生存期間）
class ChmL3KanjiDict {

public:
	static constexpr uint32_t MagicHMJ1 = 0x314A4D48;
	static constexpr uint32_t MagicHMJ2 = 0x324A4D48;
	struct DictHeaderV2 {
		uint32_t magic;
		uint32_t flags;
		uint32_t kanjiCount;
		uint32_t yomiCount;
		uint32_t variantCount;
		uint32_t kanjiOffset;
		uint32_t yomiOffset;
		uint32_t listOffset;
		uint32_t variantOffset;
		uint32_t infoOffset;
		uint32_t infoSize;
	};
	static_assert(sizeof(DictHeaderV2) == 44, "HMJ2 header layout");
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
		DictYomi.clear();
		kanjiList.clear();
		Info(L"L3kanjiDict.LoadDict");
		std::wstring path = g_environment.GetBasePath() + L"hitomoji.dic";

		std::ifstream ifs(path, std::ios::binary);
		if (!ifs) {
			Error(Format(L"   > failed to open dic file:%s" , path));
			return false;
		}

		if (!LoadDict(ifs)) {
			Error(L"   > invalid or unsupported dictionary");
			return false;
		}
		return true;
	}

	// HMJ2 metadata is not used for selection yet. Load only the legacy candidate data.
	bool LoadDict(std::istream& input)
	{
		static_assert(sizeof(DictHeader) == 24, "HMJ1 header layout");
		static_assert(sizeof(YomiEntry) == 20, "Yomi entry layout");
		DictYomi.clear();
		kanjiList.clear();
		input.seekg(0, std::ios::end);
		const auto end = input.tellg();
		if (end < std::streampos(sizeof(uint32_t))) return false;
		const uint64_t fileSize = static_cast<uint64_t>(static_cast<std::streamoff>(end));
		input.seekg(0);
		uint32_t magic = 0;
		if (!input.read(reinterpret_cast<char*>(&magic), sizeof(magic))) return false;
		input.seekg(0);
		DictHeader header{};
		uint64_t headerSize = sizeof(DictHeader);
		uint64_t listEnd = fileSize;
		if (magic == MagicHMJ1) {
			if (!input.read(reinterpret_cast<char*>(&header), sizeof(header))) return false;
		}
		else if (magic == MagicHMJ2) {
			DictHeaderV2 v2{};
			if (!input.read(reinterpret_cast<char*>(&v2), sizeof(v2))) return false;
			header = {v2.magic, v2.kanjiCount, v2.yomiCount,
				v2.kanjiOffset, v2.yomiOffset, v2.listOffset};
			headerSize = sizeof(v2);
			// Zero offsets denote absent optional blocks; empty blocks may have offsets.
			if (v2.infoOffset != 0) {
				if (v2.infoOffset < v2.listOffset ||
					uint64_t(v2.infoOffset) + v2.infoSize > fileSize) return false;
				listEnd = v2.infoOffset;
			}
			else if (v2.infoSize != 0) return false;
			if (v2.variantOffset != 0) {
				if (v2.variantOffset < v2.listOffset ||
					uint64_t(v2.variantOffset) + uint64_t(v2.variantCount) * 12 > listEnd) return false;
				listEnd = v2.variantOffset;
			}
			else if (v2.variantCount != 0) return false;
		}
		else return false;

		if (header.kanjiOffset < headerSize ||
			uint64_t(header.kanjiOffset) + uint64_t(header.kanjiCount) * 8 > header.yomiOffset ||
			uint64_t(header.yomiOffset) + uint64_t(header.yomiCount) * sizeof(YomiEntry) > header.listOffset ||
			header.listOffset > listEnd || listEnd > fileSize) return false;
		const uint64_t bytes = listEnd - header.listOffset;
		if (bytes % sizeof(uint32_t) != 0) return false;
		std::vector<YomiEntry> yomis(header.yomiCount);
		std::vector<uint32_t> candidates(static_cast<size_t>(bytes / sizeof(uint32_t)));
		input.seekg(header.yomiOffset);
		if (!yomis.empty() && !input.read(reinterpret_cast<char*>(yomis.data()),
			static_cast<std::streamsize>(yomis.size() * sizeof(YomiEntry)))) return false;
		input.seekg(header.listOffset);
		if (!candidates.empty() && !input.read(reinterpret_cast<char*>(candidates.data()),
			static_cast<std::streamsize>(bytes))) return false;
		for (const auto& yomi : yomis) {
			if (uint64_t(yomi.offset) + yomi.count > candidates.size()) return false;
		}
		DictYomi.swap(yomis);
		kanjiList.swap(candidates);
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
	BOOL Start(const std::wstring& composition)
	{
		_leadingChars.clear();
		std::wstring dictYomi = _makeDictYomi(composition);

		std::vector<std::wstring> yomis;
		if (!_parseSearchQuery(dictYomi, yomis)) {
			return FALSE;
		}
		if (yomis.empty()) {
			return FALSE;
		}

		const ChmL3KanjiDict::YomiEntry* first = _findYomi(yomis[0]);
		if (!first) {
			return FALSE;
		}
		Debug(Format(L"   > found yomi: %s, count: %d", yomis[0].c_str(), first->count));

		_ownedList.clear();
		if (yomis.size() == 1) {
			_list = &_pDict->kanjiList[first->offset];
			_count = first->count;
		}
		else if (!_buildAndList(yomis, *first)) {
			return FALSE;
		}
		_page = 0;
		return TRUE;
	}
	void Cancel()
	{
		_list = nullptr;
		_count = 0;
		_page = 0;
		_ownedList.clear();
		_leadingChars.clear();
	}
	void NextPage()
	{
		if (!_list || _count == 0 || _pageSize == 0) return;
		byte maxPage = (_count + _pageSize - 1) / _pageSize;
		_page = (_page + 1 < maxPage) ? _page + 1 : 0;
	}

	void PrevPage()
	{
		if (!_list || _count == 0 || _pageSize == 0) return;
		byte maxPage = (_count + _pageSize - 1) / _pageSize;
		_page = (_page > 0) ? _page - 1 : maxPage - 1;
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
	const std::wstring& GetLeadingChars() const { return _leadingChars; }
	BOOL BuildCandidatePage(ChmCandidatePage& page, const ChmL3Helper& helper) const;

private:

	std::wstring _makeDictYomi(const std::wstring& composition)
	{
		size_t start = 0;
		while (start < composition.length() && !_isYomiChar(composition[start])) {
			++start;
		}

		if (start == 0 || start == composition.length()) {
			return composition;
		}

		_leadingChars = composition.substr(0, start);
		return composition.substr(start);
	}

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

	BOOL _isYomiChar(wchar_t ch) const
	{
		return (ch >= 0x3040 && ch <= 0x309F) || (ch >= 0x30A0 && ch <= 0x30FF);
	}

	BOOL _isSearchOperator(wchar_t ch) const
	{
		return ch == L':' || ch == L'*';
	}

	BOOL _appendYomi(const std::wstring& query, size_t begin, size_t end, std::vector<std::wstring>& yomis) const
	{
		if (begin >= end) return FALSE;
		for (size_t i = begin; i < end; ++i) {
			if (!_isYomiChar(query[i])) return FALSE;
		}
		yomis.push_back(query.substr(begin, end - begin));
		return TRUE;
	}

	BOOL _parseSearchQuery(const std::wstring& query, std::vector<std::wstring>& yomis) const
	{
		yomis.clear();
		if (query.empty()) return FALSE;

		size_t pos = 0;
		if (!_isSearchOperator(query[pos])) {
			size_t begin = pos;
			while (pos < query.length() && !_isSearchOperator(query[pos])) {
				++pos;
			}
			if (!_appendYomi(query, begin, pos, yomis)) return FALSE;
		}

		while (pos < query.length()) {
			wchar_t op = query[pos++];
			if (op == L':') {
				size_t begin = pos;
				while (pos < query.length() && !_isSearchOperator(query[pos])) {
					++pos;
				}
				if (!_appendYomi(query, begin, pos, yomis)) return FALSE;
			}
			else {
				return FALSE;
			}
		}

		return !yomis.empty();
	}

	const ChmL3KanjiDict::YomiEntry* _findYomi(const std::wstring& yomi)
	{
		char16_t key[6];
		_makeYomiKey(yomi, key);
		return _findYomi(key);
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

	BOOL _buildAndList(const std::vector<std::wstring>& yomis, const ChmL3KanjiDict::YomiEntry& first)
	{
		std::vector<std::unordered_set<uint32_t>> filters;
		filters.reserve(yomis.size() - 1);

		for (size_t i = 1; i < yomis.size(); ++i) {
			const ChmL3KanjiDict::YomiEntry* e = _findYomi(yomis[i]);
			if (!e) return FALSE;
			Debug(Format(L"   > found yomi: %s, count: %d", yomis[i].c_str(), e->count));

			std::unordered_set<uint32_t> slots;
			for (uint16_t j = 0; j < e->count; ++j) {
				uint32_t slot = _pDict->kanjiList[e->offset + j];
				if (slot != 0) slots.insert(slot);
			}
			filters.push_back(std::move(slots));
		}

		_ownedList.assign(first.count, 0);
		bool matchedAny = false;

		for (uint16_t i = 0; i < first.count; ++i) {
			uint32_t slot = _pDict->kanjiList[first.offset + i];
			if (slot == 0) continue;

			bool foundInAll = true;
			for (const auto& filter : filters) {
				if (filter.find(slot) == filter.end()) {
					foundInAll = false;
					break;
				}
			}
			if (foundInAll) {
				_ownedList[i] = slot;
				matchedAny = true;
			}
		}

		if (!matchedAny) return FALSE;

		_list = _ownedList.data();
		_count = first.count;
		return TRUE;
	}

private:
	ChmL3KanjiDict* _pDict;
	const uint32_t* _list;
	std::vector<uint32_t> _ownedList;
	uint16_t _count;
	byte _page;
	byte _pageSize;
	std::wstring _leadingChars;
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
			map = L"dk fj sl gh ei ru a; wo qp ty c, vm x. z/ bn 38 47";
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

	int IndexToCell(size_t index) const
	{
		if (index >= CHM_CANDIDATE_MAX) return -1;
		return _indexToCell[index];
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
		std::fill(std::begin(_keyToCell), std::end(_keyToCell), -1);
		std::fill(std::begin(_indexToCell), std::end(_indexToCell), -1);

		const char* rows[] = {
			"1234567890",
			"qwertyuiop",
			"asdfghjkl;",
			"zxcvbnm,./"
		};
		for (size_t row = 0; row < 4; ++row) {
			for (size_t col = 0; col < 10; ++col) {
				unsigned char key = static_cast<unsigned char>(rows[row][col]);
				_keyToCell[key] = static_cast<int>(row * 10 + col);
			}
		}

		std::string map;
		for (char c : raw) {
			if (!isspace((unsigned char)c)) map.push_back(c);
		}
		if (map.size() > CHM_CANDIDATE_MAX) {
			map.resize(CHM_CANDIDATE_MAX);
		}

		_pageSize = (byte)map.size();

		Debug(L"L3Helper::BuildTable");
		for (size_t i = 0; i < map.size(); ++i) {
			unsigned char key = (unsigned char)map[i];
			_keyToIndex[key] = (int)i;
			_indexToCell[i] = _keyToCell[key];
			Debug(Format(L"   > [%c]=%d cell=%d" , map[i] , (int)i, _indexToCell[i]));
		}
		if (!map.empty()) {
			Info(Format(L"L3Helper::BuildTable >[%c]=%d / [%c]=%d",
				map[0], _keyToIndex[(unsigned char)map[0]],
				map[map.size() - 1], _keyToIndex[(unsigned char)map[map.size() - 1]]));
		}
	};

	int _keyToIndex[256];
	int _keyToCell[256];
	int _indexToCell[CHM_CANDIDATE_MAX];
	byte _pageSize;
};

inline BOOL ChmL3KanjiSelect::BuildCandidatePage(ChmCandidatePage& page, const ChmL3Helper& helper) const
{
	page = ChmCandidatePage{};
	page.page = _page;
	page.totalCount = _count;

	if (!_list || _count == 0 || _pageSize == 0) return FALSE;
	page.totalPages = (_count + _pageSize - 1) / _pageSize;

	size_t start = static_cast<size_t>(_page) * _pageSize;
	if (start >= _count) return FALSE;

	size_t available = _count - start;
	size_t pageCount = std::min<size_t>(_pageSize, available);
	page.candidateCount = static_cast<UINT>(pageCount);

	for (size_t i = 0; i < pageCount; ++i) {
		int cell = helper.IndexToCell(i);
		if (cell < 0 || cell >= static_cast<int>(CHM_CANDIDATE_MAX)) continue;
		page.candidates[cell] = _list[start + i];
	}

	return TRUE;
}
