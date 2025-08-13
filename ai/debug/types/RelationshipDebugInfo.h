#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"
#include "scene\RegdRefTypes.h"

#if AI_DEBUG_OUTPUT_ENABLED

class CRelationshipDebugInfo : public CAIDebugInfo
{
public:

	CRelationshipDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CRelationshipDebugInfo() {}

	virtual void Print();

private:

	bool ValidateInput();
	void PrintRelationships();

	RegdConstPed m_Ped;
};

#endif // AI_DEBUG_OUTPUT_ENABLED