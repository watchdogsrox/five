#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "scene\RegdRefTypes.h"

class CWantedDebugInfo : public CAIDebugInfo
{
public:

	CWantedDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CWantedDebugInfo() {}

	virtual void Print();
	virtual void Visualise();

private:

	bool ValidateInput();
	void PrintWanted();

	RegdConstPed m_Ped;
};

#endif // AI_DEBUG_OUTPUT_ENABLED