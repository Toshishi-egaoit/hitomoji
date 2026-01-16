#include <windows.h>
#include <msctf.h>
#include <cstdio>
#include "TsfIf.h"

class CDisplayAttributeInfo : public ITfDisplayAttributeInfo {
public:
    CDisplayAttributeInfo() : _cRef(1) {}

    // --- IUnknown 実装 ---
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

    // --- ITfDisplayAttributeInfo 実装 ---
    STDMETHODIMP GetGUID(GUID *pguid) { if (!pguid) return E_INVALIDARG; *pguid = s_myGuid; return S_OK; }
    STDMETHODIMP GetDescription(BSTR *pbstr) { if (!pbstr) return E_INVALIDARG; *pbstr = SysAllocString(L"Hitomoji Attr"); return S_OK; }
    
    STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE *pda) {
        if (!pda) return E_INVALIDARG;
        ZeroMemory(pda, sizeof(TF_DISPLAYATTRIBUTE));
        
        // 念願の「波線」指定
        pda->lsStyle = TF_LS_SQUIGGLE;      // 波線
        pda->fBoldLine = FALSE;
        pda->crLine.type = TF_CT_COLORREF;
        pda->crLine.nIndex = RGB(0, 0, 0); // 黒（お好みでRGB(255,0,0)にすると赤波線！）
        pda->bAttr = TF_ATTR_INPUT;
        return S_OK;
    }

    STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE *pda) { return E_NOTIMPL; }
    STDMETHODIMP Reset() { return S_OK; }

	// CDisplayAttributeInfo 独自処理の実装
    static HRESULT InitGuid(ITfCategoryMgr* pCategoryMgr) ;
    static TfGuidAtom GetAtom() ;
	static BOOL IsMyGuid(REFGUID guid) ;

private:
    long _cRef;
    static TfGuidAtom s_attrAtom;
	static const GUID s_myGuid; // DisplayAttribute.cppで定義
};
