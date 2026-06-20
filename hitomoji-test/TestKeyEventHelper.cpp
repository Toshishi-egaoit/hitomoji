#include "TestKeyEventHelper.h"

namespace {

bool g_shift = false;
bool g_ctrl = false;
bool g_alt = false;
bool g_caps = false;

} // namespace

namespace TestKeyEventHelper {

void SetState(bool shift, bool ctrl, bool alt, bool caps)
{
    g_shift = shift;
    g_ctrl = ctrl;
    g_alt = alt;
    g_caps = caps;
}

void ResetState()
{
    SetState();
}

SHORT __stdcall KeyStateProvider(int vkey)
{
    switch (vkey)
    {
    case VK_SHIFT:   return g_shift ? static_cast<SHORT>(0x8000) : 0;
    case VK_CONTROL: return g_ctrl  ? static_cast<SHORT>(0x8000) : 0;
    case VK_MENU:    return g_alt   ? static_cast<SHORT>(0x8000) : 0;
    case VK_CAPITAL: return g_caps  ? static_cast<SHORT>(0x0001) : 0;
    default:         return 0;
    }
}

void Install()
{
    ChmKeyEvent::SetKeyStateProvider(KeyStateProvider);
    ResetState();
}

void Restore()
{
    ResetState();
    ChmKeyEvent::SetKeyStateProvider(::GetKeyState);
}

} // namespace TestKeyEventHelper
