//
// WaterSPU.cpp	- main body of SPU job for dynamic water update;
//
//	14/03/2007	-	Andrzej:	- initial;
//
//
//
//
//
#if __SPU

#include "math/intrinsics.h"
#include "vector/vector3_consts_spu.cpp"
#include "vector/vector3.h"
#include "vector/color32.h"
#include "vector/matrix34.h"
#include <string.h>
//#include <spu_printf.h>
#include <math.h>

// SPU helper headers:
#include "../basetypes.h"
#include "fwtl/StepAllocator.h"

#include "fwmaths/RandomSPU.h"	// fwRandom()...
#include "math/random.cpp"

#include "../Renderer/WaterSPU.h"

static spuWaterParamsStruct *inWaterParams;
static spuWaterParamsStruct *outWaterParams;

#include "../Renderer/WaterUpdateDynamicCommon.h"	// include shared methods and data

//
//
//
//
void spuWaterProcess(::rage::sysTaskParameters &taskParams)
{
	fwStepAllocator scratchAllocator(taskParams.Scratch.Data, taskParams.Scratch.Size);

	for(u32 i=0; i<DYNAMIC_WATER_HEIGHT_NUM_ROWS; i++)
	{
		m_DynamicWater_Height[i] = (float*)scratchAllocator.Alloc(DYNAMIC_WATER_HEIGHT_SIZE, DYNAMIC_WATER_HEIGHT_ALIGN);
	}
	

#if !DMA_HEIGHT_BUFFER_WITH_JOB
	for(u32 i=0; i<DYNAMIC_WATER_DHEIGHT_NUM_ROWS; i++)
	{
		m_DynamicWater_dHeight[i] = (float*)scratchAllocator.Alloc(DYNAMIC_WATER_DHEIGHT_SIZE, DYNAMIC_WATER_DHEIGHT_ALIGN);
	}
#endif

	for(u32 i=0; i<WAVE_BUFFER_NUM_ROWS; i++)
	{
		m_WaveBuffer[i] = (u8*)scratchAllocator.Alloc(WAVE_BUFFER_SIZE, WAVE_BUFFER_ALIGN);
	}

	for(u32 i=0; i<WAVE_NOISE_BUFFER_NUM_ROWS; i++)
	{
		m_WaveNoiseBuffer[i] = (u8*)scratchAllocator.Alloc(WAVE_NOISE_BUFFER_SIZE, WAVE_NOISE_BUFFER_ALIGN);
	}


	fwRandom::Initialise();

#if DMA_HEIGHT_BUFFER_WITH_JOB
	WaterBuffer0* waveBuffer = (WaterBuffer0*)taskParams.Input.Data;
	inWaterParams	= &waveBuffer->spuWaterParams;
	outWaterParams	= &waveBuffer->spuWaterParams;
#else
	inWaterParams	= (spuWaterParamsStruct*)taskParams.Input.Data;
	outWaterParams	= (spuWaterParamsStruct*)taskParams.Output.Data;
#endif

	m_nNumOfWaterQuads			= inWaterParams->m_nNumOfWaterQuads;
    m_nNumOfWaterCalmingQuads	= inWaterParams->m_nNumOfWaterCalmingQuads;
	m_nNumOfWaveQuads			= inWaterParams->m_nNumOfWaveQuads;
	m_WorldBaseX		= inWaterParams->m_WorldBaseX;
	m_WorldBaseY		= inWaterParams->m_WorldBaseY;
	m_GridBaseX			= inWaterParams->m_GridBaseX;
	m_GridBaseY			= inWaterParams->m_GridBaseY;

	// copy main data to LS:

#if DMA_HEIGHT_BUFFER_WITH_JOB
	// DynamicWater Height 
	sysDmaLargeGet(m_DynamicWater_Height[0], (u32)inWaterParams->m_DynamicWater_Height, inWaterParams->m_DynamicWater_Height_size, WATER_HEIGHT_DMA_TAG);

	for(u32 i=0; i<DYNAMIC_WATER_DHEIGHT_NUM_ROWS; i++)
	{
		m_DynamicWater_dHeight[i] = (float*)((u8*)waveBuffer + i * (DYNAMIC_WATER_DHEIGHT_SIZE));
	}	
#else
	// DynamicWater Height 
	sysDmaLargeGet(m_DynamicWater_Height[0], (u32)inWaterParams->m_DynamicWater_Height, inWaterParams->m_DynamicWater_Height_size, WATER_HEIGHT_DMA_TAG);

	// DynamicWater dHeight
	sysDmaLargeGet(m_DynamicWater_dHeight[0], (u32)inWaterParams->m_DynamicWater_dHeight, inWaterParams->m_DynamicWater_dHeight_size, WATER_HEIGHT_DMA_TAG);
#endif


	// Wave Noise Buffer
	sysDmaLargeGet(m_WaveNoiseBuffer[0], (u32)inWaterParams->m_WaveNoiseBuffer,	sizeof(u8)*DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS, WATER_NOISE_BUFFER_DMA_TAG);

	// Calming Quads
	sysDmaLargeGet(&m_aCalmingQuads, (u32)inWaterParams->m_aCalmingQuads, inWaterParams->m_aCalmingQuads_size, WATER_CALM_QUADS_DMA_TAG);

	// Wave Buffer
	sysDmaLargeGet(m_WaveBuffer[0],	(u32)inWaterParams->m_WaveBuffer, sizeof(u8)*DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS, WATER_WAVE_BUFFER_AND_QUADS_DMA_TAG);

	// Wave Quads
#if DMA_HEIGHT_BUFFER_WITH_JOB
	sysDmaLargeGet(&m_aWaveQuads, (u32)inWaterParams->m_aWaveQuads,	inWaterParams->m_aWaveQuads_size, WATER_WAVE_BUFFER_AND_QUADS_DMA_TAG);
#else
	sysDmaLargeGetAndWait(&m_aWaveQuads, (u32)inWaterParams->m_aWaveQuads,	inWaterParams->m_aWaveQuads_size, WATER_WAVE_BUFFER_AND_QUADS_DMA_TAG);
#endif
	if (inWaterParams->pad0)
	{
		__debugbreak();
	}

	UpdateDynamicWater(&inWaterParams->m_UpdateParams);
	
	#if DMA_HEIGHT_BUFFER_WITH_JOB
		outWaterParams->m_OutWorldBaseX	= m_WorldBaseX;
		outWaterParams->m_OutWorldBaseY	= m_WorldBaseY;
		outWaterParams->m_OutGridBaseX	= m_GridBaseX;
		outWaterParams->m_OutGridBaseY	= m_GridBaseY;
		// copy processed data back to main store:
		sysDmaLargePut(			m_DynamicWater_Height[0], (u32)inWaterParams->m_DynamicWater_HeightVRAM			,	inWaterParams->m_DynamicWater_Height_size,WATER_HEIGHT_DMA_TAG);
		sysDmaLargePutAndWait(	m_DynamicWater_Height[0], (u32)inWaterParams->m_UpdateParams.m_WaterHeightBuffer,	inWaterParams->m_DynamicWater_Height_size,WATER_HEIGHT_DMA_TAG);
	#else
		outWaterParams->m_WorldBaseX	= m_WorldBaseX;
		outWaterParams->m_WorldBaseY	= m_WorldBaseY;
		outWaterParams->m_GridBaseX		= m_GridBaseX;
		outWaterParams->m_GridBaseY		= m_GridBaseY;

		// copy processed data back to main store:
		sysDmaLargePut(			m_DynamicWater_Height[0], (u32)inWaterParams->m_DynamicWater_HeightVRAM			,	inWaterParams->m_DynamicWater_Height_size,WATER_HEIGHT_DMA_TAG);
		sysDmaLargePut(			m_DynamicWater_Height[0],	(u32)inWaterParams->m_UpdateParams.m_WaterHeightBuffer,		inWaterParams->m_DynamicWater_Height_size,	WATER_HEIGHT_DMA_TAG);
		sysDmaLargePutAndWait(	m_DynamicWater_dHeight[0],	(u32)inWaterParams->m_UpdateParams.m_WaterVelocityBuffer,	inWaterParams->m_DynamicWater_dHeight_size,	WATER_HEIGHT_DMA_TAG);
	#endif

}// end of spuWaterProcess()...



#endif //__SPU....

