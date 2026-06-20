#pragma once

#include "ChmKeyEvent.h"

namespace TestKeyEventHelper {

void SetState(bool shift = false, bool ctrl = false,
              bool alt = false, bool caps = false);
void ResetState();
SHORT __stdcall KeyStateProvider(int vkey);
void Install();
void Restore();

} // namespace TestKeyEventHelper
