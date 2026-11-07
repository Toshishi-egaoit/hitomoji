// DisplayAttribute.cpp
#include <initguid.h>
#include "DisplayAttribute.h"

// CDisplayAttributeInfo 独自処理の実装
HRESULT CDisplayAttributeInfo::InitGuid(ITfCategoryMgr* pCategoryMgr) {
	if (!pCategoryMgr) return E_INVALIDARG;
	if (s_provisionalAtom != 0) return S_OK;

	HRESULT hr = pCategoryMgr->RegisterGUID(s_myGuid, &s_attrAtom);
	if (FAILED(hr)) return hr;
	return pCategoryMgr->RegisterGUID(s_provisionalGuid, &s_provisionalAtom);
}

// Get registered Atom (0 if not initialized)
TfGuidAtom CDisplayAttributeInfo::GetAtom(bool provisional) {
	return provisional ? s_provisionalAtom : s_attrAtom;
}

// check GUID
BOOL CDisplayAttributeInfo::IsMyGuid(REFGUID guid) {
	return IsEqualGUID(guid, s_myGuid);
}

BOOL CDisplayAttributeInfo::IsProvisionalGuid(REFGUID guid) {
	return IsEqualGUID(guid, s_provisionalGuid);
}

// static member definition

TfGuidAtom CDisplayAttributeInfo::s_attrAtom = 0;
TfGuidAtom CDisplayAttributeInfo::s_provisionalAtom = 0;

// 「入力中（変換前）」の標準スタイル(GUID_ATTR_INPUT相当)
// {191D0A24-AD9A-411a-9488-A7235677A382}
const GUID CDisplayAttributeInfo::s_myGuid = { 0x191d0a24, 0xad9a, 0x411a, { 0x94, 0x88, 0xa7, 0x23, 0x56, 0x77, 0xa3, 0x82 }};
const GUID CDisplayAttributeInfo::s_provisionalGuid = { 0x30f5ca52, 0x748b, 0x4e3a, { 0xa1, 0xd2, 0x59, 0x73, 0x7e, 0x65, 0x5c, 0xa0 }};
