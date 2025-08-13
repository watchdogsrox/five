#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "scene\RegdRefTypes.h"

class CCombatTextDebugInfo : public CAIDebugInfo
{
public:

	CCombatTextDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CCombatTextDebugInfo() {}

	virtual void Print();

private:

	bool ValidateInput();
	void PrintCombatText();

	RegdConstPed m_Ped;
};

#endif // AI_DEBUG_OUTPUT_ENABLED