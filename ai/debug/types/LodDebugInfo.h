#pragma once

// game headers
#include "ai\debug\system\AIDebugInfo.h"

#if AI_DEBUG_OUTPUT_ENABLED

class CLodDebugInfo : public CAIDebugInfo
{
public:

	CLodDebugInfo(const DebugInfoPrintParams printParams) : CAIDebugInfo(printParams) {}
	virtual ~CLodDebugInfo() {}

	virtual void Print() {};

private:

	bool ValidateInput() { return true; }

};

#endif // AI_DEBUG_OUTPUT_ENABLED