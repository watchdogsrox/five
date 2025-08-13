#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

#include "scene\RegdRefTypes.h"

class CScriptHistoryDebugInfo : public CAIDebugInfo
{
public:

	CScriptHistoryDebugInfo(const CPed* pPed, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams());
	virtual ~CScriptHistoryDebugInfo() {}

	virtual void Print();

private:

	bool ValidateInput();
	void PrintScriptHistory();

	RegdConstPed m_Ped;
};

#endif // AI_DEBUG_OUTPUT_ENABLED