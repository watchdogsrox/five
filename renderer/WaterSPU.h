//
// WaterSPU.h	- main body of SPU job for dynamic water update;
//
//	14/03/2007	-	Andrzej:	- initial;
//
//
// 
#ifndef __WATERSPU_H__
#define __WATERSPU_H__

// Tasks should only depend on taskheader.h, not task.h (which is the dispatching system)
#include "system/taskheader.h"
#include "renderer/water.h"
DECLARE_TASK_INTERFACE(WaterSPU);
void spuWaterProcess( ::rage::sysTaskParameters & );

//
//
//
//

struct spuWaterParamsStruct
{
	float*	m_DynamicWater_Height;
	float*	m_DynamicWater_HeightVRAM;

	s32		m_DynamicWater_Height_size;

#if !DMA_HEIGHT_BUFFER_WITH_JOB
	float*	m_DynamicWater_dHeight;
	s32		m_DynamicWater_dHeight_size;
#endif

	u8*		m_WaveNoiseBuffer;
	u8*		m_WaveBuffer;

	void*	m_aCalmingQuads;
	s32		m_aCalmingQuads_size;

	void*	m_aWaveQuads;
	s32		m_aWaveQuads_size;

	// stuff required in UpdateDynamicWater():
	CWaterUpdateDynamicParams	m_UpdateParams;

	s32		m_nNumOfWaterQuads;
	s32		m_nNumOfWaterCalmingQuads;
	s32		m_nNumOfWaveQuads;

	s32		m_GridBaseX;
	s32		m_GridBaseY;
	s32		m_WorldBaseX;
	s32		m_WorldBaseY;

	s32		m_WORLD_BORDERXMIN;
	s32		m_WORLD_BORDERXMAX;
	s32		m_WORLD_BORDERYMIN;
	s32		m_WORLD_BORDERYMAX;

	u32	    pad0;

#if DMA_HEIGHT_BUFFER_WITH_JOB
	s32		m_OutGridBaseX;
	s32		m_OutGridBaseY;
	s32		m_OutWorldBaseX;
	s32		m_OutWorldBaseY;
#endif
};

#if DMA_HEIGHT_BUFFER_WITH_JOB
struct WaterBuffer0 
{ 
	float data1[DYNAMICGRIDELEMENTS][DYNAMICGRIDELEMENTS];  
	spuWaterParamsStruct spuWaterParams;
};
#endif

#endif //__WATERSPU_H__...

