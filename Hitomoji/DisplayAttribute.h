#include <windows.h>
#include <msctf.h>
#include <cstdio>
#include "Hitomoji.h"

class CDisplayAttributeInfo : public ITfDisplayAttributeInfo {
public:
    CDisplayAttributeInfo() : _cRef(1) {}

    // --- IUnknown ŽÀ‘• ---
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj) {
        if (!ppvObj) return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfDisplayAttributeInfo)) {
            *ppvObj = (ITfDisplayAttributeInfo*)this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&_cRef); }
    STDMETHODIMP_(ULONG) Release() {
        ULONG res = InterlockedDecrement(&_cRef);
        if (res == 0) delete this;
        return res;
    }

    // --- ITfDisplayAttributeInfo ŽÀ‘• ---
    STDMETHODIMP GetGUID(GUID *pguid) { if (!pguid) return E_INVALIDARG; *pguid = GUID_ATTR_INPUT; return S_OK; }
    STDMETHODIMP GetDescription(BSTR *pbstr) { if (!pbstr) return E_INVALIDARG; *pbstr = SysAllocString(L"Hitomoji Attr"); return S_OK; }
    
    STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE *pda) {
        if (!pda) return E_INVALIDARG;
        ZeroMemory(pda, sizeof(TF_DISPLAYATTRIBUTE));
        
        // ”OŠè‚Ìu”güvŽw’è
        pda->lsStyle = TF_LS_SQUIGGLE;      // ”gü
        pda->fBoldLine = FALSE;
        pda->crLine.type = TF_CT_COLORREF;
        pda->crLine.nIndex = RGB(0, 0, 0); // •i‚¨D‚Ý‚ÅRGB(255,0,0)‚É‚·‚é‚ÆÔ”güIj
        pda->bAttr = TF_ATTR_INPUT;
        return S_OK;
    }

    STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE *pda) { return E_NOTIMPL; }
    STDMETHODIMP Reset() { return S_OK; }

private:
    long _cRef;
};