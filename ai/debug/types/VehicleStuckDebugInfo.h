#pragma once
// game headers
#include "ai\debug\types\VehicleDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

class CVehicle;

class CVehicleStuckDebugInfo : public CVehicleDebugInfo
{
public:
	CVehicleStuckDebugInfo(const CVehicle* pVeh = 0, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams(), s32 iNumberOfLines = 0);
	virtual ~CVehicleStuckDebugInfo() {};

	virtual void PrintRenderedSelection(bool bPrintAll = false);
};


#endif // AI_DEBUG_OUTPUT_ENABLED