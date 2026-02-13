// ChmConfig.h
#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <windows.h>

class ChmConfig
{
public:
	struct ParseError {
        size_t lineNo;
        std::wstring message;
    };

    // 内部表現は long / string に正規化
    using ConfigValue = std::variant<long, std::wstring>;
    using SectionMap  = std::unordered_map<std::wstring, ConfigValue>;
    using ConfigMap   = std::unordered_map<std::wstring, SectionMap>;
	using ErrorMap    = std::vector<ParseError>;

public:
    ChmConfig();
    ~ChmConfig() = default;

    // ini 読み込み
    BOOL LoadFile(const std::wstring& fileName=L"");
    void InitConfig();

    // Getter（見つからない／型不一致は既定値）
    BOOL GetBool(const std::wstring& section, const std::wstring& key) const;
    LONG GetLong(const std::wstring& section, const std::wstring& key) const;
    std::wstring GetString(const std::wstring& section, const std::wstring& key) const;
	std::wstring DumpErrors() const;
	BOOL HasErrors() const { return ! m_errors.empty() ; }

	// TODO: GetUnusedKeys() の実装

private:
    BOOL _parseLine(const std::wstring& rawLine,
                    std::wstring& currentSection,
                    std::wstring& errorMsg);

    // --- helper ---
    static std::wstring _normalize(const std::wstring& s);
	std::wstring _Dump() const;
	std::wstring _DumpErrors() const;
    static bool _isValidName(const std::wstring& name);
    static bool _tryParseLong(const std::wstring& s, long& outValue);
    static bool _tryParseBool(const std::wstring& s, long& outValue);

    ConfigMap m_config;
    ErrorMap  m_errors;
};

// グローバル（差し替え前提）
extern ChmConfig* g_config;

