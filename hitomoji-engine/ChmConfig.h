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

    // 内部表現は long / string に正規化
    using ConfigValue = std::variant<bool, long, std::wstring>;
    using SectionMap  = std::unordered_map<std::wstring, ConfigValue>;
    using ConfigMap   = std::unordered_map<std::wstring, SectionMap>;
    using ErrorMap    = std::vector<ParseError>;

public:
    ChmConfig();
    ~ChmConfig() = default;

    // ini 読み込み
    BOOL LoadFromStream(std::wistream& is);
    BOOL LoadFile(const std::wstring& fileName=L"");
    void InitConfig();

    // Getter（見つからない／型不一致は既定値）
    BOOL GetBool(const std::wstring& section, const std::wstring& key, const BOOL bDefault=GetBool_default) const;
    LONG GetLong(const std::wstring& section, const std::wstring& key, const LONG lDefault=GetLong_default) const;
    std::wstring GetString(const std::wstring& section, const std::wstring& key) const;
    std::wstring Dump() const;
    std::wstring DumpErrors() const;
    BOOL HasErrors() const { return !m_errors.empty(); }

    // 公開ヘルパー
    static std::wstring Trim(const std::wstring& s);
    static std::wstring Canonize(const std::wstring& s);
    
private:
	BOOL _LoadStreamInternal(std::wistream& is, 
							 const std::wstring& fileName,
						 	 BOOL isMain);
	BOOL _parseSection(const std::wstring& rawTrim,
                       std::wstring& currentSection,
                       std::wstring& errorMsg);
	BOOL _divideRawTrim(const std::wstring& rawTrim,
					std::wstring& key,
					std::wstring& value,
					std::wstring& errorMsg);
	BOOL _parseValue(const std::wstring& keyTrim,
                    const std::wstring& valueTrim,
                    const std::wstring& currentSection,
                    std::wstring& errorMsg);

    // --- helper ---
    std::wstring _Dump() const;
    std::wstring _DumpErrors() const;
    static bool _isValidName(const std::wstring& name);
    static bool _tryParseLong(const std::wstring& s, long& outValue);
    static bool _tryParseBool(const std::wstring& s, bool& outValue);
	bool _isDuplicateKey(const std::wstring& section, const std::wstring & canonizedKey) ;

	std::wstring m_currentFile;
    ConfigMap m_config;
    ErrorMap  m_errors;
};
