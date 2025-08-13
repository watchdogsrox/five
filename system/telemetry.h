#ifndef INC_TELEMETRY_H_
#define INC_TELEMETRY_H_

#include "bank/bank.h"
#include "profile/telemetry.h"

#if USE_TELEMETRY

class CTelemetry
{
public:
	static void Init();

#if __BANK
	static void InitWidgets();
	static void AddWidgets(bkBank& bank);
#endif

	static void Update();

private:
	static void PlotCPUTimings();
	static void PlotMemoryBuckets();
	static void AddRenderSummary(CBlobBuffer & buffer);

	static bool sm_PlotCPU;
	static bool sm_PlotMemory;
	static bool sm_PlotEKG;
	static bool sm_RenderSummary;
};

#endif

#endif