#include <msctf.h>
#include <ocidl.h>
#include "TsfIf.h"
#include "resource.h"

class ChmLangBarItemButton : public ITfLangBarItemButton,
                           public ITfSource
{
public:
    ChmLangBarItemButton(REFGUID guidItem) : _cRef(1), _guidItem(guidItem) {
        // アイコン情報の初期化
        _info.clsidService = CLSID_Hitomoji;
        _info.guidItem = _guidItem;
        _info.dwStyle = TF_LBI_STYLE_BTN_BUTTON;
        _info.ulSort = 0;
        wcscpy_s(_info.szDescription, L"ひともじ");
    }

    // --- IUnknown ---
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj) {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = NULL;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfLangBarItem) || IsEqualIID(riid, IID_ITfLangBarItemButton))
            *ppvObj = (ITfLangBarItemButton *)this;
        else if (IsEqualIID(riid, IID_ITfSource))
            *ppvObj = (ITfSource *)this;
        
        if (*ppvObj) { AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&_cRef); }
    STDMETHODIMP_(ULONG) Release() {
        long cRef = InterlockedDecrement(&_cRef);
        if (cRef == 0) delete this;
        return cRef;
    }

    // --- ITfLangBarItem (基本情報) ---
    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO *pInfo) {
        *pInfo = _info;
        return S_OK;
    }
    STDMETHODIMP GetStatus(DWORD *pdwStatus) {
        *pdwStatus = 0; // 有効状態
        return S_OK;
    }
    STDMETHODIMP Show(BOOL fShow) { return S_OK; }
    STDMETHODIMP GetTooltipString(BSTR *pbstr) {
        *pbstr = SysAllocString(L"ひともじ入力");
        return S_OK;
    }

    // --- ITfLangBarItemButton (アイコン表示の核心) ---
    STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT *prcArea) { return S_OK; }
    STDMETHODIMP GetText(BSTR *pbstr) {
        *pbstr = SysAllocString(L"ひともじ");
        return S_OK;
    }
    STDMETHODIMP GetIcon(HICON *phIcon) {
        // ここが重要！DLL内のリソースからアイコンをロードして渡す
        // IDI_ICON1 はリソースファイルで定義したID
        *phIcon = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_HITOMOJI_ON), IMAGE_ICON, 16, 16, LR_SHARED);
        return *phIcon ? S_OK : E_FAIL;
    }
    STDMETHODIMP InitMenu(ITfMenu *pMenu) { return S_OK; }
    STDMETHODIMP OnMenuSelect(UINT wID) { return S_OK; }

    // --- ITfSource (言語バーへの通知用) ---
    STDMETHODIMP AdviseSink(REFIID riid, IUnknown *punk, DWORD *pdwCookie) {
        if (!IsEqualIID(riid, IID_ITfLangBarItemSink)) return E_NOINTERFACE;
        _pLangBarItemSink = (ITfLangBarItemSink *)punk;
        _pLangBarItemSink->AddRef();
        *pdwCookie = 1; // 簡易的なクッキー
        return S_OK;
    }
    STDMETHODIMP UnadviseSink(DWORD dwCookie) {
        if (_pLangBarItemSink) { _pLangBarItemSink->Release(); _pLangBarItemSink = NULL; }
        return S_OK;
    }

private:
    long _cRef;
    GUID _guidItem;
    TF_LANGBARITEMINFO _info;
    ITfLangBarItemSink *_pLangBarItemSink = nullptr;
};