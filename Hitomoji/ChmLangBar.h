#include <msctf.h>
#include <ocidl.h>
#include "TsfIf.h"
#include "resource.h"

class ChmLangBarItemButton : public ITfLangBarItemButton,
                           public ITfSource
{
public:
        ChmLangBarItemButton(REFGUID guidItem, ChmTsfInterface* pTsfIf) 
		: _cRef(1), _pTsfIf(pTsfIf), _guidItem(guidItem), _bOn(FALSE), _hIconOn(nullptr), _hIconOff(nullptr) {
        // アイコン情報の初期化
        _info.clsidService = CLSID_Hitomoji;
        _info.guidItem = _guidItem;
        _info.dwStyle = TF_LBI_STYLE_BTN_MENU;
        _info.ulSort = 10;
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

		ChmLogger::Debug(L"AddToLangBar called");
		ITfLangBarItemMgr* pMgr = nullptr;
		// ThreadMgr に対して「言語バーマネージャーを貸して！」と頼む
		HRESULT hr = pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr, (void**)&pMgr);
    
		if (SUCCEEDED(hr)) {
			hr = pMgr->AddItem(this); 
			OUTPUT_HR_ON_ERROR(L"   > AddToLangBar.AddItem ", hr);
			pMgr->Release();
		}
		return hr;
	}

	HRESULT RemoveFromLangBar(ITfThreadMgr* pThreadMgr) {
        if (!pThreadMgr) return E_INVALIDARG;
        ChmLogger::Debug(L"   > RemoveFromLangBar called");

        ITfLangBarItemMgr* pMgr = nullptr;
        HRESULT hr = pThreadMgr->QueryInterface(IID_ITfLangBarItemMgr, (void**)&pMgr);
        if (SUCCEEDED(hr)) {
            hr = pMgr->RemoveItem(this);
			OUTPUT_HR_ON_ERROR(L"   > RemoveFromLangBar.RemoveItem", hr);
            pMgr->Release();
        }
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

    STDMETHODIMP Show(BOOL fShow) { 
		return S_OK; 
	}

    STDMETHODIMP GetTooltipString(BSTR *pbstr) {
        *pbstr = SysAllocString(L"ひともじON/OFF");
        return S_OK;
    }

    STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT *prcArea) { 
		_pTsfIf->ToggleIME();
		return S_OK; }

    STDMETHODIMP GetText(BSTR *pbstr) {
        *pbstr = SysAllocString(L"ひともじ");
        return S_OK;
    }
    STDMETHODIMP GetIcon(HICON *phIcon) {
        *phIcon = (_bOn) ? _hIconOn : _hIconOff;
		OutputDebugStringWithString(L"   > GetIcon: %s", (*phIcon == _hIconOn) ? L"ON" : L"OFF");
        return *phIcon ? S_OK : E_FAIL;
    }

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
			_pLangBarItemSink->OnUpdate(TF_LBI_ICON | TF_LBI_STATUS);
        }
    }

	// --- 独自メニューの処理 ---
	STDMETHODIMP InitMenu(ITfMenu *pMenu)
	{
		HRESULT hr;
		hr = pMenu->AddMenuItem(1, 0, nullptr, nullptr, L"Open Config Folder", 18, nullptr);
		OUTPUT_HR_ON_ERROR(L"AddMenuItem(Open Folder)", hr);
		hr = pMenu->AddMenuItem(2, 0, nullptr, nullptr, L"Edit Config", 11, nullptr);
		OUTPUT_HR_ON_ERROR(L"AddMenuItem(Edit Config)", hr);
		hr = pMenu->AddMenuItem(3, 0, nullptr, nullptr, L"Reload Config", 13, nullptr);
		OUTPUT_HR_ON_ERROR(L"AddMenuItem(Reload Config)", hr);

		return S_OK;
	}

	STDMETHODIMP OnMenuSelect(UINT wID)
	{
		switch (wID)
		{
		case 1:
			_pTsfIf->OpenFolder();
			break;
		case 2:
			_pTsfIf->OpenConfig();
			break;
		case 3:
			_pTsfIf->ReloadConfig();
			break;
		}
		return S_OK;
	}

private:
    long _cRef;
    GUID _guidItem;
    TF_LANGBARITEMINFO _info;
    ITfLangBarItemSink *_pLangBarItemSink = nullptr;
	ChmTsfInterface *_pTsfIf;
    BOOL _bOn;

    // icon handles
    HICON _hIconOn;
    HICON _hIconOff;
};
