#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED
#include "scene\RegdRefTypes.h"

class CMovementTextDebugInfo : public CAIDebugInfo
{
public:

	CMovementTextDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CMovementTextDebugInfo() {}

	virtual void Print();
	virtual void Visualise();

private:

	bool ValidateInput();
	void PrintMovementDebug();

	RegdConstPed m_Ped;
};

#endif // AI_DEBUG_OUTPUT_ENABLED