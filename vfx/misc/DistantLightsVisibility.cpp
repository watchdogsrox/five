// DistantLightsVisibility
// athomas: 04/03/2013

#include "DistantLightsVisibility.h"

#include "system/interlocked.h"
#include "../../renderer/occlusionAsync.h"

#define PUT_DMA_TAG		(1)

bank_float DistantLightsVisibilityData::ms_BlockMaxVisibleDistance = 2500.0f;

///////////////////////////////////////////////////////////////////////////////
//  FindBlockMin
///////////////////////////////////////////////////////////////////////////////
void DistantLightsVisibility::FindBlockMin(s32 blockX, s32 blockY, float& minX, float& minY)
{
	Assertf(blockX>=0 && blockX<DISTANTLIGHTS_CURRENT_MAX_BLOCKS, "DistantLightsVisibility::FindBlock - blockX out of range");
	Assertf(blockY>=0 && blockY<DISTANTLIGHTS_CURRENT_MAX_BLOCKS, "DistantLightsVisibility::FindBlock - blockX out of range");

	float blockWidthX = (WORLDLIMITS_REP_XMAX-WORLDLIMITS_REP_XMIN) / DISTANTLIGHTS_CURRENT_MAX_BLOCKS;
	float blockWidthY = (WORLDLIMITS_REP_YMAX-WORLDLIMITS_REP_YMIN) / DISTANTLIGHTS_CURRENT_MAX_BLOCKS;

	minX = WORLDLIMITS_REP_XMIN + blockWidthX * (float)blockX;
	minY = WORLDLIMITS_REP_YMIN + blockWidthY * (float)blockY;
}

///////////////////////////////////////////////////////////////////////////////
//  FindBlockMax
///////////////////////////////////////////////////////////////////////////////
void DistantLightsVisibility::FindBlockMax(s32 blockX, s32 blockY, float& maxX, float& maxY)
{
	Assertf(blockX>=0 && blockX<DISTANTLIGHTS_CURRENT_MAX_BLOCKS, "DistantLightsVisibility::FindBlock - blockX out of range");
	Assertf(blockY>=0 && blockY<DISTANTLIGHTS_CURRENT_MAX_BLOCKS, "DistantLightsVisibility::FindBlock - blockX out of range");

	float blockWidthX = (WORLDLIMITS_REP_XMAX-WORLDLIMITS_REP_XMIN) / DISTANTLIGHTS_CURRENT_MAX_BLOCKS;
	float blockWidthY = (WORLDLIMITS_REP_YMAX-WORLDLIMITS_REP_YMIN) / DISTANTLIGHTS_CURRENT_MAX_BLOCKS;

	maxX = WORLDLIMITS_REP_XMIN + blockWidthX * (float)(blockX+1);
	maxY = WORLDLIMITS_REP_YMIN + blockWidthY * (float)(blockY+1);
}

///////////////////////////////////////////////////////////////////////////////
//  FindBlockCentre
///////////////////////////////////////////////////////////////////////////////
void  DistantLightsVisibility::FindBlockCentre(s32 blockX, s32 blockY, float& posX, float& posY)
{
	Assertf(blockX>=0 && blockX<DISTANTLIGHTS_CURRENT_MAX_BLOCKS, "CDistantLights::FindBlock - blockX out of range");
	Assertf(blockY>=0 && blockY<DISTANTLIGHTS_CURRENT_MAX_BLOCKS, "CDistantLights::FindBlock - blockX out of range");

	posX = WORLDLIMITS_REP_XMIN + ((WORLDLIMITS_REP_XMAX-WORLDLIMITS_REP_XMIN) / DISTANTLIGHTS_CURRENT_MAX_BLOCKS) * (blockX+0.5f);
	posY = WORLDLIMITS_REP_YMIN + ((WORLDLIMITS_REP_YMAX-WORLDLIMITS_REP_YMIN) / DISTANTLIGHTS_CURRENT_MAX_BLOCKS) * (blockY+0.5f);

	Assertf(posX>=WORLDLIMITS_REP_XMIN && posX<=WORLDLIMITS_REP_XMAX, "CDistantLights::FindBlock - posX out of range");
	Assertf(posY>=WORLDLIMITS_REP_YMIN && posY<=WORLDLIMITS_REP_YMAX, "CDistantLights::FindBlock - posY out of range");
}


///////////////////////////////////////////////////////////////////////////////
//  RunFromDependency
///////////////////////////////////////////////////////////////////////////////
bool DistantLightsVisibility::RunFromDependency(const sysDependency& dependency)
{
	void* scratchMemory = dependency.m_Params[DLP_SRATCH_SIZE].m_AsPtr;
	fwStepAllocator scratchAllocator;
	scratchAllocator.Init(scratchMemory, DISTANTLIGHTSVISIBILITY_SCRATCH_SIZE);
	DistantLightsVisibilityOutput *pLocalVisibilityArray = scratchAllocator.Alloc<DistantLightsVisibilityOutput>(DISTANTLIGHTS_CURRENT_MAX_GROUPS, 16);	

#if __BANK
	DistantLightsVisibilityBlockOutput *pLocalBlockVisibilityArray = scratchAllocator.Alloc<DistantLightsVisibilityBlockOutput>(DISTANTLIGHTS_CURRENT_MAX_BLOCKS * DISTANTLIGHTS_CURRENT_MAX_BLOCKS, 16);	
#endif
	tDistantLightGroupData* lightGroups = static_cast<tDistantLightGroupData*> (dependency.m_Params[DLP_GROUPS].m_AsPtr);
	s32* blockFirstGroup = static_cast<s32*> (dependency.m_Params[DLP_BLOCK_FIRST_GROUP].m_AsPtr);
	s32* blockNumCoastalGroups = static_cast<s32*> (dependency.m_Params[DLP_BLOCK_NUM_COSTAL_GROUPS].m_AsPtr);
	s32* blockNumInlandGroups = static_cast<s32*> (dependency.m_Params[DLP_BLOCK_NUM_INLAND_GROUPS].m_AsPtr);

	DistantLightsVisibilityData* pData = static_cast<DistantLightsVisibilityData*>(dependency.m_Params[DLP_VISIBILITY].m_AsPtr);
	const fwScanBaseInfo*	scanBaseInfo = static_cast< const fwScanBaseInfo* >( dependency.m_Params[DLP_SCAN_BASE_INFO].m_AsPtr );
	const Vec4V* hiZBuffer = static_cast< const Vec4V*> (dependency.m_Params[DLP_HIZ_BUFFER].m_AsPtr);
	u32* pDependencyRunning = static_cast<u32*>(dependency.m_Params[DLP_DEPENDENCY_RUNNING].m_AsPtr);
	
	const float maxBlocks = (float)DISTANTLIGHTS_CURRENT_MAX_BLOCKS;
	const float blockRadius = (float)DISTANTLIGHTS_BLOCK_RADIUS;
	const float inlandAlpha = pData->m_InlandAlpha;
	const spdTransposedPlaneSet8 mainViewportCullPlanes = pData->m_mainViewportCullPlanes;
	const spdTransposedPlaneSet8 waterReflectionCullPlanes = pData->m_waterReflectionCullPlanes;
	const spdTransposedPlaneSet8 mirrorReflectionCullPlanes = pData->m_mirrorReflectionCullPlanes;
	const bool bPerformWaterReflectionVisibility = pData->m_PerformWaterReflectionVisibility;
	const bool bPerformMirrorReflectionVisibility = pData->m_PerformMirrorReflectionVisibility;

	DistantLightsVisibilityOutput* pDistantLightsVisibilityEA = pData->m_pDistantLightsVisibilityArray;
	u32* pDistantLightVisibilityCount = pData->m_pDistantLightsVisibilityCount;

#if __BANK
	DistantLightsVisibilityBlockOutput* pDistantLightsVisibilityBlockEA = pData->m_pDistantLightsVisibilityBlockArray;
	u32* pDistantLightVisibilityBlockCount = pData->m_pDistantLightsVisibilityBlockCount;
	u32* pDistantLightVisibilityMirrorReflectionBlockCount = pData->m_pDistantLightsVisibilityMirrorReflectionBlockCount;
	u32* pDistantLightVisibilityWaterReflectionBlockCount = pData->m_pDistantLightsVisibilityWaterReflectionBlockCount;
	u32* pDistantLightVisibilityMainVPCount = pData->m_pDistantLightsVisibilityMainVPCount;
	u32* pDistantLightVisibilityMirrorReflectionCount = pData->m_pDistantLightsVisibilityMirrorReflectionCount;
	u32* pDistantLightVisibilityWaterReflectionCount = pData->m_pDistantLightsVisibilityWaterReflectionCount;
	u32 localVisibilityMainVPCount = 0;
	u32 localVisibilityMirrorReflectionCount = 0;
	u32 localVisibilityWaterReflectionCount = 0;
	u32 localVisibilityBlockCount = 0;
	u32 localVisibilityMirrorReflectionBlockCount = 0;
	u32 localVisibilityWaterReflectionBlockCount = 0;
	DistantLightsVisibilityBlockOutput* pBlockVisibility = pLocalBlockVisibilityArray;
#endif

	u32 localVisibilityCount = 0;
	DistantLightsVisibilityOutput* pVisibility = pLocalVisibilityArray;
	for (s32 blockX = 0; blockX < maxBlocks; blockX++)
	{
		for (s32 blockY = 0; blockY < maxBlocks; blockY++)
		{
			const s32 groupIndex = blockX * (s32)maxBlocks + blockY;
			// check if there are any groups in this block
			if (blockNumCoastalGroups[groupIndex]>0 || blockNumInlandGroups[groupIndex]>0)
			{
				// calc the centre pos of the block
				float centrePosX, centrePosY;
				FindBlockCentre(blockX, blockY, centrePosX, centrePosY);
				Vec3V vCentrePos(centrePosX, centrePosY, 20.0f);

				float blockMinX, blockMinY, blockMaxX, blockMaxY;
				FindBlockMin(blockX, blockY, blockMinX, blockMinY);
				FindBlockMax(blockX, blockY, blockMaxX, blockMaxY);

				spdAABB blockbox(
					Vec3V(blockMinX, blockMinY, 20.0f - blockRadius),
					Vec3V(blockMaxX, blockMaxY, 20.0f + blockRadius)
					);

				u8 blockVisibilityFlags = 0;
				//Flag the renderphases each block is visible in
				if(mainViewportCullPlanes.IntersectsAABB(blockbox) && !IsAABBOccludedAsync(blockbox, pData->m_vCameraPos, SCAN_OCCLUSION_STORAGE_PRIMARY, scanBaseInfo, hiZBuffer
					DEV_ONLY(, pData->m_pDistantLightsOcclusionTrivialAcceptActiveFrustum, pData->m_pDistantLightsOcclusionTrivialAcceptMinZ, pData->m_pDistantLightsOcclusionTrivialAcceptVisiblePixel)))
				{
					blockVisibilityFlags |= DISTANT_LIGHT_VISIBLE_MAIN_VIEWPORT;
#if __BANK
					localVisibilityBlockCount++;
					pBlockVisibility->blockX = (u8)blockX;
					pBlockVisibility->blockY = (u8)blockY;
					pBlockVisibility++;
#endif
				}
				if(bPerformWaterReflectionVisibility && waterReflectionCullPlanes.IntersectsAABB(blockbox))
				{
					blockVisibilityFlags |= DISTANT_LIGHT_VISIBLE_WATER_REFLECTION;
#if __BANK
					localVisibilityWaterReflectionBlockCount++;
#endif
				}
				if(bPerformMirrorReflectionVisibility && mirrorReflectionCullPlanes.IntersectsAABB(blockbox))
				{
					blockVisibilityFlags |= DISTANT_LIGHT_VISIBLE_MIRROR_REFLECTION;
#if __BANK
					localVisibilityMirrorReflectionBlockCount++;
#endif
				}
				
				if(blockVisibilityFlags && blockbox.DistanceToPoint(pData->m_vCameraPos).Getf() <= DistantLightsVisibilityData::ms_BlockMaxVisibleDistance)
				{						
					// loop through the block's lights twice
					// the first time to do coastal lights and the second time to do inland lights
					for (s32 type=0; type<2; type++)
					{
						s32 groupOffset = 0;
						s32 numGroups = 0;
						if (type==0)
						{
							groupOffset = blockFirstGroup[groupIndex];
							numGroups = blockNumCoastalGroups[groupIndex];
						}
						else if (type==1 && inlandAlpha>0.0f)
						{
							groupOffset = blockFirstGroup[groupIndex] + blockNumCoastalGroups[groupIndex];
							numGroups = blockNumInlandGroups[groupIndex];
						}

						for (s32 s=groupOffset; s<groupOffset+numGroups; s++)
						{
							const DistantLightGroup_t* pCurrGroup = &lightGroups[s];

							//Flag the renderphases each group is visible in
							const Vec4V sphere((float)pCurrGroup->centreX,
								(float)pCurrGroup->centreY,
								(float)pCurrGroup->centreZ,
								(float)pCurrGroup->radius );
							const spdSphere boundSphere(sphere);
							u8 groupVisibilityFlags = 0;
							
							spdAABB boundBox;
							boundBox.SetAsSphereV4(sphere);

							if((blockVisibilityFlags & DISTANT_LIGHT_VISIBLE_MAIN_VIEWPORT) && mainViewportCullPlanes.IntersectsSphere(boundSphere) && 
								!IsAABBOccludedAsync(boundBox, pData->m_vCameraPos, SCAN_OCCLUSION_STORAGE_PRIMARY, scanBaseInfo, hiZBuffer
								DEV_ONLY(, pData->m_pDistantLightsOcclusionTrivialAcceptActiveFrustum, pData->m_pDistantLightsOcclusionTrivialAcceptMinZ, pData->m_pDistantLightsOcclusionTrivialAcceptVisiblePixel)))
							{
								groupVisibilityFlags |= DISTANT_LIGHT_VISIBLE_MAIN_VIEWPORT;
#if __BANK
								localVisibilityMainVPCount++;
#endif
							}
							if((blockVisibilityFlags & DISTANT_LIGHT_VISIBLE_WATER_REFLECTION) && waterReflectionCullPlanes.IntersectsSphere(boundSphere))
							{
								groupVisibilityFlags |= DISTANT_LIGHT_VISIBLE_WATER_REFLECTION;
#if __BANK
								localVisibilityWaterReflectionCount++;
#endif
							}
							if((blockVisibilityFlags & DISTANT_LIGHT_VISIBLE_MIRROR_REFLECTION) && mirrorReflectionCullPlanes.IntersectsSphere(boundSphere))
							{
								groupVisibilityFlags |= DISTANT_LIGHT_VISIBLE_MIRROR_REFLECTION;
#if __BANK
								localVisibilityMirrorReflectionCount++;
#endif
							}
							// check if the group is visible
							if(pVisibility && groupVisibilityFlags)
							{
								//write "s" index as output along with group Visibility flags
								pVisibility->index = (s16)s;
								pVisibility->lightType = type;
								pVisibility->visibilityFlags = groupVisibilityFlags;
								pVisibility++;
								localVisibilityCount++;

								Assertf(localVisibilityCount < DISTANTLIGHTS_CURRENT_MAX_GROUPS, "Exceeded maximum groups!");
							}
						}
					}					
				}
			}
		}
	}
	
#if __SPU
	sysDmaLargePutAndWait(pLocalVisibilityArray, (size_t)pDistantLightsVisibilityEA, sizeof(DistantLightsVisibilityOutput)*localVisibilityCount, PUT_DMA_TAG);
#else
	memcpy(pDistantLightsVisibilityEA, pLocalVisibilityArray, sizeof(DistantLightsVisibilityOutput)*localVisibilityCount);
#endif
	sysInterlockedExchange(pDistantLightVisibilityCount, localVisibilityCount);
#if __BANK

#if __SPU
	sysDmaLargePutAndWait(pLocalBlockVisibilityArray, (size_t)pDistantLightsVisibilityBlockEA, sizeof(DistantLightsVisibilityBlockOutput)*localVisibilityBlockCount, PUT_DMA_TAG);
#else
	memcpy(pDistantLightsVisibilityBlockEA, pLocalBlockVisibilityArray, sizeof(DistantLightsVisibilityBlockOutput)*localVisibilityBlockCount);
#endif
	sysInterlockedExchange(pDistantLightVisibilityMainVPCount, localVisibilityMainVPCount);
	sysInterlockedExchange(pDistantLightVisibilityWaterReflectionCount, localVisibilityWaterReflectionCount);
	sysInterlockedExchange(pDistantLightVisibilityMirrorReflectionCount, localVisibilityMirrorReflectionCount);
	sysInterlockedExchange(pDistantLightVisibilityBlockCount, localVisibilityBlockCount);
	sysInterlockedExchange(pDistantLightVisibilityWaterReflectionBlockCount, localVisibilityWaterReflectionBlockCount);
	sysInterlockedExchange(pDistantLightVisibilityMirrorReflectionBlockCount, localVisibilityMirrorReflectionBlockCount);
#endif
	sysInterlockedExchange(pDependencyRunning, 0);
	return true;
}

