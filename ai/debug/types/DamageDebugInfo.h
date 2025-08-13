#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "scene\RegdRefTypes.h"

class CDamageDebugInfo : public CAIDebugInfo
{
public:

	CDamageDebugInfo(const CPed* pPed, bool bOnlyDamageRecords, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CDamageDebugInfo() {}

	virtual void Print();

private:

	bool ValidateInput();
	void PrintDamageDebug();

	RegdConstPed m_Ped;
	bool m_OnlyDamageRecords;
};

#endif // AI_DEBUG_OUTPUT_ENABLED