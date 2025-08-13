#pragma once

// game headers
#include "ai\debug\types\VehicleDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

class CVehicle;
class CTask;

class CVehicleTaskDebugInfo : public CVehicleDebugInfo
{
public:
	CVehicleTaskDebugInfo(const CVehicle* pVeh = 0, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams(), s32 iNumberOfLines = 0);
	virtual ~CVehicleTaskDebugInfo() {};

	virtual void PrintRenderedSelection(bool bPrintAll = false);
	virtual void Visualise();

private:
	void PrintTaskTree(bool bPrintAll);
	void PrintTaskHistory(bool bPrintAll);
	void PrintTask(CTask* pTask, bool bPrintAll);
};
 
#endif // AI_DEBUG_OUTPUT_ENABLED