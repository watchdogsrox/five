//
//	LightClassificationSPU job:
//
//	2011/07/28	- Andrzej:	initial;
//
//
//
//
#if __SPU
#define DEBUG_SPU_JOB_NAME "spupmLightClassificationSPU"

#define AM_SPU_DEBUG				(1)
#define SPU_DEBUGF(X)				X
//#define SPU_DEBUGF(x)
#define SPU_DEBUGF1(X)				X

#define SUB_TILE_SIZE				16

#undef	__STRICT_ALIGNED			// new.h compile warnings
#define ATL_INMAP_H					// inmap.h compile warnings

#define V2SAMESIGNALL__4			// v2vector4.inl compile warnings
#define V3RESULTTOINDEXZYX__4		// v3vector4.inl compile warnings
#define V3RESULTTOINDEXXYZ__4		// v3vector4.inl compile warnings
#define V3SAMESIGNALL__4			// v3vector4.inl compile warnings
#define V4SAMESIGNV					// v4vector4.inl compile warnings
#define V4SAMESIGNALL				// v4vector4.inl compile warnings

#define SYS_DMA_VALIDATION			(1)	// 1 -	General checks on valid alignment, size and DMA source and  destination addresses for all DMA commands. 
#include "system/dma.h"
#include "system/memops.h"			// bits/memset.h & bits/memcpy.h compile warnings
#include <cell/atomic.h>
#include <spu_printf.h>
#include "math/float16.h"
#include "fwmaths/vectorutil.h"

#include "scene\2dEffect.h"
#include "renderer\Lights\LightCommon.h"
#include "renderer\Lights\LightSource.h"
#include "renderer\Lights\TiledLightingSettings.h"

#define CSPULIGHTCLASSIFICATIONINFO_DEF_STRUCT
#include "renderer\SpuPM\SpuPmMgr.h"
#include "renderer\SpuPM\SpuPmMgrSPU.h"


CSPULightClassificationInfo* g_lcInfo=0;

static u8*					        sm_classificationTexture = 0;
static float*				        sm_subTileData = 0;
static float*				        sm_tileData = 0;

static atFixedBitSet<MAX_STORED_LIGHTS>* sm_subTileActiveLights = 0;
static atFixedBitSet<MAX_STORED_LIGHTS>* sm_tileActiveLights = 0;

static LightStats*					sm_lightStats=0;

#define START_TIMER(index)
#define STOP_TIMER(index)
u32 spuScratchAddr=0;

#include "..\Lights\LightClassification.inl"

CSPUPostProcessDriverInfo* LightClassificationSPU_cr0(char *_end, void *infoPacketSource, unsigned int infoPacketSize)
{	
	// Start SPU memory allocator.  Align to info packet alignment to get best DMA performance.
	spuScratchAddr = matchAlignment128((u32)_end, infoPacketSource);
	// Get the job packet
	u32 alignedSize = AlignSize16(infoPacketSize);
	CSPUPostProcessDriverInfo *infoPacket = (CSPUPostProcessDriverInfo*)spuScratchAddr;
	sysDmaGet(infoPacket, (u64)infoPacketSource, alignedSize, 0);
	spuScratchAddr = stepLSAddr(spuScratchAddr, alignedSize);
	sysDmaWaitTagStatusAll(1<<0);

	// Grab userdata packet (if any):
	if(infoPacket->m_userDataEA)
	{
		alignedSize = AlignSize16(infoPacket->m_userDataSize);
		g_lcInfo = (CSPULightClassificationInfo*)spuScratchAddr;
		sysDmaLargeGet(g_lcInfo, (u64)infoPacket->m_userDataEA, alignedSize, 0);
		spuScratchAddr = stepLSAddr(spuScratchAddr, alignedSize);
		sysDmaWaitTagStatusAll(1<<0);
	}

	const u32 yStart = infoPacket->m_sourceY * SUB_TILE_SIZE;
	const u32 yEnd = (infoPacket->m_sourceY + infoPacket->m_sourceHeight) * SUB_TILE_SIZE;

	const u32 tileYStart = yStart / TILE_SIZE;
	const u32 subTileYStart = yStart / SUB_TILE_SIZE;
	const u32 numSubTilesY = (u32)ceilf(float(yEnd - yStart) / SUB_TILE_SIZE);
	const u32 numTilesY = (u32)ceilf(float(yEnd - yStart) / TILE_SIZE);

	const u32 totalSubTiles = numSubTilesY * NUM_SUB_TILES_X;
	const u32 totalTiles = numTilesY * NUM_TILES_X;

	const u32 classificationTexture_size = (totalSubTiles * sizeof(Float16) * NUM_TILE_COMPONENTS);
	spuScratchAddr = Align(spuScratchAddr, 128);
	sm_classificationTexture = (u8*)spuScratchAddr;
	spuScratchAddr = stepLSAddr(spuScratchAddr, classificationTexture_size);
	Assert(sm_classificationTexture);

	u8 *srcData = (u8*)infoPacket->m_sourceData;
	sysDmaGet(sm_classificationTexture, (u64)(srcData), classificationTexture_size, 0);
	sysDmaWaitTagStatusAll(1<<0);

	// allocate depth min/max buffer for 64x64 and 16x16 tiles
	const u32 sm_tileData_size = (totalTiles * sizeof(float) * NUM_TILE_COMPONENTS);
	spuScratchAddr = Align(spuScratchAddr, 128);
	sm_tileData = (float*)spuScratchAddr;
	spuScratchAddr = stepLSAddr(spuScratchAddr, sm_tileData_size);

	const u32 sm_subTileData_size = (totalSubTiles * sizeof(float) * NUM_TILE_COMPONENTS);
	spuScratchAddr = Align(spuScratchAddr, 128);
	sm_subTileData = (float*)spuScratchAddr;
	spuScratchAddr = stepLSAddr(spuScratchAddr, sm_subTileData_size);

	const u32 sm_lightStats_size = sizeof(LightStats)*MAX_STORED_LIGHTS;
	spuScratchAddr = Align(spuScratchAddr, 128);
	sm_lightStats = (LightStats*)spuScratchAddr;
	spuScratchAddr = stepLSAddr(spuScratchAddr, sm_lightStats_size);

	// TODO: improve this crap
	sysMemSet(sm_lightStats, 0x00, sm_lightStats_size);

	// Unpack
	DepthDownsampleUnpackSPU((Vec::Vector_4V*)sm_subTileData, (Vec::Vector_4V*)sm_classificationTexture, totalSubTiles);
	
	// Down-sample 16x16 tile pixels to 64x64 tile pixels
	for (u32 y = 0; y < numTilesY; ++y)
	{
		for (u32 x = 0; x < NUM_TILES_X; ++x)
		{
			const u32 tileIndex = y * NUM_TILES_X + x;
			float depthMin = FLT_MAX;
			float depthMax = -FLT_MAX;
			float specular = 0.0f;
			float interior = 0.0f;

			u32 cornerSubTileX = (x * TILE_SIZE) / SUB_TILE_SIZE;
			u32 cornerSubTileY = (y * TILE_SIZE) / SUB_TILE_SIZE;

			for (u32 sy = 0; sy < SUB_TILE_TO_TILE_OFFSET; ++sy)
			{
				for (u32 sx = 0; sx < SUB_TILE_TO_TILE_OFFSET; ++sx)
				{
					const u32 subTileX = Min(cornerSubTileX + sx, u32(NUM_SUB_TILES_X));
					const u32 subTileY = Min(cornerSubTileY + sy, u32(NUM_SUB_TILES_Y));

					const u32 subTileIndex = subTileY * NUM_SUB_TILES_X + subTileX;

					const float subTileDepthMin = sm_subTileData[subTileIndex * NUM_TILE_COMPONENTS + TILE_DEPTH_MIN];
					const float subTileDepthMax = sm_subTileData[subTileIndex * NUM_TILE_COMPONENTS + TILE_DEPTH_MAX];
					
					depthMin = Min(depthMin, subTileDepthMin);
					depthMax = Max(depthMax, subTileDepthMax);

					// Get specular 
					const float subTileSpecular = sm_subTileData[subTileIndex * NUM_TILE_COMPONENTS + TILE_SPECULAR];
					const float subTileInterior = sm_subTileData[subTileIndex * NUM_TILE_COMPONENTS + TILE_INTERIOR];

					specular += subTileSpecular;
					interior += subTileInterior;
				}
			}

			sm_tileData[tileIndex * NUM_TILE_COMPONENTS + TILE_DEPTH_MIN] = depthMin;
			sm_tileData[tileIndex * NUM_TILE_COMPONENTS + TILE_DEPTH_MAX] = depthMax;
			
			sm_tileData[tileIndex * NUM_TILE_COMPONENTS + TILE_SPECULAR] = specular / 16.0f;
			sm_tileData[tileIndex * NUM_TILE_COMPONENTS + TILE_INTERIOR] = interior / 16.0f;
		}
	}

	// Generate a min/max depth buffer for 64x64 tiles and 16x16 tiles
	const u32 sm_tileActiveLights_size = (totalTiles * sizeof(atFixedBitSet<MAX_STORED_LIGHTS>));
	spuScratchAddr = Align(spuScratchAddr, 128);
	sm_tileActiveLights = (atFixedBitSet<MAX_STORED_LIGHTS>*)spuScratchAddr;
	spuScratchAddr = stepLSAddr(spuScratchAddr, sm_tileActiveLights_size);

	const u32 sm_subTileActiveLights_size = (totalSubTiles * sizeof(atFixedBitSet<MAX_STORED_LIGHTS>));
	spuScratchAddr = Align(spuScratchAddr, 128);
	sm_subTileActiveLights = (atFixedBitSet<MAX_STORED_LIGHTS>*)spuScratchAddr;
	spuScratchAddr = stepLSAddr(spuScratchAddr, sm_subTileActiveLights_size);

	// Classify lights
	ClassifyLightsSPU(
		0, NUM_TILES_X,
		0, numTilesY,
		0, tileYStart,
		totalTiles, totalSubTiles,
		sm_tileActiveLights,
		sm_tileData,
		sm_subTileActiveLights,
		sm_subTileData,
		g_lcInfo);

	// send downsample and active light results back (for both the sub tiles and tiles)
	const u32 offset1 = (subTileYStart * NUM_SUB_TILES_X) * NUM_TILE_COMPONENTS;
	sysDmaLargePut((void*)sm_subTileData, (u64)(&g_lcInfo->m_subTileData[offset1]), sm_subTileData_size, 0);

	const u32 offset2 = (tileYStart * NUM_TILES_X) * NUM_TILE_COMPONENTS;
	sysDmaPut((void*)sm_tileData, (u64)(&g_lcInfo->m_tileData[offset2]), sm_tileData_size, 0);

	const u32 offset3 = (subTileYStart * NUM_SUB_TILES_X);
	sysDmaLargePut((void*)sm_subTileActiveLights, (u64)(&g_lcInfo->m_subTileActiveLights[offset3]), sm_subTileActiveLights_size, 0);

	const u32 offset4 = (tileYStart * NUM_TILES_X);
	sysDmaPut((void*)sm_tileActiveLights, (u64)(&g_lcInfo->m_tileActiveLights[offset4]), sm_tileActiveLights_size, 0);


	// note: update syncOffset if you happen to change split[] in CTiledLighting::DepthDownsample()
	const u32 jobIndex = (infoPacket->m_sourceY/12) & (0x3);	// allowed range:  0-3


	// last classification job?
	if(jobIndex==3)
	{
		// wait for current timestamp from 3 other sister jobs
		#define RSX_SPIN_WAIT		for(u32 __i=0; __i<2400; ++__i) spu_readch(SPU_RdDec)

		const u64 expectedTimeStamp64 = (u64(g_lcInfo->m_timeStamp)<<32) | u64(g_lcInfo->m_timeStamp);
		const u32 expectedTimeStamp32 = u32(g_lcInfo->m_timeStamp);
		u64 currentTs01 = cellAtomicNop64((u64*)buffer128, (u64)&g_lcInfo->m_timeStampDesc[0]);
		u32 currentTs2x = cellAtomicNop32((u32*)buffer128, (u64)&g_lcInfo->m_timeStampDesc[2]);

		u32 tryWait=200;	// only 200 tries
		while((tryWait--) && ((currentTs01!=expectedTimeStamp64) || (currentTs2x!=expectedTimeStamp32)))
		{
			//spu_printf("\n timeStamp1: current=(%d,%d,%d,x), expected=%d", u32(currentTs01>>32), u32(currentTs01), currentTs2x, u32(expectedTimeStamp32));
			RSX_SPIN_WAIT;
			currentTs01 = cellAtomicNop64((u64*)buffer128, (u64)&g_lcInfo->m_timeStampDesc[0]);
			currentTs2x = cellAtomicNop32((u32*)buffer128, (u64)&g_lcInfo->m_timeStampDesc[2]);
		}
		Assertf(tryWait!=0, "ClassificationSPU: Wait for other jobs failed!");


		// 64x64 classification info:
		const u32 tileActiveLightsAll_size = NUM_TILES_X * NUM_TILES_Y * sizeof(atFixedBitSet<MAX_STORED_LIGHTS>);	//7.5KB
		spuScratchAddr = Align(spuScratchAddr, 128);
		atFixedBitSet<MAX_STORED_LIGHTS>* tileActiveLightsAll = (atFixedBitSet<MAX_STORED_LIGHTS>*)spuScratchAddr;
		spuScratchAddr = stepLSAddr(spuScratchAddr, tileActiveLightsAll_size);

		// grab classification info from 3 other jobs at the beginning of the array:
		sysDmaGet(tileActiveLightsAll, (u64)(&g_lcInfo->m_tileActiveLights[0]), tileActiveLightsAll_size-sm_tileActiveLights_size, 1);
		// ...and refill rest of the array with the info from this job:
		sysMemCpy(&tileActiveLightsAll[tileYStart * NUM_TILES_X], sm_tileActiveLights, sm_tileActiveLights_size);

		// grab light stats from 3 other sister jobs:
		LightStats *sm_otherLightStats012 = AllocaAligned(LightStats, 3*MAX_STORED_LIGHTS, 128);	// 3KB
		Assert(sm_otherLightStats012);

		LightStats *sm_otherLightStat0 = (LightStats*)( ((u8*)sm_otherLightStats012)+0*sm_lightStats_size );
		LightStats *sm_otherLightStat1 = (LightStats*)( ((u8*)sm_otherLightStats012)+1*sm_lightStats_size );
		LightStats *sm_otherLightStat2 = (LightStats*)( ((u8*)sm_otherLightStats012)+2*sm_lightStats_size );

		sysDmaGet(sm_otherLightStat0, (u64)g_lcInfo->m_lightStatsDesc[0], sm_lightStats_size, 1);
		sysDmaGet(sm_otherLightStat1, (u64)g_lcInfo->m_lightStatsDesc[1], sm_lightStats_size, 1);
		sysDmaGet(sm_otherLightStat2, (u64)g_lcInfo->m_lightStatsDesc[2], sm_lightStats_size, 1);

		sysDmaWaitTagStatusAll(1<<1);


		// accumulate 4 stats into 1:
		u32 totalCoverage64x64 = 0;
		u32 totalCoverage16x16 = 0;
		for(u32 s=0; s<MAX_STORED_LIGHTS; s++)
		{
			sm_lightStats[s].m_numTiles64x64 +=		sm_otherLightStat0[s].m_numTiles64x64 +
													sm_otherLightStat1[s].m_numTiles64x64 +
													sm_otherLightStat2[s].m_numTiles64x64;
			sm_lightStats[s].m_numTiles16x16 +=		sm_otherLightStat0[s].m_numTiles16x16 +
													sm_otherLightStat1[s].m_numTiles16x16 +
													sm_otherLightStat2[s].m_numTiles16x16;
		
			totalCoverage64x64 += sm_lightStats[s].m_numTiles64x64;
			totalCoverage16x16 += sm_lightStats[s].m_numTiles16x16;
		}
		

		// Load balancing of rendering: SPU or RSX:
const u32 SPU_16x16_TILES_MAX_PER_LIGHT		= g_lcInfo->m_lightBalancerMaxLightTileLimit;	//180*16;	// light covering too much screen for SPU
const u32 SPU_16x16_TILES_MAX				= g_lcInfo->m_lightBalancerTileLimit;			// no more than this amount of tiles for SPU
		if(1)
		{
			atFixedBitSet<MAX_STORED_LIGHTS> disabledLights;
			disabledLights.Reset();

		u32 numTotalTiles16x16=0;
			// go over lights, drop big lights on RSX:
			for(u32 lightIdx=0; lightIdx<MAX_STORED_LIGHTS; lightIdx++)
			{
				if(sm_lightStats[lightIdx].m_numTiles64x64)
				{
					const u32 numTiles16x16 = sm_lightStats[lightIdx].m_numTiles16x16;
					
					
					if( ((numTotalTiles16x16 + numTiles16x16) < SPU_16x16_TILES_MAX) && (numTiles16x16 < SPU_16x16_TILES_MAX_PER_LIGHT) )
					{
						// render on SPU
						numTotalTiles16x16 += numTiles16x16;
					}
					else
					{
						// too much workload - switch this light over to RSX:
						// 1: disable 64x64 classification bit:
						disabledLights.Set( lightIdx );
						// 2: patch RSX cmdbuf to render this light:
						sysDmaPutUInt32(0x00000000, (u64)(&g_lcInfo->m_lightBalancerTab[ lightIdx*SPUPM_LIGHTBALANCER_CMD_BUF_SIZE ]), 1);
						
						totalCoverage64x64 -= sm_lightStats[lightIdx].m_numTiles64x64;
						totalCoverage16x16 -= sm_lightStats[lightIdx].m_numTiles16x16;
					}
				}
			}// for(u32 lightIdx=0; lightIdx<MAX_STORED_LIGHTS; lightIdx++)...

			for(u32 c=0; c<NUM_TILES_X*NUM_TILES_Y; c++)
			{
				tileActiveLightsAll[c].IntersectNegate( disabledLights );
			}
		}// Load balancing...

		sysDmaWaitTagStatusAll(1<<1);	// wait for ealier patches
		// clear global syncing JTS on RSX:
		sysDmaPutUInt32(0x00000000, (u64)(&g_lcInfo->m_lightBalancerTab[MAX_STORED_LIGHTS*SPUPM_LIGHTBALANCER_CMD_BUF_SIZE]), 1);
//		spu_printf("\n%d: LightClassificationSPU: globalJTS cleaned (table=%x).", (u32)g_lcInfo->m_timeStamp, (u32)g_lcInfo->m_lightBalancerTab);

		// accumulate all 64x64 processed to find number of lights on SPU:
		u32 totalSpuLights=0;
		{
			atFixedBitSet<MAX_STORED_LIGHTS> allLightsOr;
			allLightsOr = tileActiveLightsAll[0];
			for(u32 c=1; c<NUM_TILES_X*NUM_TILES_Y; c++)
			{
				allLightsOr.Union( tileActiveLightsAll[c] );
			}
			totalSpuLights = allLightsOr.CountBits();
		}


		sysDmaWaitTagStatusAll(1<<0);	// wait for *all* earlier stores

		// return updated 64x64 classification info:
		sysDmaPut(tileActiveLightsAll, (u64)(&g_lcInfo->m_tileActiveLights[0]), tileActiveLightsAll_size, 1);
		// return accumulated stats info at index #0:
		sysDmaPut((void*)sm_lightStats, (u64)g_lcInfo->m_lightStatsDesc[0], sm_lightStats_size, 1);

		const u64 totalCoverage = u64(totalSpuLights&0xffff)<<48 | u64(totalCoverage64x64&0xffff)<<32 | u64(totalCoverage16x16);
		cellAtomicStore64((u64*)buffer128, (u64)g_lcInfo->m_lightStatsCoverage, totalCoverage);

		sysDmaWaitTagStatusAll(1<<1);
	}
	else
	{
		sysDmaPut((void*)sm_lightStats, (u64)g_lcInfo->m_lightStatsDesc[jobIndex], sm_lightStats_size, 0);
		sysDmaWaitTagStatusAll(1<<0);
	}


	// final synchronisation stamp:
//	cellAtomicStore32((u32*)buffer128, (u64)&g_lcInfo->m_timeStampDesc[jobIndex], (u32)g_lcInfo->m_timeStamp);

	return(infoPacket);
}// end of LightClassificationSPU_cr0()...

#endif //__SPU....

