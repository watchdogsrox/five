// =======================================
// debug/AssetAnalysis/GeometryCollector.h
// (c) 2013 RockstarNorth
// =======================================

#ifndef _DEBUG_ASSETANALYSIS_GEOMETRYCOLLECTOR_H_
#define _DEBUG_ASSETANALYSIS_GEOMETRYCOLLECTOR_H_

#if __BANK

namespace rage { class bkBank; }

namespace GeomCollector {

void AddWidgets(bkBank& bk);

void ProcessDwdSlot(int slot);
void ProcessDrawableSlot(int slot);
void ProcessFragmentSlot(int slot);
void ProcessMapDataSlot(int slot);

void Start();
void FinishAndReset();

} // namespace GeomCollector

#endif // __BANK

// ================================================================================================

#if __BANK || (__WIN32PC && !__FINAL)

#include "atl/array.h"
#include "atl/string.h"

namespace GeomCollector {

class CPropGroupEntry
{
public:
	CPropGroupEntry() {}
	CPropGroupEntry(const char* name, float fRadius) : m_name(name), m_fRadius(fRadius) {}

	atString m_name;
	float m_fRadius;
};

class CPropGroup
{
public:
	CPropGroup() {}
	CPropGroup(const char* name) : m_name(name) {}

	atString m_name;
	atArray<CPropGroupEntry> m_entries;
};

const atArray<CPropGroup>& GetPropGroups();
bool IsInPropGroups(const char* pModelName);

} // namespace GeomCollector

#endif // __BANK || (__WIN32PC && !__FINAL)
#endif // _DEBUG_ASSETANALYSIS_GEOMETRYCOLLECTOR_H_
