// ChmRomajiConverter.h
#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "ChmConfig.h"

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
    static void convert(const std::wstring& rawInput,
                        std::wstring& converted,
                        std::wstring& pending,
						bool isSybolMode,
					    bool isBsSymbol);

    // 1ユニットだけ変換できるか試みる（v0.1.5 用）
    static bool TryConvertOne(const std::wstring& rawInput,
                              size_t pos,
							  Unit& out,
							  bool isSymbolMode);

	static size_t GetLastRawUnitLength() {
		return _lastRawUnitLength;
	}

    static std::wstring HiraganaToKatakana(const std::wstring& hira);

private:
    // v0.1.5 用：TryConvertOne ベースの拡張変換
    // lastRawUnitLength は内部状態として保持する
    static size_t _lastRawUnitLength;
    static const std::unordered_map<std::wstring, std::wstring>& table();
    static const std::unordered_set<wchar_t>& sokuonConsonants();
};

// ---- implementation ----

// static 初期化用テーブル
inline const std::unordered_map<std::wstring, std::wstring>& ChmRomajiConverter::table() {
    static const std::unordered_map<std::wstring, std::wstring> tbl = {
        // --- 母音 ---
        {L"a", L"あ"}, {L"i", L"い"}, {L"u", L"う"}, {L"e", L"え"}, {L"o", L"お"},

        // ---か(k)、が(g) ---
        {L"ka", L"か"}, {L"ki", L"き"}, {L"ku", L"く"}, {L"ke", L"け"}, {L"ko", L"こ"},
        {L"ga", L"が"}, {L"gi", L"ぎ"}, {L"gu", L"ぐ"}, {L"ge", L"げ"}, {L"go", L"ご"},

        // ---さ(s)、ざ(z) ---
        {L"sa", L"さ"}, {L"si", L"し"}, {L"shi", L"し"}, {L"su", L"す"}, {L"se", L"せ"}, {L"so", L"そ"},
        {L"za", L"ざ"}, {L"zi", L"じ"}, {L"ji" , L"じ"}, {L"zu", L"ず"}, {L"ze", L"ぜ"}, {L"zo", L"ぞ"},

        // ---た(t)、だ(d) ---
        {L"ta", L"た"}, {L"ti", L"ち"}, {L"chi", L"ち"}, {L"tu", L"つ"}, {L"te", L"て"}, {L"to", L"と"},
        {L"da", L"だ"}, {L"di", L"ぢ"},                 {L"du", L"づ"}, {L"de", L"で"}, {L"do", L"ど"},

        // ---な(n) ---
        {L"na", L"な"}, {L"ni", L"に"}, {L"nu", L"ぬ"}, {L"ne", L"ね"}, {L"no", L"の"},

        // --- は(h)、ば(b)、ぱ(p) ---
        {L"ha", L"は"}, {L"hi", L"ひ"}, {L"hu", L"ふ"}, {L"he", L"へ"}, {L"ho", L"ほ"},
        {L"ba", L"ば"}, {L"bi", L"び"}, {L"bu", L"ぶ"}, {L"be", L"べ"}, {L"bo", L"ぼ"},
        {L"pa", L"ぱ"}, {L"pi", L"ぴ"}, {L"pu", L"ぷ"}, {L"pe", L"ぺ"}, {L"po", L"ぽ"},

        // ---ま(m) ---
        {L"ma", L"ま"}, {L"mi", L"み"}, {L"mu", L"む"}, {L"me", L"め"}, {L"mo", L"も"},

        // ---や(y) ---
        {L"ya", L"や"}, {L"yu", L"ゆ"}, {L"yo", L"よ"},

        // ---ら(r) ---
        {L"ra", L"ら"}, {L"ri", L"り"}, {L"ru", L"る"}, {L"re", L"れ"}, {L"ro", L"ろ"},

        // --- わ(w)行（本仮名遣い対応） ---
        {L"wa", L"わ"}, {L"wi", L"ゐ"}, {L"we", L"ゑ"}, {L"wo", L"を"},

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
    return tbl;
}

// 促音判定用 子音セット（x, l, n 除外）
inline const std::unordered_set<wchar_t>& ChmRomajiConverter::sokuonConsonants() {
    static const std::unordered_set<wchar_t> set = {
        L'b',L'c',L'd',L'f',L'g',L'h',L'j',L'k',L'm',L'p',L'q',L'r',L's',L't',L'v',L'w',L'y',L'z'
    };
    return set;
}

bool ChmRomajiConverter::TryConvertOne(const std::wstring& rawInput,
                                        size_t pos,
                                        Unit& out,
										bool isDispSymbol)
{
	// TODO: この かんすうをユニットのもじすうをもとめるきのうとレンダリングきのうにわける。
	// それによって、DisplayModeをConvertに とじこめる
    out.output.clear();
    out.rawLength = 0;

    if (pos >= rawInput.size()) return false;

    // lower 化（convert と同じ方針）
    std::wstring lowerInput;
    lowerInput.reserve(rawInput.size());
    for (wchar_t c : rawInput) {
        lowerInput.push_back(static_cast<char>(std::tolower(c)));
    }

        // --- 最長一致（3→2→1） ---
    for (int len = 3; len >= 1; --len) {
        if (pos + len > lowerInput.size()) continue;
        auto key = lowerInput.substr(pos, len);
        auto it = table().find(key);
        if (it != table().end()) {
			if (isDispSymbol) {
				out.output = it->second;
			} else {
				std::wstring sub = rawInput.substr(pos,len);
				out.output = std::wstring(sub.begin(), sub.end());
			}
            out.rawLength = len;
            return true;
        }
    }

    // --- 記号・非アルファベットは即確定 ---
    wchar_t cur = lowerInput[pos];
    if (!std::isalpha(static_cast<unsigned char>(cur))) {
        out.output.push_back(static_cast<wchar_t>(rawInput[pos]));
        out.rawLength = 1;
        return true;
    }

    // --- 促音判定（tt など） ---
    if (pos + 1 < lowerInput.size()) {
        wchar_t c1 = lowerInput[pos];
        wchar_t c2 = lowerInput[pos + 1];
        if (c1 == c2 && sokuonConsonants().count(c1)) {
            out.output = L"っ";
            out.rawLength = 1; // 子音1文字ぶん消費
            return true;
        }
    }

    // ここまで来たら未確定（pending）
    return false;
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

void ChmRomajiConverter::convert(const std::wstring& rawInput,
                                 std::wstring& converted,
                                 std::wstring& pending,
								 bool isDispSymbol,
								 bool isBsSymbol)
{
    converted.clear();
    pending.clear();
    _lastRawUnitLength = 0;

    size_t pos = 0;
    Unit unit;

	while (pos < rawInput.size()) {
		if (TryConvertOne(rawInput, pos, unit,isDispSymbol)) {
			converted += unit.output;
			_lastRawUnitLength = unit.rawLength;
			pos += unit.rawLength;
		}
		else if (pos + 2 >= rawInput.size()) { // 残り2文字以下なら pending に回す
			// TODO:この かんがえかたは ローマじかなへんかんでのみりようかのう。ConvertMgrではつかえない。
			pending = rawInput.substr(pos);
			_lastRawUnitLength = pending.length();
			// ループ終了
			break;
		}
		else { // 未確定部分がまだある場合、1文字進めて再試行}
			converted += rawInput[pos]; // 未確定文字をそのまま出力に追加
			_lastRawUnitLength = 1;
			pos ++;
		}
	}
	// ただし、bsUnitがASCIIの場合は、最後のユニット長を1にする
	if (!isBsSymbol) {
		_lastRawUnitLength = 1;
	}
}
