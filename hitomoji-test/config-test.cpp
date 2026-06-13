#include "gtest/gtest.h"
#include "ChmConfig.h"
#include "ChmRomajiConverter.h"
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

namespace {

static std::wstring ConvertWithPendingForConfigTest(const std::wstring& input,
                                                    bool bs = true)
{
    std::wstring converted;
    std::wstring pending;

    ChmRomajiConverter::convert(input, converted, pending, bs);
    return converted + L":" + pending;
}

class TempConfigDir {
public:
    TempConfigDir()
    {
        wchar_t tempPath[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath);

        wchar_t uniqueName[MAX_PATH];
        GetTempFileNameW(tempPath, L"hmj", 0, uniqueName);
        DeleteFileW(uniqueName);

        _baseDir = uniqueName;
        _baseDir += L"\\";
        CreateDirectoryW(_baseDir.c_str(), nullptr);
    }

    ~TempConfigDir()
    {
        for (const auto& file : _files) {
            DeleteFileW(file.c_str());
        }
        RemoveDirectoryW(_baseDir.c_str());
    }

    const std::wstring& BaseDir() const
    {
        return _baseDir;
    }

    void WriteFile(const std::wstring& fileName, const std::wstring& content)
    {
        std::wstring path = _baseDir + fileName;
        int size = WideCharToMultiByte(CP_UTF8, 0,
            content.c_str(), static_cast<int>(content.size()),
            nullptr, 0, nullptr, nullptr);
        std::string utf8(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0,
            content.c_str(), static_cast<int>(content.size()),
            utf8.data(), size, nullptr, nullptr);

        std::ofstream file(path, std::ios::binary);
        file.write(utf8.data(), utf8.size());
        file.close();
        _files.push_back(path);
    }

private:
    std::wstring _baseDir;
    std::vector<std::wstring> _files;
};

static std::wstring ValidConfigText()
{
    return
        L"# comment1\n"
        L"\n"
        L"[bOOLs]\n"
        L"bool-true-1=true\n"
        L"BOOL_TRUE_2 =True\n"
        L"bool-true-3= TRUE\n"
        L"Bool-true-4 = yes\n"
        L"  boOL-true-5=on\n"
        L"\tbool-true-6=1\n"
        L"bool_FALSE-1=false\n"
        L"bool-false-2=no\n"
        L"bool_false_3=off\n"
        L"bool-false_4=0\n"
        L"\n"
        L"[ numbers]\n"
        L"long-1=123\n"
        L"LOng_2 = -789\n"
        L"loNG-3=\t2100000000\n"
        L"\n"
        L"[strings ]\n"
        L"string-1=Hitomoji\n"
        L" string_2 = Hitomoji2\n"
        L"VALID-KEY-9=C:\\temp\\FILE.txt\n"
        L"\n"
        L"; comment2\n"
        L"[ canonize-1 ]\n"
        L"bool-true-1=true\n"
        L"bool_false_1=false\n"
        L"\n"
        L"; comment2\n";
}

static std::wstring FunctionKeyConfigText()
{
    return
        L"; FunctionKey test INI\n"
        L"\n"
        L"[Function-key]\n"
        L"CTRL+Z=cancel-finish\n"
        L"CTRL+Y = CANCEL\n"
        L"return = FINISH\n"
        L"SHIFT+ALT+F4=FINISH\n"
        L"ALT+CONTROL+ENTER=finish-katakana\n"
        L"CONTROL+ALT+ENTER=finish-katakana\n"
        L"CTRL+1 = finish-raw\n"
        L"ALT+A = finish-raw-wide\n"
        L"SHIFT+CTRL+0=finish-raw\n";
}

static std::wstring InvalidConfigText()
{
    return
        L"# invalid test file\n"
        L"\n"
        L"[valid_section]\n"
        L"key1=123\n"
        L"\n"
        L"[invalid section\n"
        L"missing_bracket=true\n"
        L"\n"
        L"no_section_key=456\n"
        L"\n"
        L"[valid_section]\n"
        L"=missing_key\n"
        L"\n"
        L"bad_bool=maybe\n"
        L"\n"
        L"bad_long=12abc\n"
        L"\n"
        L"[another]\n"
        L"valid=ok\n";
}

static void LoadTempConfig(ChmConfig& config,
                           TempConfigDir& dir,
                           const std::wstring& fileName,
                           const std::wstring& content)
{
    dir.WriteFile(fileName, content);
    config.SetBasePath(dir.BaseDir());
    EXPECT_TRUE(config.LoadFile(fileName));
}

} // namespace

TEST(ConfigTest, LoadFunctionKeyIni)
{
    TempConfigDir dir;
    ChmConfig config;
    LoadTempConfig(config, dir, L"funckey.ini", FunctionKeyConfigText());

    // duplicate を含んでいる想定
    EXPECT_FALSE(config.HasErrors());
    EXPECT_TRUE(config.HasInfos());

    std::wstring infos = config.DumpInfos();
    OutputDebugString(L"===FUNCKEY INFOS===");
    OutputDebugString(infos.c_str());

    // duplicate 文言が含まれていることを確認
    EXPECT_NE(std::wstring::npos, infos.find(L"duplicate"));

    // unknown やinvalid が含まれていないことを確認
    EXPECT_EQ(std::wstring::npos, infos.find(L"unknown"));
    EXPECT_EQ(std::wstring::npos, infos.find(L"invalid"));
}

TEST(ConfigTest, LoadValidFile)
{
    TempConfigDir dir;
    ChmConfig config;
    LoadTempConfig(config, dir, L"valid.ini", ValidConfigText());
    EXPECT_TRUE(config.GetBool(L"bools", L"bool-true-1"));

    std::wstring dump = config.Dump();
    OutputDebugString(L"===Tables");
    OutputDebugString(dump.c_str());
    EXPECT_FALSE(dump.empty());
    EXPECT_EQ(22, std::count(dump.begin(), dump.end(), L'\n'));

    std::wstring errors = config.DumpErrors();
    OutputDebugString(L"===ERRORS");
    OutputDebugString(errors.c_str());
    EXPECT_TRUE(errors.empty());
}

TEST(ConfigTest, BoolTest)
{
    TempConfigDir dir;
    ChmConfig config;
    LoadTempConfig(config, dir, L"valid.ini", ValidConfigText());

    EXPECT_TRUE(config.GetBool(L"bools", L"bool-true-1"));
    EXPECT_TRUE(config.GetBool(L"BOOLS", L"BOOL-TRUE-2"));
    EXPECT_TRUE(config.GetBool(L"Bools", L"Bool-True-3"));
    EXPECT_TRUE(config.GetBool(L"bools", L"bool-true-4"));
    EXPECT_TRUE(config.GetBool(L"bools", L"bool-true-5"));
    EXPECT_TRUE(config.GetBool(L"bools", L"bool-true-6"));
    EXPECT_FALSE(config.GetBool(L"bools", L"bool-false-1"));
    EXPECT_FALSE(config.GetBool(L"BOOLS", L"BOOL-FALSE-2"));
    EXPECT_FALSE(config.GetBool(L"Bools", L"Bool-False-3"));
    EXPECT_FALSE(config.GetBool(L"bools", L"bool-false-4"));
}

TEST(ConfigTest, LongTest)
{
    TempConfigDir dir;
    ChmConfig config;
    LoadTempConfig(config, dir, L"valid.ini", ValidConfigText());

    EXPECT_EQ(123, config.GetLong(L"numbers", L"long-1"));
    EXPECT_EQ(-789, config.GetLong(L"NUMBERS", L"Long-2"));
    EXPECT_EQ(2100000000, config.GetLong(L"numbers", L"long-3"));
}

TEST(ConfigTest, StringTest)
{
    TempConfigDir dir;
    ChmConfig config;
    LoadTempConfig(config, dir, L"valid.ini", ValidConfigText());

    EXPECT_EQ(L"Hitomoji", config.GetString(L"strings", L"string-1"));
    EXPECT_NE(L"hitomoji", config.GetString(L"strings", L"string-1"));
    EXPECT_NE(L"hITOMOJI", config.GetString(L"strings", L"string-1"));
    EXPECT_NE(L"hitomoji", config.GetString(L"strings", L"string-2"));
    EXPECT_EQ(L"C:\\temp\\FILE.txt", config.GetString(L"strings", L"valid-key-9"));
}

TEST(ConfigTest, InvalidClassAccess)
{
    TempConfigDir dir;
    ChmConfig config;
    LoadTempConfig(config, dir, L"valid.ini", ValidConfigText());

    // BOOL値をLongやStringで取ろうとした場合
    EXPECT_EQ(0, config.GetLong(L"bools", L"bool-true-1"));
    EXPECT_EQ(L"", config.GetString(L"bools", L"bool-true-1"));

    // long値をBoolやStringで取ろうとした場合
    EXPECT_FALSE(config.GetBool(L"numbers", L"long-1"));
    EXPECT_EQ(L"", config.GetString(L"numbers", L"long-1"));

    // string値をBoolやlongで取ろうとした場合
    EXPECT_FALSE(config.GetBool(L"strings", L"string-1"));
    EXPECT_EQ(0, config.GetLong(L"strings", L"string-1"));
}

TEST(ConfigTest, Canonize)
{
    TempConfigDir dir;
    ChmConfig config;
    LoadTempConfig(config, dir, L"valid.ini", ValidConfigText());

    EXPECT_TRUE(config.GetBool(L"canonize-1", L"bool-true-1"));
    EXPECT_TRUE(config.GetBool(L"canonize_1", L"bool_true_1"));

    EXPECT_TRUE(config.GetBool(L"canonize-2", L"bool-false-1"));
    EXPECT_TRUE(config.GetBool(L"canonize_2", L"bool_false_1"));
}

TEST(ConfigTest, DuplicateKey)
{
    TempConfigDir dir;
    ChmConfig config;
    LoadTempConfig(config, dir, L"duplicate.ini",
        L"\n"
        L"[ dup ]\n"
        L"value=1\n"
        L"value=2\n");

    // 最後の値が有効
    EXPECT_EQ(2, config.GetLong(L"dup", L"value"));

    EXPECT_FALSE(config.HasErrors());
    EXPECT_TRUE(config.HasInfos());

    std::wstring infos = config.DumpInfos();
    EXPECT_NE(std::wstring::npos, infos.find(L"duplicate key"));
}

TEST(ConfigTest, BlankFile)
{
    TempConfigDir dir;
    ChmConfig config;
    LoadTempConfig(config, dir, L"null.ini", L"");

    EXPECT_TRUE(config.GetBool(L"undefined", L"not-exist-bool-key"));
    EXPECT_EQ(0, config.GetLong(L"undefined", L"not-exist-long-key"));
    EXPECT_EQ(L"", config.GetString(L"undefined", L"not-exist-string-key"));
}

TEST(ConfigTest, EmptyFileWithCustomBasePath)
{
    TempConfigDir dir;
    dir.WriteFile(L"empty.ini", L"");

    ChmConfig config;
    config.SetBasePath(dir.BaseDir());

    EXPECT_TRUE(config.LoadFile(L"empty.ini"));
    EXPECT_FALSE(config.HasErrors());
    EXPECT_FALSE(config.HasInfos());
    EXPECT_TRUE(config.GetBool(L"undefined", L"not-exist-bool-key"));
    EXPECT_EQ(0, config.GetLong(L"undefined", L"not-exist-long-key"));
    EXPECT_EQ(L"", config.GetString(L"undefined", L"not-exist-string-key"));
}

TEST(ConfigTest, KeyTableLoadedFromConfigAffectsRomajiConverter)
{
    TempConfigDir dir;
    dir.WriteFile(L"override.ini",
        L"[key-table]\n"
        L"wi=うぃ\n"
        L"wwi=ゐ\n"
        L"->=→\n"
        L"<<=《\n");

    ChmConfig config;
    config.SetBasePath(dir.BaseDir());

    EXPECT_TRUE(config.LoadFile(L"override.ini"));
    EXPECT_FALSE(config.HasErrors());

    EXPECT_EQ(L"うぃ:", ConvertWithPendingForConfigTest(L"wi"));
    EXPECT_EQ(L"ゐ:", ConvertWithPendingForConfigTest(L"wwi"));
    EXPECT_EQ(L"→:", ConvertWithPendingForConfigTest(L"->"));
    EXPECT_EQ(L"《:", ConvertWithPendingForConfigTest(L"<<"));
}

TEST(ConfigTest, LoadFileClearsPreviousKeyTableOverrides)
{
    TempConfigDir dir;
    dir.WriteFile(L"override.ini",
        L"[key-table]\n"
        L"wi=うぃ\n");
    dir.WriteFile(L"empty.ini", L"");

    ChmConfig config;
    config.SetBasePath(dir.BaseDir());

    EXPECT_TRUE(config.LoadFile(L"override.ini"));
    EXPECT_EQ(L"うぃ:", ConvertWithPendingForConfigTest(L"wi"));

    EXPECT_TRUE(config.LoadFile(L"empty.ini"));
    EXPECT_EQ(L"\x3090:", ConvertWithPendingForConfigTest(L"wi"));
}

TEST(ConfigTest, LoadInvalidFile)
{
    TempConfigDir dir;
    dir.WriteFile(L"invalid.ini", InvalidConfigText());

    ChmConfig config;
    config.SetBasePath(dir.BaseDir());
    EXPECT_FALSE(config.LoadFile(L"invalid.ini"));

    EXPECT_TRUE(config.HasErrors());

    std::wstring errors = config.DumpErrors();
    OutputDebugString(errors.c_str());

    // 部分一致で見る（全文一致は壊れやすい）
    EXPECT_NE(std::wstring::npos, errors.find(L"missing ']'"));
    EXPECT_NE(std::wstring::npos, errors.find(L"empty key name"));

    // Get系でセクションが間違っているパターン（全部デフォルト値）
    EXPECT_TRUE(config.GetBool(L"bad-section", L"bad-bool"));
    EXPECT_EQ(0L, config.GetLong(L"bad-section", L"bad-long"));
    EXPECT_EQ(L"", config.GetString(L"bad-section", L"bad-string"));

    // Get系でキー名が間違っているパターン（全部デフォルト値）
    EXPECT_TRUE(config.GetBool(L"valid-section", L"not-exists-bool"));
    EXPECT_EQ(0L, config.GetLong(L"valid-section", L"not-exists-long"));
    EXPECT_EQ(L"", config.GetString(L"valid-section", L"not-exists-string"));

    // Get系でのエラーで、デフォルト値が指定されていた場合
    EXPECT_FALSE(config.GetBool(L"valid-section", L"not-exists-bool", FALSE));
    EXPECT_EQ(1234, config.GetLong(L"valid-section", L"not-exists-long", 1234));
}

TEST(ConfigTest, FileNotFound)
{
    TempConfigDir dir;
    ChmConfig config;
    config.SetBasePath(dir.BaseDir());
    EXPECT_FALSE(config.LoadFile(L"no-such-file.ini"));
}

// --- include + SetBasePath テスト ---
TEST(ConfigTest, IncludeWithCustomBasePath)
{
    TempConfigDir dir;
    dir.WriteFile(L"sub.ini",
        L"[numbers]\n"
        L"value=123\n");
    dir.WriteFile(L"main.ini",
        L"@include sub.ini\n");

    ChmConfig config;
    config.SetBasePath(dir.BaseDir());

    EXPECT_TRUE(config.LoadFile(L"main.ini"));
    EXPECT_EQ(123, config.GetLong(L"numbers", L"value"));
}
