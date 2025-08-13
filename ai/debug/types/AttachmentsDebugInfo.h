#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED
#include "scene\RegdRefTypes.h"

class CPhysical;

class CAttachmentsDebugInfo : public CAIDebugInfo
{
public:

	static const char* GetAttachStateString(s32 i);

	CAttachmentsDebugInfo(const CPhysical* pPhys, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CAttachmentsDebugInfo() {}

	virtual void Print();

private:

	bool ValidateInput();
	void PrintAttachments();

	RegdConstPhy m_Phys;
};

#endif // AI_DEBUG_OUTPUT_ENABLED