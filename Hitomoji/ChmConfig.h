// ChmConfig.h
#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <windows.h>

class ChmConfig
{
public:
    // 内部表現は long / string に正規化
    using ConfigValue = std::variant<long, std::wstring>;
    using SectionMap  = std::unordered_map<std::wstring, ConfigValue>;
    using ConfigMap   = std::unordered_map<std::wstring, SectionMap>;

    struct ParseError
    {
        size_t lineNo;
        std::wstring message;
    };

public:
    ChmConfig(const std::wstring& fileName = L"");
    ~ChmConfig() = default;

    // ini 読み込み
    BOOL LoadFile(const std::wstring& fileName);
    void InitConfig();

    // Getter（見つからない／型不一致は既定値）
    BOOL         GetBool(const std::wstring& section, const std::wstring& key) const;
    ULONG        GetLong(const std::wstring& section, const std::wstring& key) const;
    std::wstring GetString(const std::wstring& section, const std::wstring& key) const;

    // debug dump
    std::wstring Dump() const;
    std::wstring DumpErrors() const;

private:
    BOOL _parseLine(const std::wstring& rawLine,
                    std::wstring& currentSection,
                    std::wstring& errorMsg);

    std::wstring _Dump() const;
    std::wstring _DumpErrors() const;

private:
    ConfigMap m_config;
    std::vector<ParseError> m_errors;
};

// グローバル（差し替え前提）
extern ChmConfig* g_config;

