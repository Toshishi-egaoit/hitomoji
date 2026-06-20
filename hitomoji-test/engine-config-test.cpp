#include "gtest/gtest.h"
#include "ChmConfig.h"
#include "ChmEngine.h"
#include "ChmEnvironment.h"
#include "ChmKeyEvent.h"
#include "ChmRomajiConverter.h"
#include "TestConfigHelper.h"
#include "TestKeyEventHelper.h"
#include <string>

namespace {

std::wstring ConvertWithPending(const std::wstring& input, bool bs = true)
{
    std::wstring converted;
    std::wstring pending;
    ChmRomajiConverter::convert(input, converted, pending, bs);
    return converted + L":" + pending;
}
ChmFuncType TypeForKey(WPARAM key, bool ctrl = false,
                       ChmEngine::State state = ChmEngine::State::Inputing)
{
    TestKeyEventHelper::SetState(false, ctrl);
    ChmKeyEvent event(key, 0, state);
    return event.GetType();
}


class EngineConfigTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        g_environment.Init();
        _oldBasePath = g_environment.GetBasePath();
        g_environment.SetBasePath(_dir.BaseDir());
        TestKeyEventHelper::Install();
        ChmKeyEvent::InitFunctionKey();
    }

    void TearDown() override
    {
        TestKeyEventHelper::Restore();
        ChmKeyEvent::InitFunctionKey();
        g_environment.SetBasePath(_oldBasePath);
    }

    void WriteConfig(const std::wstring& content)
    {
        _dir.WriteFile(L"Hitomoji.ini", content);
    }

    TempConfigDir _dir;
    std::wstring _oldBasePath;
};

} // namespace

TEST_F(EngineConfigTest, ActivateLoadsConfigFromEnvironmentBasePath)
{
    WriteConfig(
        L"[ui]\n"
        L"loglevel=debug\n"
        L"candidate-delay=321\n");

    ChmEngine engine;
    engine.Activate();

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(_dir.BaseDir(), engine.GetConfig()->GetConfigPath());
    EXPECT_EQ(321, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));

    engine.Deactivate();
}

TEST_F(EngineConfigTest, ActivateUsesEmptyConfigWhenInitialLoadFails)
{
    ChmEngine engine;
    engine.Activate();

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(_dir.BaseDir(), engine.GetConfig()->GetConfigPath());
    EXPECT_TRUE(engine.GetConfig()->GetBool(L"undefined", L"missing-bool"));
    EXPECT_EQ(0, engine.GetConfig()->GetLong(L"undefined", L"missing-long"));
    EXPECT_EQ(L"", engine.GetConfig()->GetString(L"undefined", L"missing-string"));

    engine.Deactivate();
}

TEST_F(EngineConfigTest, InitConfigReplacesExistingConfigAndFunctionKeysAfterSuccessfulReload)
{
    WriteConfig(
        L"[ui]\n"
        L"candidate-delay=111\n"
        L"[function-key]\n"
        L"CTRL+Y=cancel\n");

    ChmEngine engine;
    engine.Activate();

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(111, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(ChmFuncType::Cancel, TypeForKey('Y', true));

    WriteConfig(
        L"[ui]\n"
        L"candidate-delay=222\n"
        L"[function-key]\n"
        L"CTRL+Y=finish-raw\n");
    engine.InitConfig();

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(222, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(ChmFuncType::CompFinishKey, TypeForKey('Y', true));

    engine.Deactivate();
}

TEST_F(EngineConfigTest, InitConfigKeepsExistingConfigAndFunctionKeysAfterFailedReload)
{
    WriteConfig(
        L"[ui]\n"
        L"candidate-delay=111\n"
        L"[function-key]\n"
        L"CTRL+Y=cancel\n");

    ChmEngine engine;
    engine.Activate();

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(111, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(ChmFuncType::Cancel, TypeForKey('Y', true));

    WriteConfig(
        L"[ui\n"
        L"candidate-delay=222\n"
        L"[function-key]\n"
        L"CTRL+Y=finish-raw\n");
    engine.InitConfig();

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(111, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(ChmFuncType::Cancel, TypeForKey('Y', true));

    engine.Deactivate();
}
TEST_F(EngineConfigTest, DISABLED_InitConfigKeepsKeyTableOverridesAfterFailedReload)
{
    WriteConfig(
        L"[ui]\n"
        L"candidate-delay=111\n"
        L"[key-table]\n"
        L"wi=うぃ\n");

    ChmEngine engine;
    engine.Activate();

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(111, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(L"うぃ:", ConvertWithPending(L"wi"));

    WriteConfig(
        L"[ui\n"
        L"candidate-delay=222\n"
        L"[key-table]\n"
        L"wi=ゐ\n");
    engine.InitConfig();

    ASSERT_NE(nullptr, engine.GetConfig());
    EXPECT_EQ(111, engine.GetConfig()->GetLong(L"ui", L"candidate-delay"));
    EXPECT_EQ(L"うぃ:", ConvertWithPending(L"wi"));

    engine.Deactivate();
}
