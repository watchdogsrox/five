#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "scene\RegdRefTypes.h"

class CTargetingDebugInfo : public CAIDebugInfo
{
public:

	CTargetingDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CTargetingDebugInfo() {}

	virtual void Print();
	virtual void Visualise();

private:

	bool ValidateInput();
	void PrintTargeting();

	RegdConstPed m_Ped;
};

#endif // AI_DEBUG_OUTPUT_ENABLED