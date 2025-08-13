#pragma once
// game headers
#include "ai\debug\types\VehicleDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

class CVehicle;

class CVehicleNodeListDebugInfo : public CVehicleDebugInfo
{
public:
	CVehicleNodeListDebugInfo(const CVehicle* pVeh = 0, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams(), s32 iNumberOfLines = 0);
	virtual ~CVehicleNodeListDebugInfo() {};

	virtual void PrintRenderedSelection(bool bPrintAll = false);
	virtual void Visualise();

private:
	void PrintFollowRoute(bool bPrintAll);
	void PrintFutureNodes(bool bPrintAll);
	void PrintSearchInfo();
};

#endif // AI_DEBUG_OUTPUT_ENABLED