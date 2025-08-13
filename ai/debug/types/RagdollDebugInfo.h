#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "scene\RegdRefTypes.h"

class CRagdollDebugInfo : public CAIDebugInfo
{
public:

	CRagdollDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CRagdollDebugInfo() {}

	virtual void Print();

private:

	bool ValidateInput();

	void PrintRagdollText();
	void PrintRagdollFlags();

	RegdConstPed m_Ped;
};

#endif // AI_DEBUG_OUTPUT_ENABLED