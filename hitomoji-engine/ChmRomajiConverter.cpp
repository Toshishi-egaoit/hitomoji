#include "ChmRomajiConverter.h"
#include <cwctype>

size_t ChmRomajiConverter::_lastRawUnitLength = 0;

// ---- static member ----

// Override table (Configから追加される)
std::unordered_map<std::wstring, std::wstring> _overrideTable;

// Base table (固定のconst)
static const std::unordered_map<std::wstring, std::wstring> _baseTable = {
        {L"a", L"あ"}, {L"i", L"い"}, {L"u", L"う"}, {L"e", L"え"}, {L"o", L"お"},
        {L"ka", L"か"}, {L"ki", L"き"}, {L"ku", L"く"}, {L"ke", L"け"}, {L"ko", L"こ"},
        {L"ga", L"が"}, {L"gi", L"ぎ"}, {L"gu", L"ぐ"}, {L"ge", L"げ"}, {L"go", L"ご"},
        {L"sa", L"さ"}, {L"si", L"し"}, {L"shi", L"し"}, {L"su", L"す"}, {L"se", L"せ"}, {L"so", L"そ"},
        {L"za", L"ざ"}, {L"zi", L"じ"}, {L"ji", L"じ"}, {L"zu", L"ず"}, {L"ze", L"ぜ"}, {L"zo", L"ぞ"},
        {L"ta", L"た"}, {L"ti", L"ち"}, {L"chi", L"ち"}, {L"tu", L"つ"}, {L"te", L"て"}, {L"to", L"と"},
        {L"da", L"だ"}, {L"di", L"ぢ"}, {L"du", L"づ"}, {L"de", L"で"}, {L"do", L"ど"},
        {L"na", L"な"}, {L"ni", L"に"}, {L"nu", L"ぬ"}, {L"ne", L"ね"}, {L"no", L"の"},
        {L"ha", L"は"}, {L"hi", L"ひ"}, {L"hu", L"ふ"}, {L"he", L"へ"}, {L"ho", L"ほ"},
        {L"ba", L"ば"}, {L"bi", L"び"}, {L"bu", L"ぶ"}, {L"be", L"べ"}, {L"bo", L"ぼ"},
        {L"pa", L"ぱ"}, {L"pi", L"ぴ"}, {L"pu", L"ぷ"}, {L"pe", L"ぺ"}, {L"po", L"ぽ"},
        {L"ma", L"ま"}, {L"mi", L"み"}, {L"mu", L"む"}, {L"me", L"め"}, {L"mo", L"も"},
        {L"ya", L"や"}, {L"yu", L"ゆ"}, {L"yo", L"よ"},
        {L"ra", L"ら"}, {L"ri", L"り"}, {L"ru", L"る"}, {L"re", L"れ"}, {L"ro", L"ろ"},
        {L"wa", L"わ"}, {L"wi", L"ゐ"}, {L"we", L"ゑ"}, {L"wo", L"を"},
        // --- 以下元テーブル続き（省略なし） ---
        // --- 拗音の清音（y系） ---
		{L"kya", L"きゃ"}, {L"kyu", L"きゅ"}, {L"kyo", L"きょ"}, 
		{L"sya", L"しゃ"}, {L"syu", L"しゅ"}, {L"sye", L"しぇ"}, {L"syo", L"しょ"}, 
		{L"sha", L"しゃ"}, {L"shu", L"しゅ"}, {L"sho", L"しょ"}, 
		{L"cha", L"ちゃ"}, {L"chu", L"ちゅ"}, {L"cho", L"ちょ"}, 
		{L"tya", L"ちゃ"}, {L"tyu", L"ちゅ"}, {L"tye", L"ちぇ"}, {L"tyo", L"ちょ"}, 
		{L"nya", L"にゃ"}, {L"nyu", L"にゅ"}, {L"nyo", L"にょ"},
		{L"hya", L"ひゃ"}, {L"hyu", L"ひゅ"}, {L"hyo", L"ひょ"},
		{L"mya", L"みゃ"}, {L"myu", L"みゅ"}, {L"myo", L"みょ"},
        {L"rya", L"りゃ"}, {L"ryu", L"りゅ"}, {L"ryo", L"りょ"},
        // --- 拗音の濁音 (gy系) ---
        {L"gya", L"ぎゃ"}, {L"gyu", L"ぎゅ"}, {L"gyo", L"ぎょ"},
        // --- 拗音の濁音 (zy,j系) ---
		{L"zya", L"じゃ"}, {L"zyu", L"じゅ"}, {L"zye", L"じぇ"}, {L"zyo", L"じょ"},
        {L"ja", L"じゃ"},  {L"ju", L"じゅ"},  {L"je", L"じぇ"},  {L"jo", L"じょ"},
        // --- 拗音の濁音 (dy系) ---
		{L"dya", L"ぢゃ"}, {L"dyu", L"ぢゅ"}, {L"dyo", L"ぢょ"}, 
        // --- 拗音の濁音 (by,py系) ---
		{L"bya", L"びゃ"}, {L"byu", L"びゅ"}, {L"byo", L"びょ"},
		{L"pya", L"ぴゃ"}, {L"pyu", L"ぴゅ"}, {L"pyo", L"ぴょ"},
		
        {L"nn", L"ん"}, {L"n", L"ん"}, {L"n'", L"ん"},
        // --- 小さい文字 (IME慣習) ---
        {L"ltu", L"っ"}, {L"xtu", L"っ"},
        {L"lya", L"ゃ"}, {L"lyu", L"ゅ"}, {L"lyo", L"ょ"},
        {L"xya", L"ゃ"}, {L"xyu", L"ゅ"}, {L"xyo", L"ょ"},
        {L"la", L"ぁ"}, {L"li", L"ぃ"}, {L"lu", L"ぅ"}, {L"le", L"ぇ"}, {L"lo", L"ぉ"},
        {L"xa", L"ぁ"}, {L"xi", L"ぃ"}, {L"xu", L"ぅ"}, {L"xe", L"ぇ"}, {L"xo", L"ぉ"},
        {L"lwa", L"ゎ"}, {L"lka", L"ゕ"}, {L"lke", L"ゖ"},
        {L"xwa", L"ゎ"}, {L"xka", L"ゕ"}, {L"xke", L"ゖ"},
        // --- 外来音・拡張表記 ---
        {L"khu", L"くゅ"}, {L"qe", L"くぇ"}, {L"qo",L"くぉ"} ,
		{L"she", L"しぇ"},
		{L"che", L"ちぇ"},
		{L"va", L"ゔぁ"}, {L"vi", L"ゔぃ"}, {L"vu", L"ゔ"}, {L"ve", L"ゔぇ"}, {L"vo", L"ゔぉ"},
        {L"fa", L"ふぁ"}, {L"fi", L"ふぃ"}, {L"fu", L"ふ"}, {L"fe", L"ふぇ"}, {L"fo", L"ふぉ"},
        {L"thi", L"てぃ"}, {L"thu", L"とぅ"}, 
		{L"dhi", L"でぃ"}, {L"dhu", L"でゅ"},
        {L"whi", L"うぃ"}, {L"whe", L"うぇ"}, {L"who", L"うぉ"},
        // --- 役物、記号類
        {L".", L"。"}, {L",", L"、"}, {L"?", L"？"}, {L"!", L"！"},
        {L"[", L"「"}, {L"]", L"」"}, 
        {L"-", L"ー"},
};

const std::unordered_set<wchar_t>& ChmRomajiConverter::sokuonConsonants() {
    static const std::unordered_set<wchar_t> set = {
        L'b',L'c',L'd',L'f',L'g',L'h',L'j',L'k',L'm',L'p',L'q',L'r',L's',L't',L'v',L'w',L'y',L'z'
    };
    return set;
}

// Find: override優先 → base
static const std::wstring* FindEntry(const std::wstring& key)
{
    auto itO = _overrideTable.find(key);
    if (itO != _overrideTable.end()) return &itO->second;

    auto itB = _baseTable.find(key);
    if (itB != _baseTable.end()) return &itB->second;

    return nullptr;
}

bool ChmRomajiConverter::TryConvertOne(const std::wstring& rawInput,
                                       size_t pos,
                                       Unit& out,
                                       bool isDispSymbol)
{
    out.output.clear();
    out.rawLength = 0;
    if (pos >= rawInput.size()) return false;

    std::wstring lowerInput;
    lowerInput.reserve(rawInput.size());
    for (wchar_t c : rawInput)
        lowerInput.push_back(static_cast<wchar_t>(std::towlower(c)));

    for (int len = 3; len >= 1; --len) {
        if (pos + len > lowerInput.size()) continue;
        auto key = lowerInput.substr(pos, len);
        auto val = FindEntry(key);
        if (val) {
            if (isDispSymbol) out.output = *val;
            else out.output = rawInput.substr(pos, len);
            out.rawLength = len;
            return true;
        }
    }

    wchar_t cur = lowerInput[pos];
    if (!std::iswalpha(cur)) {
        out.output.push_back(rawInput[pos]);
        out.rawLength = 1;
        return true;
    }

    if (pos + 1 < lowerInput.size()) {
        wchar_t c1 = lowerInput[pos];
        wchar_t c2 = lowerInput[pos + 1];
        if (c1 == c2 && sokuonConsonants().count(c1)) {
            out.output = L"っ";
            out.rawLength = 1;
            return true;
        }
    }
    return false;
}

std::wstring ChmRomajiConverter::HiraganaToKatakana(const std::wstring& hira)
{
    std::wstring result;
    for (wchar_t ch : hira) {
        if (ch >= 0x3041 && ch <= 0x3096) result.push_back(ch + 0x60);
        else result.push_back(ch);
    }
    return result;
}

size_t ChmRomajiConverter::GetLastRawUnitLength() {
    return _lastRawUnitLength;
}

void ChmRomajiConverter::convert(const std::wstring& rawInput,
                                 std::wstring& converted,
                                 std::wstring& pending,
                                 bool isDisplayKana,
                                 bool isBackspaceSymbol)
{
    converted.clear();
    pending.clear();
    _lastRawUnitLength = 0;

    size_t pos = 0;
    Unit unit;

    while (pos < rawInput.size()) {
        if (TryConvertOne(rawInput, pos, unit, isDisplayKana)) {
            converted += pending + unit.output;
			pending = L"";
            _lastRawUnitLength = unit.rawLength;
            pos += unit.rawLength;
        }
        else {
            pending += rawInput[pos];
            _lastRawUnitLength = pending.length();
            pos++;
        }
    }

    if (!isBackspaceSymbol) {
        _lastRawUnitLength = 1;
    }
}

// v0.2.2
// ChmConfig::_parseLine と互換のあるインタフェースにする
// ＝ 解析だけを行い、登録は呼び出し側に任せる

// 成功: true
// 失敗: false（error に理由を入れる）

bool ChmKeytableParser::ParseLine(const std::wstring& line,
                                  std::wstring& left,
                                  std::wstring& right,
                                  std::wstring& error)
{
    left.clear();
    right.clear();
    error.clear();

        // 前後Trim
    std::wstring trimmed = Trim(line);

    // 空行はTrue
    if (trimmed.empty()) {
        return true;
    }

    // コメント行は無視（; または #）
    if (trimmed[0] == L';' || trimmed[0] == L'#') {
        return true;
    }

    // '=' の位置（右から検索）
    size_t pos = line.rfind(L'=');
    if (pos == std::wstring::npos) {
        error = L"no '=' found";
        return false;
    }

    left  = Trim(line.substr(0, pos));
    right = Trim(line.substr(pos + 1));

    if (left.empty()) {
        error = L"left side empty";
        return false;
    }

    // 予約行（将来用）はTrueをかえす
    if (left == L"algorithm") {
      left.clear();
      right.clear();
      return true;
    }
  
    if (right.empty()) {
        error = L"right side empty";
        return false;
    }

    return true;
}

std::wstring ChmKeytableParser::Trim(const std::wstring& s)
{
    size_t start = 0;
    while (start < s.size() && iswspace(s[start])) start++;

    size_t end = s.size();
    while (end > start && iswspace(s[end - 1])) end--;

    return s.substr(start, end - start);
}

