#include "gtest/gtest.h"
#include "ChmConfig.h"

TEST(ConfigTest, LoadValidFile)
{
    ChmConfig config;

	EXPECT_TRUE(config.LoadFile(L".\\testdata\\valid.ini"));

    EXPECT_TRUE(config.GetBool(L"bools", L"bool_true_1"));
    EXPECT_TRUE(config.GetBool(L"BOOLS", L"BOOL_TRUE_2"));
    EXPECT_TRUE(config.GetBool(L"Bools", L"Bool_True_3"));
    EXPECT_TRUE(config.GetBool(L"bools", L"bool_true_4"));
    EXPECT_TRUE(config.GetBool(L"bools", L"bool_true_5"));
    EXPECT_TRUE(config.GetBool(L"bools", L"bool_true_6"));
    EXPECT_FALSE(config.GetBool(L"bools", L"bool_false_1"));
    EXPECT_FALSE(config.GetBool(L"BOOLS", L"BOOL_FALSE_2"));
    EXPECT_FALSE(config.GetBool(L"Bools", L"Bool_False_3"));
    EXPECT_FALSE(config.GetBool(L"bools", L"bool_false_4"));

    EXPECT_EQ(123, config.GetLong(L"numbers", L"long_1"));
    EXPECT_EQ(-789, config.GetLong(L"NUMBERS", L"Long_2"));
    EXPECT_EQ(2100000000, config.GetLong(L"numbers", L"long_3"));

    EXPECT_EQ(L"Hitomoji", config.GetString(L"strings", L"string_1"));
    EXPECT_NE(L"hitomoji", config.GetString(L"strings", L"string_1"));
    EXPECT_NE(L"hITOMOJI", config.GetString(L"strings", L"string_1"));
    EXPECT_EQ(L"C:\\temp\\FILE.txt", config.GetString(L"strings", L"valid_key_9"));
}

TEST(ConfigTest, WhitespaceLine)
{
    ChmConfig config;
	
	EXPECT_TRUE(config.LoadFile(L".\\testdata\\null.ini"));

    EXPECT_FALSE(config.GetBool(L"undefined", L"not_exist_bool_key"));
    EXPECT_EQ(0, config.GetLong(L"undefined", L"not_exist_long_key"));
    EXPECT_EQ(L"", config.GetString(L"undefined", L"not_exist_string_key"));
}

TEST(ConfigTest, LoadInvalidFile)
{
    ChmConfig config;
    EXPECT_FALSE(config.LoadFile(L".\\testdata\\invalid.ini"));

    EXPECT_TRUE(config.HasErrors());

    std::wstring errors = config.DumpErrors();

    // ïîï™àÍívÇ≈å©ÇÈÅiëSï∂àÍívÇÕâÛÇÍÇ‚Ç∑Ç¢Åj
    EXPECT_NE(std::wstring::npos, errors.find(L"missing ']'"));
    EXPECT_NE(std::wstring::npos, errors.find(L"empty key name"));

    EXPECT_FALSE(config.GetBool(L"valid_section", L"bad_bool"));
    EXPECT_EQ(0L,config.GetLong(L"valid_section", L"bad_long"));

}

TEST(ConfigTest, FileNotFound)
{
    ChmConfig config;
    EXPECT_FALSE(config.LoadFile(L".\\testdata\\no_such_file.ini"));
}

