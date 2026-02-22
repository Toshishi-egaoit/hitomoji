#include "gtest/gtest.h"
#include "ChmConfig.h"

TEST(ConfigTest, LoadFunctionKeyIni)
{
    ChmConfig config;

    EXPECT_FALSE(config.LoadFile(L".\\testdata\\funckey.ini"));

    // funckey.ini には duplicate を含んでいる想定
    EXPECT_TRUE(config.HasErrors());

    std::wstring errors = config.DumpErrors();
    OutputDebugString(L"===FUNCKEY ERRORS===");
    OutputDebugString(errors.c_str());

    // duplicate 文言が含まれていることを確認
    EXPECT_NE(std::wstring::npos, errors.find(L"duplicate"));
}


TEST(ConfigTest, LoadValidFile)

{
    ChmConfig config;

	EXPECT_TRUE(config.LoadFile(L".\\testdata\\valid.ini"));
    EXPECT_TRUE(config.GetBool(L"bools", L"bool-true-1"));

	std::wstring dump = config.Dump();
	OutputDebugString(L"===Tables");
	OutputDebugString(dump.c_str());
    EXPECT_FALSE(dump.empty());
	EXPECT_EQ(22, std::count(dump.begin(), dump.end(), L'\n'));   // 期待する行数

	std::wstring errors = config.DumpErrors();
	OutputDebugString(L"===ERRORS");
	OutputDebugString(errors.c_str());
    EXPECT_TRUE(errors.empty());
	
}

TEST(ConfigTest, BoolTest) 
{
    ChmConfig config;

	EXPECT_TRUE(config.LoadFile(L".\\testdata\\valid.ini"));

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
    ChmConfig config;

	EXPECT_TRUE(config.LoadFile(L".\\testdata\\valid.ini"));

    EXPECT_EQ(123, config.GetLong(L"numbers", L"long-1"));
    EXPECT_EQ(-789, config.GetLong(L"NUMBERS", L"Long-2"));
    EXPECT_EQ(2100000000, config.GetLong(L"numbers", L"long-3"));
}

TEST(ConfigTest, StringTest) 
{
    ChmConfig config;

	EXPECT_TRUE(config.LoadFile(L".\\testdata\\valid.ini"));

    EXPECT_EQ(L"Hitomoji", config.GetString(L"strings", L"string-1"));
    EXPECT_NE(L"hitomoji", config.GetString(L"strings", L"string-1"));
    EXPECT_NE(L"hITOMOJI", config.GetString(L"strings", L"string-1"));
    EXPECT_NE(L"hitomoji", config.GetString(L"strings", L"string-2"));
    EXPECT_EQ(L"C:\\temp\\FILE.txt", config.GetString(L"strings", L"valid-key-9"));
}

TEST(ConfigTest, InvalidClassAccess) 
{
    ChmConfig config;

	EXPECT_TRUE(config.LoadFile(L".\\testdata\\valid.ini"));

	// BOOL値をLongやStringで取ろうとした場合
    EXPECT_EQ(0,config.GetLong(L"bools", L"bool-true-1"));
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
    ChmConfig config;

	EXPECT_TRUE(config.LoadFile(L".\\testdata\\valid.ini"));

    EXPECT_TRUE(config.GetBool(L"canonize-1", L"bool-true-1"));
    EXPECT_TRUE(config.GetBool(L"canonize_1", L"bool_true_1"));

    EXPECT_TRUE(config.GetBool(L"canonize-2", L"bool-false-1"));
    EXPECT_TRUE(config.GetBool(L"canonize_2", L"bool_false_1"));
}

TEST(ConfigTest, DuplicateKey)
{
    ChmConfig config;
    EXPECT_FALSE(config.LoadFile(L".\\testdata\\duplicate.ini"));

    // 最後の値が有効
    EXPECT_EQ(2, config.GetLong(L"dup", L"value"));

    EXPECT_TRUE(config.HasErrors());

    std::wstring errors = config.DumpErrors();
    EXPECT_NE(std::wstring::npos, errors.find(L"duplicate key"));
}


TEST(ConfigTest, BlankFile)
{
    ChmConfig config;
	
	EXPECT_TRUE(config.LoadFile(L".\\testdata\\null.ini"));

    EXPECT_TRUE(config.GetBool(L"undefined", L"not-exist-bool-key"));
    EXPECT_EQ(0, config.GetLong(L"undefined", L"not-exist-long-key"));
    EXPECT_EQ(L"", config.GetString(L"undefined", L"not-exist-string-key"));
}

TEST(ConfigTest, LoadInvalidFile)
{
    ChmConfig config;
    EXPECT_FALSE(config.LoadFile(L".\\testdata\\invalid.ini"));

    EXPECT_TRUE(config.HasErrors());

    std::wstring errors = config.DumpErrors();
	OutputDebugString(errors.c_str());

    // 部分一致で見る（全文一致は壊れやすい）
    EXPECT_NE(std::wstring::npos, errors.find(L"missing ']'"));
    EXPECT_NE(std::wstring::npos, errors.find(L"empty key name"));

	// Get系でセクションが間違っているパターン（全部デフォルト値）
    EXPECT_TRUE(config.GetBool(L"bad-section", L"bad-bool"));
    EXPECT_EQ(0L,config.GetLong(L"bad-section", L"bad-long"));
    EXPECT_EQ(L"", config.GetString(L"bad-section", L"bad-string"));

	// Get系でキー名が間違っているパターン（全部デフォルト値）
    EXPECT_TRUE(config.GetBool(L"valid-section", L"not-exists-bool"));
    EXPECT_EQ(0L,config.GetLong(L"valid-section", L"not-exists-long"));
    EXPECT_EQ(L"", config.GetString(L"valid-section", L"not-exists-string"));

	// Get系でのエラーで、デフォルト値が指定されていた場合
    EXPECT_FALSE(config.GetBool(L"valid-section", L"not-exists-bool",FALSE));
    EXPECT_EQ(1234,config.GetLong(L"valid-section", L"not-exists-long",1234));
}

TEST(ConfigTest, FileNotFound)
{
    ChmConfig config;
    EXPECT_FALSE(config.LoadFile(L".\\testdata\\no-such-file.ini"));
}

