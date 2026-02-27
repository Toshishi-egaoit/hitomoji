// ChmConfig.h
#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <windows.h>

class ChmConfig
{
public:
    // セクションやキーが見つからない／型不一致の場合の既定値
    static constexpr BOOL GetBool_default = TRUE;  // FALSE だとネガティブ値がデフォルトとなるため
    static constexpr LONG GetLong_default = 0;

    struct ParseError {
		std::wstring fileName;
        size_t lineNo;
        std::wstring message;
    };

	enum class ParseLevel {
		None,
		Info,
		Error
	};

	struct ParseResult {
		ParseLevel level = ParseLevel::None;
		std::wstring message;
	};

    // 内部表現は long / string に正規化
    using ConfigValue = std::variant<bool, long, std::wstring>;
    using SectionMap  = std::unordered_map<std::wstring, ConfigValue>;
    using ConfigMap   = std::unordered_map<std::wstring, SectionMap>;
    using ErrorMap    = std::vector<ParseError>;
    using InfoMap     = std::vector<ParseError>;

    ChmConfig();
    ~ChmConfig() = default;

    // ini 読み込み
    BOOL LoadFile(const std::wstring& fileName=L"");
    void InitConfig();

    // Getter（見つからない／型不一致は既定値）
    BOOL GetBool(const std::wstring& section, const std::wstring& key, const BOOL bDefault=GetBool_default) const;
    LONG GetLong(const std::wstring& section, const std::wstring& key, const LONG lDefault=GetLong_default) const;
    std::wstring GetString(const std::wstring& section, const std::wstring& key) const;
    std::wstring Dump() const;
    std::wstring DumpErrors() const;
    std::wstring DumpInfos() const;
    BOOL HasErrors() const { return !m_errors.empty(); }
    BOOL HasInfos() const { return !m_infos.empty(); }

    // 公開ヘルパー
    static std::wstring Trim(const std::wstring& s);
    static std::wstring Canonize(const std::wstring& s);
	static void SetError(ParseResult& r, const std::wstring& msg)
	{
		r.level = ParseLevel::Error;
		r.message = msg;
	}
	static void SetInfo(ParseResult& r, const std::wstring& msg)
	{
		r.level = ParseLevel::Info;
		r.message = msg;
	}
    
private:
	BOOL _LoadStreamInternal(std::wistream& is,
                             const std::wstring& fileName,
                             BOOL isMain,
                             std::wstring currentSection);
	BOOL _parseSection(const std::wstring& rawTrim,
                       std::wstring& currentSection,
					   ParseResult& errorMsg);
	BOOL _divideRawTrim(const std::wstring& rawTrim,
					std::wstring& key,
					std::wstring& value,
	 			    ParseResult& errorMsg);
	BOOL _parseValue(const std::wstring& keyTrim,
                    const std::wstring& valueTrim,
                    const std::wstring& currentSection,
                    ParseResult& errorMsg);

    // --- helper ---
    std::wstring _Dump() const;
    std::wstring _DumpErrors() const;
    std::wstring _DumpInfos() const;
    static bool _isValidName(const std::wstring& name);
    static bool _tryParseLong(const std::wstring& s, long& outValue);
    static bool _tryParseBool(const std::wstring& s, bool& outValue);
	bool _isDuplicateKey(const std::wstring& section, const std::wstring & canonizedKey) ;
	bool _addErrorOrInfo(ParseResult& errorMsg, size_t lineNo);

	std::wstring m_currentFile;
    ConfigMap m_config;
    ErrorMap  m_errors;
    InfoMap  m_infos;
};
