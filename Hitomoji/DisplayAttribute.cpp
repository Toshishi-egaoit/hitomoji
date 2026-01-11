// DisplayAttribute.cpp
#include <initguid.h>
#include "DisplayAttribute.h"

// CDisplayAttributeInfo 独自処理の実装
HRESULT CDisplayAttributeInfo::InitGuid(ITfCategoryMgr* pCategoryMgr) {
	if (!pCategoryMgr) return E_INVALIDARG;
	if (s_attrAtom != 0) return S_OK; // already initialized (idempotent)

	return pCategoryMgr->RegisterGUID(s_myGuid, &s_attrAtom);
}

// Get registered Atom (0 if not initialized)
TfGuidAtom CDisplayAttributeInfo::GetAtom() {
	return s_attrAtom;
}

// check GUID
BOOL CDisplayAttributeInfo::IsMyGuid(REFGUID guid) {
	return IsEqualGUID(guid, s_myGuid);
}

// static member definition

TfGuidAtom CDisplayAttributeInfo::s_attrAtom = 0;

// 「入力中（変換前）」の標準スタイル(GUID_ATTR_INPUT相当)
// {191D0A24-AD9A-411a-9488-A7235677A382}
const GUID CDisplayAttributeInfo::s_myGuid = { 0x191d0a24, 0xad9a, 0x411a, { 0x94, 0x88, 0xa7, 0x23, 0x56, 0x77, 0xa3, 0x82 }};
