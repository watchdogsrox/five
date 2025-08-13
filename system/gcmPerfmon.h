#ifndef GCMPERFMON_H__
#define GCMPERFMON_H__

#define USE_GCMPERFMON 0

#if (1 == __PPU) && (1 == USE_GCMPERFMON)

#include <cell/gcm_pm.h>
#include "grcore/wrapper_gcm.h"

// Timer Definition 
extern uint32_t pm_Events_IDs[CELL_GCM_PM_DOMAIN_NUM][4];

// Timer Result
extern u32 pm_Cycles[CELL_GCM_PM_DOMAIN_NUM];
extern u32 pm_Events[CELL_GCM_PM_DOMAIN_NUM][4];

extern void pm_Init();

inline void pm_Exit() 
{
	// We're done..
	cellGcmExitPerfMon();
	
	// No String kill... I rely on the static destructor to do his job
}

inline void pm_EventSet(uint32_t domainID, 
						uint32_t event0, 
						uint32_t event1, 
						uint32_t event2, 
						uint32_t event3)
{
	pm_Events_IDs[domainID][0] = event0;
	pm_Events_IDs[domainID][1] = event1;
	pm_Events_IDs[domainID][2] = event2;
	pm_Events_IDs[domainID][3] = event3;
}

inline void pm_EventSetup()
{
	// Tell gcm_pm what we'll be recording, for each domain
	cellGcmSetPerfMonEvent( CELL_GCM_PM_DOMAIN_GCLK, pm_Events_IDs[CELL_GCM_PM_DOMAIN_GCLK] );
	cellGcmSetPerfMonEvent( CELL_GCM_PM_DOMAIN_SCLK, pm_Events_IDs[CELL_GCM_PM_DOMAIN_SCLK] );
	cellGcmSetPerfMonEvent( CELL_GCM_PM_DOMAIN_RCLK, pm_Events_IDs[CELL_GCM_PM_DOMAIN_RCLK] );
	cellGcmSetPerfMonEvent( CELL_GCM_PM_DOMAIN_MCLK, pm_Events_IDs[CELL_GCM_PM_DOMAIN_MCLK] );
	cellGcmSetPerfMonEvent( CELL_GCM_PM_DOMAIN_ICLK, pm_Events_IDs[CELL_GCM_PM_DOMAIN_ICLK] );
}

inline void pm_Start()
{
	// Trigger will store the result internally and reset the timers,
	// So we trigger at start
	cellGcmSetPerfMonTrigger(GCM_CONTEXT);
}

inline void pm_End()
{
	// Trigger will store the result internally and reset the timers,
	// And we trigger at end
	cellGcmSetPerfMonTrigger(GCM_CONTEXT);
}

inline void pm_Get()
{
	// Gather the actual datam for display or dump
	cellGcmGetPerfMonCounter(CELL_GCM_PM_DOMAIN_GCLK,pm_Events[CELL_GCM_PM_DOMAIN_GCLK],&pm_Cycles[CELL_GCM_PM_DOMAIN_GCLK]);
	cellGcmGetPerfMonCounter(CELL_GCM_PM_DOMAIN_SCLK,pm_Events[CELL_GCM_PM_DOMAIN_SCLK],&pm_Cycles[CELL_GCM_PM_DOMAIN_SCLK]);
	cellGcmGetPerfMonCounter(CELL_GCM_PM_DOMAIN_RCLK,pm_Events[CELL_GCM_PM_DOMAIN_RCLK],&pm_Cycles[CELL_GCM_PM_DOMAIN_RCLK]);
	cellGcmGetPerfMonCounter(CELL_GCM_PM_DOMAIN_MCLK,pm_Events[CELL_GCM_PM_DOMAIN_MCLK],&pm_Cycles[CELL_GCM_PM_DOMAIN_MCLK]);
	cellGcmGetPerfMonCounter(CELL_GCM_PM_DOMAIN_ICLK,pm_Events[CELL_GCM_PM_DOMAIN_ICLK],&pm_Cycles[CELL_GCM_PM_DOMAIN_ICLK]);
}

extern void pm_Display(int startX = 80, int startY = 10);

#else  //  (1 == __PPU) && (1 == USE_GCMPERFMON)

#define pm_Init() /* NoOp*/ 
#define pm_Exit() /* NoOp*/
#define pm_EventSet(a, b, c, d, e)
#define pm_EventSetup() /* NoOp*/
#define pm_Start() /* NoOp*/
#define pm_End() /* NoOp*/
#define pm_Get() /* NoOp*/
#define pm_Display(...) /* NoOp */

#endif //  (1 == __PPU) && (1 == USE_GCMPERFMON)

#endif // GCMPERFMON_H__
