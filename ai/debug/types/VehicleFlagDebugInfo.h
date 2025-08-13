#pragma once
// game headers
#include "ai\debug\types\VehicleDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

class CVehicle;

class CVehicleFlagDebugInfo : public CVehicleDebugInfo
{
public:
	CVehicleFlagDebugInfo(const CVehicle* pVeh = 0, DebugInfoPrintParams rPrintParams = DebugInfoPrintParams(), s32 iNumberOfLines = 0);
	virtual ~CVehicleFlagDebugInfo() {};

	virtual void PrintRenderedSelection(bool bPrintAll = false);

private:
	void DisplayFlagIfSet(u8 bFlagValue, const char* szFlagName);
};

#endif // AI_DEBUG_OUTPUT_ENABLED