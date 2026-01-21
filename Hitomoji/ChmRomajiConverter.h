// ChmRomajiConverter.h
#pragma once
#include <string>
#include <unordered_map>

// rawInput (ASCII) を走査して、ひらがなへ変換するクラス
// v0.1.1: 最長一致（3→2→1）のみ実装
class ChmRomajiConverter {
public:
    // コンストラクタ不要（static 初期化のみ）

    // rawInput 全体を入力として処理する
    // converted : かなに変換できた部分
    // pending   : 変換できずに残った部分（未確定ローマ字）
        // rawInput を唯一の入力として処理する
    // 内部で lower 化した文字列を用いてかな変換を行う
    // converted : かなに変換できた部分
    // pending   : rawInput を基準にした未確定部分（表示用）
    static void convert(const std::string& rawInput,
                 std::wstring& converted,
                 std::string& pending) ;

	static std::wstring HiraganaToKatakana(const std::wstring& hira);

private:
    static const std::unordered_map<std::string, std::wstring>& table();
};

// ---- implementation ----

// static 初期化用テーブル
inline const std::unordered_map<std::string, std::wstring>& ChmRomajiConverter::table() {
    static const std::unordered_map<std::string, std::wstring> tbl = {
        {"a", L"あ"}, {"i", L"い"}, {"u", L"う"}, {"e", L"え"}, {"o", L"お"},
        {"ka", L"か"}, {"ki", L"き"}, {"ku", L"く"}, {"ke", L"け"}, {"ko", L"こ"},
        {"na", L"な"}, {"ni", L"に"}, {"nu", L"ぬ"}, {"ne", L"ね"}, {"no", L"の"},
        {"ra", L"ら"}, {"ri", L"り"}, {"ru", L"る"}, {"re", L"れ"}, {"ro", L"ろ"},
        {"rya", L"りゃ"}, {"ryu", L"りゅ"}, {"ryo", L"りょ"},
        {"nn", L"ん"}, {"n", L"ん"},
        {"-", L"ー"}
    };
    return tbl;
}

void ChmRomajiConverter::convert(const std::string& rawInput,
                                          std::wstring& converted,
                                          std::string& pending) {
    converted.clear();
    pending.clear();

    // かな変換用に lower 化した入力を生成
    std::string lowerInput;
    lowerInput.reserve(rawInput.size());
    for (unsigned char c : rawInput) {
        lowerInput.push_back(static_cast<char>(std::tolower(c)));
    }

    size_t i = 0;
    while (i < lowerInput.size()) {
        bool matched = false;

        // 最長一致: 3 → 2 → 1
        for (int len = 3; len >= 1; --len) {
            if (i + len > lowerInput.size()) continue;

            auto key = lowerInput.substr(i, len);
            auto it = table().find(key);
            if (it != table().end()) {
                converted += it->second;
                i += len;
                matched = true;
                break;
            }
        }

        if (!matched) break; // ここから先は pending
    }

    // pending は rawInput を基準に切り出す（大小文字を保持）
    if (i < rawInput.size()) {
        pending = rawInput.substr(i);
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
