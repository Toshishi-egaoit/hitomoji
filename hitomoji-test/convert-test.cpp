#include "gtest/gtest.h"
#include "ChmRomajiConverter.h"

class ConvertOverrideTest : public ::testing::Test {
protected:
    void SetUp() override {
        ChmKeytableParser::ClearOverrideTable();
    }
};

static std::wstring ConvertWithPending(const std::wstring& input,
                                       bool bs = true)
{
    std::wstring converted;
    std::wstring pending;

    ChmRomajiConverter::convert(input, converted, pending, bs);
    return converted + L":" + pending;
}

//
// 基本
//
TEST(ConvertTest, BasicVowel)
{
    EXPECT_EQ(L"あ:", ConvertWithPending(L"a"));
}

TEST(ConvertTest, BasicConsonant)
{
    EXPECT_EQ(L"か:", ConvertWithPending(L"ka"));
}

TEST(ConvertTest, Nn)
{
    EXPECT_EQ(L"ん:", ConvertWithPending(L"nn"));
}

//
// 拗音
//
TEST(ConvertTest, Youon)
{
    EXPECT_EQ(L"きゃ:", ConvertWithPending(L"kya"));
    EXPECT_EQ(L"しゃ:", ConvertWithPending(L"sha"));
    EXPECT_EQ(L"りょ:", ConvertWithPending(L"ryo"));
}

//
// 促音
//
TEST(ConvertTest, Sokuon)
{
    EXPECT_EQ(L"った:", ConvertWithPending(L"tta"));
    EXPECT_EQ(L"っか:", ConvertWithPending(L"kka"));
}

//
// 記号
//
TEST(ConvertTest, Symbols)
{
    EXPECT_EQ(L"。:", ConvertWithPending(L"."));
    EXPECT_EQ(L"、:", ConvertWithPending(L","));
    EXPECT_EQ(L"？:", ConvertWithPending(L"?"));
    EXPECT_EQ(L"「:", ConvertWithPending(L"["));
}

//
// pending
//
TEST(ConvertTest, PendingCases)
{
    EXPECT_EQ(L":k", ConvertWithPending(L"k"));
    EXPECT_EQ(L":ky", ConvertWithPending(L"ky"));
    EXPECT_EQ(L"っ:t", ConvertWithPending(L"tt"));
}

//
// フラグ差分
//
TEST(ConvertTest, BackspaceMode)
{
    ConvertWithPending(L"ka", true);
    EXPECT_EQ(2, ChmRomajiConverter::GetLastRawUnitLength());

    EXPECT_EQ(L":k", ConvertWithPending(L"k"));
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());
    EXPECT_EQ(L":ky", ConvertWithPending(L"ky"));
    EXPECT_EQ(2, ChmRomajiConverter::GetLastRawUnitLength());
    EXPECT_EQ(L"dcあ:", ConvertWithPending(L"dca"));
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());
    EXPECT_EQ(L"chま:", ConvertWithPending(L"chma"));
    EXPECT_EQ(2, ChmRomajiConverter::GetLastRawUnitLength());
    EXPECT_EQ(L"っ:t", ConvertWithPending(L"tt"));
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());

    EXPECT_EQ(L":ky", ConvertWithPending(L"ky",false));
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());
}

TEST(ConvertTest, RawLengthConverted)
{
    ConvertWithPending(L"a");
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());
    ConvertWithPending(L"ka");
    EXPECT_EQ(2, ChmRomajiConverter::GetLastRawUnitLength());
    ConvertWithPending(L"cha");
    EXPECT_EQ(3, ChmRomajiConverter::GetLastRawUnitLength());
}

TEST(ConvertTest, RawLengthPending)
{
    ConvertWithPending(L"k");
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());
    ConvertWithPending(L"ch");
    EXPECT_EQ(2, ChmRomajiConverter::GetLastRawUnitLength());
    EXPECT_EQ(L":chm", ConvertWithPending(L"chm"));
    EXPECT_EQ(3, ChmRomajiConverter::GetLastRawUnitLength());
    EXPECT_EQ(L"chま:t", ConvertWithPending(L"chmat"));
    EXPECT_EQ(1, ChmRomajiConverter::GetLastRawUnitLength());
}

//
// override
//
TEST_F(ConvertOverrideTest, OverrideLongestMatch)
{
    ChmKeytableParser::RegisterOverrideTable(L"wi", L"うぃ");
    ChmKeytableParser::RegisterOverrideTable(L"wwi", L"ゐ");

    EXPECT_EQ(L"ゐ:", ConvertWithPending(L"wwi"));
    EXPECT_EQ(L"うぃ:", ConvertWithPending(L"wi"));
}

TEST_F(ConvertOverrideTest, OverrideSymbols)
{
    ChmKeytableParser::RegisterOverrideTable(L"<<", L"《");
    ChmKeytableParser::RegisterOverrideTable(L">>", L"》");

    EXPECT_EQ(L"《:", ConvertWithPending(L"<<"));
    EXPECT_EQ(L"》:", ConvertWithPending(L">>"));
}

TEST_F(ConvertOverrideTest, OverrideFallbackToBase)
{
    ChmKeytableParser::RegisterOverrideTable(L"ka", L"X");
    EXPECT_EQ(L"X:", ConvertWithPending(L"ka"));

    // base still works for others
    EXPECT_EQ(L"あ:", ConvertWithPending(L"a"));
}
