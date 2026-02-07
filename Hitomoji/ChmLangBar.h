#include <msctf.h>
#include <ocidl.h>
#include "TsfIf.h"
#include "resource.h"

class ChmLangBarItemButton : public ITfLangBarItemButton,
                           public ITfSource
{
public:
        ChmLangBarItemButton(REFGUID guidItem) : _cRef(1), _guidItem(guidItem), _bOn(FALSE), _hIconOn(nullptr), _hIconOff(nullptr) {
        // アイコン情報の初期化
        _info.clsidService = CLSID_Hitomoji;
        _info.guidItem = _guidItem;
        _info.dwStyle = TF_LBI_STYLE_BTN_BUTTON;
        _info.ulSort = 0;
        wcscpy_s(_info.szDescription, L"ひともじ");
        LoadIcons();
    }
    ~ChmLangBarItemButton() {
        FreeIcons();
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

    // --- icon management (instance-based) ---
    void LoadIcons() {
        _hIconOn = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_HITOMOJI_ON), IMAGE_ICON, 16, 16, 0);
        _hIconOff = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(IDI_HITOMOJI_OFF), IMAGE_ICON, 16, 16, 0);
    }

    void FreeIcons() {
        if (_hIconOn) { DestroyIcon(_hIconOn); _hIconOn = nullptr; }
        if (_hIconOff) { DestroyIcon(_hIconOff); _hIconOff = nullptr; }
    }

	HRESULT AddToLangBar(ITfThreadMgr *pThreadMgr) { // 引数で ThreadMgr を受け取る
		if (!pThreadMgr) return E_INVALIDARG;

		OutputDebugString(L"ChmLangBarItemButton::AddToLangBar called");
		ITfLangBarItemMgr* pMgr = nullptr;
		// ThreadMgr に対して「言語バーマネージャーを貸して！」と頼む
		HRESULT hr = pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr, (void**)&pMgr);
    
		if (SUCCEEDED(hr)) {
			OutputDebugString(L"skip AddItem");
			//hr = pMgr->AddItem(this); 
			pMgr->Release();
		}
		OUTPUT_HR_ON_ERROR(L"AddToLangBar.AddItem (via ThreadMgr)", hr);
		return hr;
	}

	HRESULT RemoveFromLangBar() {
		ITfLangBarItemMgr* pMgr = nullptr;
		HRESULT hr = CoCreateInstance(
			CLSID_TF_LangBarItemMgr, nullptr, CLSCTX_INPROC_SERVER,
			IID_ITfLangBarItemMgr, (void**)&pMgr);
		OUTPUT_HR_n_RETURN_ON_ERROR(L"CoCreateInstance:CLSID_TF_LangBarItemMgr", hr);

		hr = pMgr->RemoveItem(this); // Release される
		pMgr->Release();
		OUTPUT_HR_ON_ERROR(L"RemoveFromLangBar.RemoveItem", hr);
		return hr;
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
        *phIcon = (_bOn) ? _hIconOn : _hIconOff;
		OutputDebugStringWithInt(L"   > GetIcon: *phIcon=%x", (ULONG)(*phIcon));
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

    // --- IME 状態切り替え ---
    // Engine 側の IsOn() / BOOL インタフェースに合わせる
    void SetImeState(BOOL bOn) {
        if (_bOn == bOn) return;
        _bOn = bOn;
        if (_pLangBarItemSink) {
            _pLangBarItemSink->OnUpdate(TF_LBI_ICON);
        }
    }

private:
    long _cRef;
    GUID _guidItem;
    TF_LANGBARITEMINFO _info;
    ITfLangBarItemSink *_pLangBarItemSink = nullptr;
    BOOL _bOn;

    // icon handles
    HICON _hIconOn;
    HICON _hIconOff;
};
