# Browser Password Mode Investigation

## Purpose

Investigate why Hitomoji does not automatically turn IME off when focus enters password fields in browsers.

Hitomoji already turns IME off in some application-specific contexts, such as Vim mode changes, so the browser password field behavior is likely a TSF context/input-scope detection gap rather than a general IME state problem.

## Scope

This branch is for investigation only. It may contain temporary logging, reproduction pages, and notes. The final fix should be made separately from a clean main branch after the cause is understood.

## Current Hypothesis

The existing password detection path uses `ITfInputScope` from the focused `ITfContext` and checks whether any returned `InputScope` is `IS_PASSWORD`.

Browser password fields may differ in one of these ways:

- The expected focus/context event is not fired when entering a password field.
- The focused TSF context does not expose `ITfInputScope`.
- `GetInputScopes()` returns no scopes or a scope other than `IS_PASSWORD`.
- The browser updates password state after the event where Hitomoji currently checks it.
- The password field is represented through a different document/context path than native controls.

## Instrumentation

Temporary `OutputDebugString` logs were added in `Hitomoji/TsfIf.cpp` around:

- `ITfKeyEventSink::OnSetFocus`
- `ITfThreadMgrEventSink::OnSetFocus`
- `OnPushContext`
- compartment `OnChange`
- `_ApplyAppInputMode`
- `_IsPasswordContext`

The important values to compare are:

- Whether `_ApplyAppInputMode()` is called when focus enters text/password fields.
- Whether `ITfInputScope` can be queried from the focused context.
- Whether `ITfContext::GetProperty(GUID_PROP_INPUTSCOPE)` succeeds even when `ITfInputScope` is not exposed directly.
- The `HRESULT`, count, and values returned by `GetInputScopes()`.
- Whether Hitomoji turns IME off because of password scope or falls through to open/close compartment sync.

## Reproduction Page

Use `password-test.html` in this directory.

Open it as a local file first:

```text
file:///E:/project/hitomoji/research/browser-password-mode/password-test.html
```

The page includes:

- plain text input
- password input
- new password input
- search input
- email input
- textarea
- contenteditable element

The page also shows browser-side focus, key, input, and composition events. Compare those events with Hitomoji's debug output.

## Observations

- Vim mode changes do not use password/input-scope detection. `ITfInputScope` is not exposed there either; Hitomoji follows `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE` changes.
- Firefox, SeaMonkey, and Edge reached `_ApplyAppInputMode()` for browser text/password fields, but direct `QueryInterface(IID_ITfInputScope)` on the focused `ITfContext` returned `E_NOINTERFACE` (`0x80004002`).
- SDK headers define both `ITfInputScope` and `GUID_PROP_INPUTSCOPE`. The next check is whether browser contexts expose input scope as a TSF property even when they do not expose `ITfInputScope` directly.
## 2026-06-27 Compartment Findings

Captured log: `2026-06-27-browser-field-compartments.log`.

The compartment direction was correct, but the original check looked only at the thread manager compartment. Browser password fields can publish the disable request on the focused context compartment instead.

Key observations:

- Vim and VsVim mode changes are represented by `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE` changing on the thread manager compartment.
- Browser password fields can expose `GUID_COMPARTMENT_EMPTYCONTEXT = VT_I4:1` and `GUID_COMPARTMENT_KEYBOARD_DISABLED = VT_I4:1` on the focused context compartment.
- The same password-field events still show the thread manager `GUID_COMPARTMENT_EMPTYCONTEXT` and `GUID_COMPARTMENT_KEYBOARD_DISABLED` values as `VT_EMPTY`, so the current `_GetCompartmentBool()` path returns `keyboardDisabled=0 emptyContext=0`.
- Browser fields such as number, tel, email, textarea, and contenteditable did not show the same `KEYBOARD_DISABLED=1` context signal in the captured run. Some contexts expose `EMPTYCONTEXT` with `VT_EMPTY`, which should not be treated as a disable request.
- `ITfInputScope` and `GUID_PROP_INPUTSCOPE` were still unavailable in these browser contexts, so the input-scope path is not useful for this bug.

Inferred cause:

`_ApplyAppInputMode(pic)` receives the focused `ITfContext`, but its disable checks use only the thread manager compartment. That misses browser password fields where the security-related state is attached to the context.

Likely fix:

- Add a helper that reads a boolean compartment from any `IUnknown` exposing `ITfCompartmentMgr`.
- In `_ApplyAppInputMode(pic)`, check `GUID_COMPARTMENT_KEYBOARD_DISABLED` and `GUID_COMPARTMENT_EMPTYCONTEXT` on the focused context first.
- Also keep the existing thread manager checks as a fallback.
- If either the context or thread manager has a real boolean/integer disable value, force Hitomoji IME off.
- If no disable request is present, continue syncing to `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE` as before.

This keeps Vim/VsVim behavior intact while allowing browser password fields to disable Hitomoji through the context compartment.

