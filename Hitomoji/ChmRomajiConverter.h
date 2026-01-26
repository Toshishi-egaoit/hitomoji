// ChmRomajiConverter.h
#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>

// rawInput (ASCII) を走査して、ひらがなへ変換するクラス
// v0.1.1: 最長一致（3→2→1）のみ実装
class ChmRomajiConverter {
public:
    // コンストラクタ不要（static 初期化のみ）

    struct Unit {
        std::wstring output;   // 確定した1ユニットの表示文字
        size_t rawLength = 0;  // 消費した raw 文字数
    };

    // rawInput 全体を入力として処理する
    // converted : かなに変換できた部分
	// pending   : 変換できずに残った部分（未確定ローマ字）
    static void convert(const std::string& rawInput,
                        std::wstring& converted,
                        std::string& pending);

    // 1ユニットだけ変換できるか試みる（v0.1.5 用）
    static bool TryConvertOne(const std::string& rawInput,
                              size_t pos,
                              Unit& out);

    static size_t GetLastRawUnitLength() {
        return _lastRawUnitLength;
    }


    static std::wstring HiraganaToKatakana(const std::wstring& hira);

private:
    // v0.1.5 用：TryConvertOne ベースの拡張変換
    // lastRawUnitLength は内部状態として保持する
    static size_t _lastRawUnitLength;
    static const std::unordered_map<std::string, std::wstring>& table();
    static const std::unordered_set<char>& sokuonConsonants();
};

// ---- implementation ----

// static 初期化用テーブル
inline const std::unordered_map<std::string, std::wstring>& ChmRomajiConverter::table() {
    static const std::unordered_map<std::string, std::wstring> tbl = {
        // --- 母音 ---
        {"a", L"あ"}, {"i", L"い"}, {"u", L"う"}, {"e", L"え"}, {"o", L"お"},

        // ---か(k)、が(g) ---
        {"ka", L"か"}, {"ki", L"き"}, {"ku", L"く"}, {"ke", L"け"}, {"ko", L"こ"},
        {"ga", L"が"}, {"gi", L"ぎ"}, {"gu", L"ぐ"}, {"ge", L"げ"}, {"go", L"ご"},

        // ---さ(s)、ざ(z) ---
        {"sa", L"さ"}, {"si", L"し"}, {"shi", L"し"}, {"su", L"す"}, {"se", L"せ"}, {"so", L"そ"},
        {"za", L"ざ"}, {"zi", L"じ"}, {"ji" , L"じ"}, {"zu", L"ず"}, {"ze", L"ぜ"}, {"zo", L"ぞ"},

        // ---た(t)、だ(d) ---
        {"ta", L"た"}, {"ti", L"ち"}, {"chi", L"ち"}, {"tu", L"つ"}, {"te", L"て"}, {"to", L"と"},
        {"da", L"だ"}, {"di", L"ぢ"},                 {"du", L"づ"}, {"de", L"で"}, {"do", L"ど"},

        // ---な(n) ---
        {"na", L"な"}, {"ni", L"に"}, {"nu", L"ぬ"}, {"ne", L"ね"}, {"no", L"の"},

        // --- は(h)、ば(b)、ぱ(p) ---
        {"ha", L"は"}, {"hi", L"ひ"}, {"hu", L"ふ"}, {"he", L"へ"}, {"ho", L"ほ"},
        {"ba", L"ば"}, {"bi", L"び"}, {"bu", L"ぶ"}, {"be", L"べ"}, {"bo", L"ぼ"},
        {"pa", L"ぱ"}, {"pi", L"ぴ"}, {"pu", L"ぷ"}, {"pe", L"ぺ"}, {"po", L"ぽ"},

        // ---ま(m) ---
        {"ma", L"ま"}, {"mi", L"み"}, {"mu", L"む"}, {"me", L"め"}, {"mo", L"も"},

        // ---や(y) ---
        {"ya", L"や"}, {"yu", L"ゆ"}, {"yo", L"よ"},

        // ---ら(r) ---
        {"ra", L"ら"}, {"ri", L"り"}, {"ru", L"る"}, {"re", L"れ"}, {"ro", L"ろ"},

        // --- わ(w)行（本仮名遣い対応） ---
        {"wa", L"わ"}, {"wi", L"ゐ"}, {"we", L"ゑ"}, {"wo", L"を"},

        // --- 拗音の清音（y系） ---
		{"kya", L"きゃ"}, {"kyu", L"きゅ"}, {"kyo", L"きょ"}, 
		{"sya", L"しゃ"}, {"syu", L"しゅ"}, {"sye", L"しぇ"}, {"syo", L"しょ"}, 
		{"sha", L"しゃ"}, {"shu", L"しゅ"}, {"sho", L"しょ"}, 
		{"cha", L"ちゃ"}, {"chu", L"ちゅ"}, {"cho", L"ちょ"}, 
		{"tya", L"ちゃ"}, {"tyu", L"ちゅ"}, {"tye", L"ちぇ"}, {"tyo", L"ちょ"}, 
		{"nya", L"にゃ"}, {"nyu", L"にゅ"}, {"nyo", L"にょ"},
		{"hya", L"ひゃ"}, {"hyu", L"ひゅ"}, {"hyo", L"ひょ"},
		{"mya", L"みゃ"}, {"myu", L"みゅ"}, {"myo", L"みょ"},
        {"rya", L"りゃ"}, {"ryu", L"りゅ"}, {"ryo", L"りょ"},
        // --- 拗音の濁音 (gy系) ---
        {"gya", L"ぎゃ"}, {"gyu", L"ぎゅ"}, {"gyo", L"ぎょ"},
        // --- 拗音の濁音 (zy,j系) ---
		{"zya", L"じゃ"}, {"zyu", L"じゅ"}, {"zye", L"じぇ"}, {"zyo", L"じょ"},
        {"ja", L"じゃ"},  {"ju", L"じゅ"},  {"je", L"じぇ"},  {"jo", L"じょ"},
        // --- 拗音の濁音 (dy系) ---
		{"dya", L"ぢゃ"}, {"dyu", L"ぢゅ"}, {"dyo", L"ぢょ"}, 
        // --- 拗音の濁音 (by,py系) ---
		{"bya", L"びゃ"}, {"byu", L"びゅ"}, {"byo", L"びょ"},
		{"pya", L"ぴゃ"}, {"pyu", L"ぴゅ"}, {"pyo", L"ぴょ"},
		
        {"nn", L"ん"}, {"n", L"ん"}, {"n'", L"ん"},
        // --- 小さい文字 (IME慣習) ---
        {"ltu", L"っ"}, {"xtu", L"っ"},
        {"lya", L"ゃ"}, {"lyu", L"ゅ"}, {"lyo", L"ょ"},
        {"xya", L"ゃ"}, {"xyu", L"ゅ"}, {"xyo", L"ょ"},
        {"la", L"ぁ"}, {"li", L"ぃ"}, {"lu", L"ぅ"}, {"le", L"ぇ"}, {"lo", L"ぉ"},
        {"xa", L"ぁ"}, {"xi", L"ぃ"}, {"xu", L"ぅ"}, {"xe", L"ぇ"}, {"xo", L"ぉ"},
        {"lwa", L"ゎ"}, {"lka", L"ゕ"}, {"lke", L"ゖ"},
        {"xwa", L"ゎ"}, {"xka", L"ゕ"}, {"xke", L"ゖ"},
        // --- 外来音・拡張表記 ---
        {"khu", L"くゅ"}, {"qe", L"くぇ"}, {"qo",L"くぉ"} ,
		{"she", L"しぇ"},
		{"che", L"ちぇ"},
		{"va", L"ゔぁ"}, {"vi", L"ゔぃ"}, {"vu", L"ゔ"}, {"ve", L"ゔぇ"}, {"vo", L"ゔぉ"},
        {"fa", L"ふぁ"}, {"fi", L"ふぃ"}, {"fu", L"ふ"}, {"fe", L"ふぇ"}, {"fo", L"ふぉ"},
        {"thi", L"てぃ"}, {"thu", L"とぅ"}, 
		{"dhi", L"でぃ"}, {"dhu", L"でゅ"},
        {"whi", L"うぃ"}, {"whe", L"うぇ"}, {"who", L"うぉ"},
        // --- 役物、記号類
        {".", L"。"}, {",", L"、"}, {"?", L"？"}, {"!", L"！"},
        {"[", L"「"}, {"]", L"」"},
        {"-", L"ー"},
    };
    return tbl;
}

// 促音判定用 子音セット（x, l, n 除外）
inline const std::unordered_set<char>& ChmRomajiConverter::sokuonConsonants() {
    static const std::unordered_set<char> set = {
        'b','c','d','f','g','h','j','k','m','p','q','r','s','t','v','w','y','z'
    };
    return set;
}

bool ChmRomajiConverter::TryConvertOne(const std::string& rawInput,
                                        size_t pos,
                                        Unit& out)
{
    out.output.clear();
    out.rawLength = 0;

    if (pos >= rawInput.size()) return false;

    // lower 化（convert と同じ方針）
    std::string lowerInput;
    lowerInput.reserve(rawInput.size());
    for (unsigned char c : rawInput) {
        lowerInput.push_back(static_cast<char>(std::tolower(c)));
    }

        // --- 最長一致（3→2→1） ---
    for (int len = 3; len >= 1; --len) {
        if (pos + len > lowerInput.size()) continue;
        auto key = lowerInput.substr(pos, len);
        auto it = table().find(key);
        if (it != table().end()) {
            out.output = it->second;
            out.rawLength = len;
            return true;
        }
    }

    // --- 記号・非アルファベットは即確定 ---
    char cur = lowerInput[pos];
    if (!std::isalpha(static_cast<unsigned char>(cur))) {
        out.output.push_back(static_cast<wchar_t>(rawInput[pos]));
        out.rawLength = 1;
        return true;
    }

    // --- 促音判定（tt など） ---
    if (pos + 1 < lowerInput.size()) {
        char c1 = lowerInput[pos];
        char c2 = lowerInput[pos + 1];
        if (c1 == c2 && sokuonConsonants().count(c1)) {
            out.output = L"っ";
            out.rawLength = 1; // 子音1文字ぶん消費
            return true;
        }
    }

    // ここまで来たら未確定（pending）
    return false;
}

void ChmRomajiConverter::convert(const std::string& rawInput,
                                 std::wstring& converted,
                                 std::string& pending)
{
    converted.clear();
    pending.clear();
    _lastRawUnitLength = 0;

    size_t pos = 0;
    Unit unit;

    while (pos < rawInput.size()) {
        if (TryConvertOne(rawInput, pos, unit)) {
            converted += unit.output;
            _lastRawUnitLength = unit.rawLength;
            pos += unit.rawLength;
        } else {
            // 未確定部分は pending に回す
            pending = rawInput.substr(pos);
            if (pending.length() != 0) _lastRawUnitLength = pending.length();  
            break;
        }
    }
}

std::wstring ChmRomajiConverter::HiraganaToKatakana(const std::wstring& hira)
{
    std::wstring result;
    for (wchar_t ch : hira) {
        // Unicode: ひらがな U+3041–3096
        //           カタカナ U+30A1–30F6
        if (ch >= 0x3041 && ch <= 0x3096) {
            result.push_back(ch + 0x60);
        } else {
            result.push_back(ch);
        }
    }
    return result;
}

// --- v0.1.5: convert を TryConvertOne ベースで再実装 ---
size_t ChmRomajiConverter::_lastRawUnitLength = 0;
