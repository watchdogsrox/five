// ProcessLodLightVisibility
// jlandry: 03/01/2013


#define g_auxDmaTag		(0)
#include "ProcessLodLightVisibility.h"

#include <algorithm>
#include "../basetypes.h"
#include "Lights/LightCommon.h"
#include "system/interlocked.h"
#include "occlusionAsync.h"

#define MAX_LIGHTS_PER_BUCKET	2048
#define GET_DMA_TAG		(0)
#define PUT_DMA_TAG		(1)

#if __SPU
#define ALIGN_TO_16(n) ((n&~0xF)+16)

#define DO_DMA_GET(ppuAddr, lsBuffer, lsPtr, type, size)		{																				\
	u32 ppuEA = (ppuAddr) & ~0xf;																												\
	u32 offset = (ppuAddr) & 0xf;																												\
	u32 dmaSize = ALIGN_TO_16(size + offset);																									\
	u32 lsAddr = (u32)lsBuffer + offset;																										\
	lsPtr = (type*)lsAddr;																														\
	sysDmaGet((void*)lsBuffer, ppuEA, dmaSize, GET_DMA_TAG);																					\
}
#endif

bool ProcessLodLightVisibility::RunFromDependency(const sysDependency& dependency)
{
	void* scratchMemory = dependency.m_Params[0].m_AsPtr;
	fwStepAllocator scratchAllocator;
	scratchAllocator.Init(scratchMemory, PROCESSLODLIGHT_CORONAS_SCRATCH_SIZE);
	Corona_t *pLocalCoronaArray = scratchAllocator.Alloc<Corona_t>(CORONAS_MAX, 16);	
	LodLightVisibility *pLocalVisibilityArray = scratchAllocator.Alloc<LodLightVisibility>(MAX_VISIBLE_LOD_LIGHTS_ASYNC, 16);
	LodLightVisibility *pLocalVisibilitySpotArray = NULL;
	LodLightVisibility *pLocalVisibilityPointArray = NULL;
	LodLightVisibility *pLocalVisibilityCapsuleArray = NULL;

	CLODLightBucket** LODLightsToRender = static_cast<CLODLightBucket**>(dependency.m_Params[1].m_AsPtr);
	int bucketCount = dependency.m_DataSizes[1] / sizeof(CLODLightBucket*);
	u32* aSceneLightHashes = static_cast<u32*>(dependency.m_Params[2].m_AsPtr);
	const int numSceneLightHashes = dependency.m_DataSizes[2] / sizeof(u32);
	u32* aBrokenLightHashes = static_cast<u32*>(dependency.m_Params[3].m_AsPtr);
	const int numBrokenLightHashes = dependency.m_DataSizes[3] / sizeof(u32);
	LodLightVisibilityData* pData = static_cast<LodLightVisibilityData*>(dependency.m_Params[4].m_AsPtr);
	u32* pDependencyRunning = static_cast<u32*>(dependency.m_Params[5].m_AsPtr);	
	const fwScanBaseInfo*	scanBaseInfo = static_cast< const fwScanBaseInfo* >( dependency.m_Params[6].m_AsPtr );
	const Vec4V* hiZBuffer = static_cast< const Vec4V*> (dependency.m_Params[7].m_AsPtr);

#if __SPU
	u32* aCoronaLightHashesBuffer = scratchAllocator.Alloc<u32>(pData->m_numCoronaLightHashes, 16);
	u32 numCoronaLightHashes = pData->m_numCoronaLightHashes;
	u32* aCoronaLightHashes = NULL;

	DO_DMA_GET((u32)pData->m_coronaLightHashes, aCoronaLightHashesBuffer, aCoronaLightHashes, u32, sizeof(u32) * numCoronaLightHashes);
	sysDmaWait(1 << GET_DMA_TAG);
#else
	u32* aCoronaLightHashes = pData->m_coronaLightHashes;
	u32 numCoronaLightHashes = pData->m_numCoronaLightHashes;
#endif

	//Using separate arrays for each light type as a way of sorting them within the group of MAX_VISIBLE_LOD_LIGHTS_ASYNC lights
#if __BANK
	if(pData->m_bPerformSort)
#endif
	{
		pLocalVisibilitySpotArray = scratchAllocator.Alloc<LodLightVisibility>(MAX_VISIBLE_LOD_LIGHTS_ASYNC, 16);
		pLocalVisibilityPointArray = scratchAllocator.Alloc<LodLightVisibility>(MAX_VISIBLE_LOD_LIGHTS_ASYNC, 16);
		pLocalVisibilityCapsuleArray = scratchAllocator.Alloc<LodLightVisibility>(MAX_VISIBLE_LOD_LIGHTS_ASYNC, 16);
	}

	ProcessVisibility(&scratchAllocator, 
		pLocalVisibilityArray, pLocalVisibilitySpotArray, pLocalVisibilityPointArray, pLocalVisibilityCapsuleArray,
		pLocalCoronaArray, 
		LODLightsToRender, bucketCount, 
		aSceneLightHashes, numSceneLightHashes, 
		aBrokenLightHashes, numBrokenLightHashes,
		aCoronaLightHashes, numCoronaLightHashes, 
		scanBaseInfo, 
		hiZBuffer, 
		pData BANK_ONLY(, true));

	sysInterlockedExchange(pDependencyRunning, 0);
	
	return true;
}



void ProcessLodLightVisibility::ProcessVisibility(
	fwStepAllocator *pScratchAllocator,
	LodLightVisibility* pLocalVisibilityArray,
	LodLightVisibility *pLocalVisibilitySpotArray,
	LodLightVisibility *pLocalVisibilityPointArray,
	LodLightVisibility *pLocalVisibilityCapsuleArray,
	Corona_t *pLocalCoronaArray,
	CLODLightBucket* const* LODLightsToRender,
	int bucketCount,
	u32* aSceneLightHashes,
	int numSceneLightHashes,
	u32* aBrokenLightHashes,
	int numBrokenLightHashes,
	u32* aCoronaLightHashes,
	int numCoronaLightHashes,
	const fwScanBaseInfo* scanBaseInfo,
	const Vec4V* hiZBuffer,
	LodLightVisibilityData* pData
	BANK_ONLY(, bool bProcessAsync)
	)
{
	//Using separate arrays for each light type as a way of sorting them within the group of MAX_VISIBLE_LOD_LIGHTS_ASYNC lights

#if __SPU
	void* pBucketBuffer = pScratchAllocator->Alloc(sizeof(CLODLightBucket) + 16, 16);
	void* pLightDataBuffer = pScratchAllocator->Alloc(sizeof(CLODLight) + 16, 16);
	void* pDistantLightBuffer = pScratchAllocator->Alloc(sizeof(CDistantLODLight) + 16, 16);
	void *pDirectionBuffer = pScratchAllocator->Alloc(sizeof(FloatXYZ)*MAX_LIGHTS_PER_BUCKET + 16, 16);
	void *pFalloffBuffer = pScratchAllocator->Alloc(sizeof(float)*MAX_LIGHTS_PER_BUCKET + 16, 16);
	void *pTimeAndStateFlagsBuffer = pScratchAllocator->Alloc(sizeof(u32)*MAX_LIGHTS_PER_BUCKET + 16, 16);
	void *pHashBuffer = pScratchAllocator->Alloc(sizeof(u32)*MAX_LIGHTS_PER_BUCKET + 16, 16);
	void *pCoronaIntensityBuffer = pScratchAllocator->Alloc(sizeof(u8)*MAX_LIGHTS_PER_BUCKET + 16, 16);
	void *pConeOuterAngleBuffer = pScratchAllocator->Alloc(sizeof(u8)*MAX_LIGHTS_PER_BUCKET + 16, 16);
	void *pPositionBuffer = pScratchAllocator->Alloc(sizeof(FloatXYZ)*MAX_LIGHTS_PER_BUCKET + 16, 16);
	void *pRGBIBuffer = pScratchAllocator->Alloc(sizeof(u32)*MAX_LIGHTS_PER_BUCKET + 16, 16);
#else
	(void)pScratchAllocator;
#endif

	std::sort(aSceneLightHashes, aSceneLightHashes + numSceneLightHashes);
	std::sort(aBrokenLightHashes, aBrokenLightHashes + numBrokenLightHashes);
	std::sort(aCoronaLightHashes, aCoronaLightHashes + numCoronaLightHashes);

	ScalarV lodLightRangeSqrV[LODLIGHT_CATEGORY_COUNT];
	ScalarV lodLightCoronaRangeSqrV[LODLIGHT_CATEGORY_COUNT];
	ScalarV lodLightFadeRangeSqrV[LODLIGHT_CATEGORY_COUNT];

	for (u32 i = 0; i < LODLIGHT_CATEGORY_COUNT; i++)
	{
		lodLightRangeSqrV[i] = ScalarV(pData->m_LodLightRange[i]);
		lodLightRangeSqrV[i] = Scale(lodLightRangeSqrV[i], lodLightRangeSqrV[i]);

		lodLightCoronaRangeSqrV[i] = ScalarV(pData->m_LodLightCoronaRange[i]);
		lodLightCoronaRangeSqrV[i] = Scale(lodLightCoronaRangeSqrV[i], lodLightCoronaRangeSqrV[i]);

		lodLightFadeRangeSqrV[i] = ScalarV(pData->m_LodLightFadeRange[i]);
		lodLightFadeRangeSqrV[i] = Scale(lodLightFadeRangeSqrV[i], lodLightFadeRangeSqrV[i]);
	}

	Corona_t* pCoronaArrayEA = pData->m_pCoronaArray;	
	LodLightVisibility* pVisibilityArrayEA = pData->m_pVisibilityArray;
	u32* pCoronaCount = pData->m_pCoronaCount;
	u32* pVisibilityCount = pData->m_pVisibilityCount;
#if __BANK
	u32* pOverflowCount = pData->m_pOverflowCount;
	u32* pLODLightCoronaCount = pData->m_pLODLightCoronaCount;
	const bool bPerformLightVisibility = pData->m_bPerformLightVisibility;
#endif

	int hour = pData->m_hour;
	int nextHour = (pData->m_hour + 1) % 24;
	float fade = pData->m_fade;
	float brightness = pData->m_brightness;
	float coronaSize = pData->m_coronaSize;
	float zBias = pData->m_coronaZBias;
	float lodLightCoronaIntensityMult = pData->m_lodLightCoronaIntensityMult;
	const bool bCheckReflectionPhase = pData->m_bCheckReflectionPhase;

	CLODLightBucket *pBucket = NULL;
	CLODLight *pLightData = NULL;
	CDistantLODLight *pDistantLight = NULL;

	FloatXYZ* aDirection = NULL;
	float* aFalloff = NULL;
	u32* aTimeAndStateFlags = NULL;
	u32* aHash = NULL;
	u8* aCoronaIntensity = NULL;
	u8* aConeOuterAngle = NULL;
	FloatXYZ* aPosition = NULL;
	u32* aRGBI = NULL;
	
	u32 localCoronaCount = 0;
	u32 localVisibilityCount = 0;
	u32 localVisibilitySpotCount = 0;
	u32 localVisibilityPointCount = 0;
	u32 localVisibilityCapsuleCount = 0;
	u32 localVisibilityCountTotal = 0;
#if __BANK
	u32 localOverflowCount = 0;
#endif

	for(int iBucket = 0; iBucket < bucketCount; iBucket++)
	{
#if __SPU
		DO_DMA_GET((u32)LODLightsToRender[iBucket], pBucketBuffer, pBucket, CLODLightBucket, sizeof(CLODLightBucket));
		sysDmaWait(1 << GET_DMA_TAG);

		if(pBucket == NULL)
			continue;

		DO_DMA_GET((u32)pBucket->pLights, pLightDataBuffer, pLightData, CLODLight, sizeof(CLODLight));
		DO_DMA_GET((u32)pBucket->pDistLights, pDistantLightBuffer, pDistantLight, CDistantLODLight, sizeof(CDistantLODLight));
		sysDmaWait(1 << GET_DMA_TAG);		
#else
		pBucket = LODLightsToRender[iBucket];
		if(pBucket == NULL)
			continue;

		pLightData = pBucket->pLights;
		pDistantLight = pBucket->pDistLights;
#endif
		int numLightsRemaining = pBucket->lightCount;
		const s32 numStreetLights = pDistantLight->m_numStreetLights;
		eLodLightCategory category = (eLodLightCategory)pDistantLight->m_category;
		int brokenSourceIDX = 0;
		int	sourceIDX = 0;
		int coronaSourceIDX = 0;
		int arrayIdx = 0;

		while(numLightsRemaining > 0)
		{
#if __SPU
			int lightCount = MIN(numLightsRemaining, MAX_LIGHTS_PER_BUCKET);
			DO_DMA_GET(((u32)pLightData->m_direction.GetElements() + arrayIdx*sizeof(FloatXYZ)), pDirectionBuffer, aDirection, FloatXYZ, sizeof(FloatXYZ)*lightCount);
			DO_DMA_GET(((u32)pLightData->m_falloff.GetElements() + arrayIdx*sizeof(float)), pFalloffBuffer, aFalloff, float, sizeof(float)*lightCount);
			DO_DMA_GET(((u32)pLightData->m_timeAndStateFlags.GetElements() + arrayIdx*sizeof(u32)), pTimeAndStateFlagsBuffer, aTimeAndStateFlags, u32, sizeof(u32)*lightCount);
			DO_DMA_GET(((u32)pLightData->m_hash.GetElements() + arrayIdx*sizeof(u32)), pHashBuffer, aHash, u32, sizeof(u32)*lightCount);
			DO_DMA_GET(((u32)pLightData->m_coronaIntensity.GetElements() + arrayIdx*sizeof(u8)), pCoronaIntensityBuffer, aCoronaIntensity, u8, sizeof(u8)*lightCount);
			DO_DMA_GET(((u32)pLightData->m_coneOuterAngleOrCapExt.GetElements() + arrayIdx*sizeof(u8)), pConeOuterAngleBuffer, aConeOuterAngle, u8, sizeof(u8)*lightCount);
			DO_DMA_GET(((u32)pDistantLight->m_position.GetElements() + arrayIdx*sizeof(FloatXYZ)), pPositionBuffer, aPosition, FloatXYZ, sizeof(FloatXYZ)*lightCount);
			DO_DMA_GET(((u32)pDistantLight->m_RGBI.GetElements() + arrayIdx*sizeof(u32)), pRGBIBuffer, aRGBI, u32, sizeof(u32)*lightCount);
			sysDmaWait(1 << GET_DMA_TAG);
#else
			int lightCount = numLightsRemaining;
			aDirection = pLightData->m_direction.GetElements();
			aFalloff = pLightData->m_falloff.GetElements();
			aTimeAndStateFlags = pLightData->m_timeAndStateFlags.GetElements();
			aHash = pLightData->m_hash.GetElements();
			aCoronaIntensity = pLightData->m_coronaIntensity.GetElements();
			aConeOuterAngle = pLightData->m_coneOuterAngleOrCapExt.GetElements();
			aPosition = pDistantLight->m_position.GetElements();
			aRGBI = pDistantLight->m_RGBI.GetElements();
#endif
			for (int idx = 0; idx < lightCount; idx++)
			{
				const u32 lightHash = aHash[idx];

				if(idx == numStreetLights)
				{
					brokenSourceIDX = 0;
					sourceIDX = 0;
					coronaSourceIDX = 0;
				}

				bool bAddLight = true;
				const packedTimeAndStateFlags& flags = (packedTimeAndStateFlags&)(aTimeAndStateFlags[idx]);
				if(flags.bIsCoronaOnly || (flags.bDontUseInCutscene && pData->m_bIsCutScenePlaying))
				{
					bAddLight = false;
				}

				// Broken Lights
				u32	brokenSourceHash = 0;
				while(brokenSourceIDX < numBrokenLightHashes && aBrokenLightHashes[brokenSourceIDX] < lightHash)
				{
					brokenSourceIDX++;
				}

				if (brokenSourceIDX < numBrokenLightHashes)
				{
					brokenSourceHash = aBrokenLightHashes[brokenSourceIDX];
				}

				// Render
				u32	sourceHash = 0;	// A none hash
				while(sourceIDX < numSceneLightHashes && aSceneLightHashes[sourceIDX] < lightHash)
				{
					sourceIDX++;
				}

				if (sourceIDX < numSceneLightHashes)
				{
					sourceHash = aSceneLightHashes[sourceIDX];
				}

				// Corona-only Lights
				u32	coronaSourceHash = 0;
				while(coronaSourceIDX < numCoronaLightHashes && aCoronaLightHashes[coronaSourceIDX] < lightHash)
				{
					coronaSourceIDX++;
				}

				if (coronaSourceIDX < numCoronaLightHashes)
				{
					coronaSourceHash = aCoronaLightHashes[coronaSourceIDX];
				}

				// If we've exhausted our lookup for checking, or the hashes don't match, then we must be able to render.
				if(sourceHash != lightHash && brokenSourceHash != lightHash && coronaSourceHash != lightHash)
				{
					Vec3V position = Vec3V(aPosition[idx].x, aPosition[idx].y, aPosition[idx].z);
					Vec4V vSphere = Vec4V(position, ScalarV(aFalloff[idx]));
					spdSphere sphereBounds(vSphere);
					const ScalarV distToCameraSqrV = MagSquared(Subtract(position, pData->m_vCameraPos));
					const ScalarV distToCameraWaterReflectionSqrV = MagSquared(Subtract(position, pData->m_vWaterReflectionCameraPos));

					const bool bViewportVsSphere = pData->m_cullPlanes.IntersectsSphere(sphereBounds);

					const bool bCoronaVisibleInMainViewPort = 
						IsLessThanOrEqualAll(distToCameraSqrV, lodLightCoronaRangeSqrV[category]) && 
						bViewportVsSphere;
				
					const bool bCoronaVisibleInWaterReflectionViewPort =  
						bCheckReflectionPhase && 
						IsLessThanOrEqualAll(distToCameraWaterReflectionSqrV, lodLightCoronaRangeSqrV[category]) &&
						pData->m_waterReflectionCullPlanes.IntersectsSphere(sphereBounds);
		
					// Coronas
					bool bAddCorona = (bCoronaVisibleInMainViewPort || bCoronaVisibleInWaterReflectionViewPort);

					if (bAddCorona)
					{
						const u32 timeFlags = flags.timeFlags;

						Color32 colour;
						colour.Set(aRGBI[idx]);
						float intensity = LL_UNPACK_U8(colour.GetAlpha(), MAX_LODLIGHT_INTENSITY) * CalculateTimeFadeCommon(timeFlags, hour, nextHour, fade);
						colour.SetAlpha(255);

						if (category == LODLIGHT_CATEGORY_SMALL)
						{
							ScalarV intensityMult = Subtract(
								distToCameraSqrV,
								Subtract(lodLightRangeSqrV[LODLIGHT_CATEGORY_SMALL], lodLightFadeRangeSqrV[LODLIGHT_CATEGORY_SMALL]));
							intensityMult = Saturate(InvScale(intensityMult, lodLightFadeRangeSqrV[LODLIGHT_CATEGORY_SMALL]));
							intensity *= 1.0f - intensityMult.Getf();
						}

						if (intensity > 0)
						{
							if(localCoronaCount < CORONAS_MAX)
							{
								const float coronaIntensity = LL_UNPACK_U8(aCoronaIntensity[idx], MAX_LODLIGHT_CORONA_INTENSITY);
								const float coneOuterAngle = LL_UNPACK_U8(aConeOuterAngle[idx], MAX_LODLIGHT_CONE_ANGLE);
								const float finalIntensity = intensity * brightness * coronaIntensity * lodLightCoronaIntensityMult;
								Vec3V dir = Vec3V(V_ZERO);

								//Setting flags according to visibility
								u16 coronaFlags = CORONA_DONT_RENDER_UNDERWATER | CORONA_FROM_LOD_LIGHT | CORONA_DONT_ROTATE | CORONA_DONT_RAMP_UP;
								if(bCoronaVisibleInMainViewPort && !bCoronaVisibleInWaterReflectionViewPort)
								{
									coronaFlags |= CORONA_DONT_REFLECT;
								}
								else if(!bCoronaVisibleInMainViewPort && bCoronaVisibleInWaterReflectionViewPort)
								{
									coronaFlags |= CORONA_ONLY_IN_REFLECTION;
								}

								if ((eLightType)flags.lightType == LIGHT_TYPE_SPOT)
								{
									coronaFlags |= CORONA_HAS_DIRECTION;

									// Enable the fadeoff Dir Mult only if the light has a distant Light
									// All lights in small category do not have distant lights
									if(category > LODLIGHT_CATEGORY_SMALL)
									{
										coronaFlags |= CORONA_FADEOFF_DIR_MULT;
									}

									dir = Vec3V(aDirection[idx].x, aDirection[idx].y, aDirection[idx].z);

									// Calculate view direction fade. If view direction fade is negative, corona does not get registered
									// Has the directional multiplier fade by default so just apply it here too
									const Vec3V vForward = pData->m_vCameraPos - position;
									const ScalarV oneOverLength = InvMag(vForward);
									const ScalarV len = InvScale(ScalarV(V_ONE), oneOverLength);
									const ScalarV fadeOut = Saturate(InvScale(Subtract(len, pData->m_vDirMultFadeOffStart), pData->m_vDirMultFadeOffDist));
									const Vec3V vForwardDir = Lerp(fadeOut, Scale(vForward, oneOverLength), dir);
									const float fViewDirFade = Dot(dir, vForwardDir).Getf();
									bAddCorona = ((finalIntensity * fViewDirFade) > 0.0f);
								}

								const float finalSize = coronaSize;
								bAddCorona = bAddCorona && (finalIntensity > 0.0f && finalSize > 0.0f);
								const u8 coneAngleOuter = ((eLightType)flags.lightType == LIGHT_TYPE_SPOT)? ((u8)Clamp<float>(coneOuterAngle, 0.0f, CORONAS_MAX_DIR_LIGHT_CONE_ANGLE_OUTER)) : ((u8)(CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER));
								if( bAddCorona )
								{
									BANK_ONLY(if(Likely(bProcessAsync)))
									{
										Corona_t* pCorona = pLocalCoronaArray + localCoronaCount;
										pCorona->SetPosition(position);
										pCorona->SetSize(finalSize);
										pCorona->SetColor(colour);
										pCorona->SetIntensity(finalIntensity);
										pCorona->SetZBias(zBias);
										pCorona->SetDirectionViewThreshold(Vec4V(dir,ScalarV(V_ONE)));
										pCorona->SetCoronaFlagsAndDirLightConeAngles(coronaFlags, (u8)CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER, coneAngleOuter);

									}
#if __BANK && (__PPU || __WIN32)
									else
									{

										g_coronas.Register(
											position,
											finalSize,
											colour,
											finalIntensity,
											zBias,
											dir,
											1.0f,
											CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER,
											CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER,
											coronaFlags);

									}
#endif

									localCoronaCount++;
								}
							}
#if __BANK
							else
							{
								localOverflowCount++;
							}
#endif
						}
					}
					
					// LOD lights
					bool bLightVisibleInMainViewport = 
						bViewportVsSphere && IsLessThanOrEqualAll(distToCameraSqrV, lodLightRangeSqrV[category]);

					if (category == LODLIGHT_CATEGORY_LARGE)
					{
						bLightVisibleInMainViewport = 
							bLightVisibleInMainViewport && 
							IsGreaterThanOrEqualAll(distToCameraSqrV, lodLightCoronaRangeSqrV[LODLIGHT_CATEGORY_MEDIUM]);
							
					}

					bAddLight = bAddLight && bLightVisibleInMainViewport;

					// Fast check against occlusion system
					if(bAddLight)
					{
#if __BANK
						if(pData->m_bPerformOcclusion)
#endif
						{
							spdAABB bound;
							bound.SetAsSphereV4(vSphere);
							BANK_ONLY(if(Likely(bProcessAsync)))
							{
								bAddLight = (IsAABBOccludedAsync( bound, pData->m_vCameraPos, SCAN_OCCLUSION_STORAGE_PRIMARY, scanBaseInfo, hiZBuffer 
									DEV_ONLY(, pData->m_pLodLightsOcclusionTrivialAcceptActiveFrustum, pData->m_pLodLightsOcclusionTrivialAcceptMinZ, pData->m_pLodLightsOcclusionTrivialAcceptVisiblePixel)) == false
									);
							}
#if __BANK && (__PPU || __WIN32)
							else
							{

								bAddLight = COcclusion::IsBoxVisibleFast(bound);
							}
#endif
						}

					}

					if(bAddLight && (localVisibilityCountTotal < MAX_VISIBLE_LOD_LIGHTS)
#if __BANK 
						&& bPerformLightVisibility
#endif						
						)
					{
						BANK_ONLY(if(Likely(bProcessAsync)))
						{
							if(localVisibilityCount == MAX_VISIBLE_LOD_LIGHTS_ASYNC)
							{
								//Sorting the data
	#if __BANK
								if(pData->m_bPerformSort)
	#endif
								{							
									LodLightVisibility* pVisibility = pLocalVisibilityArray;
									for(u32 indexVisibleSpot=0; indexVisibleSpot<localVisibilitySpotCount; indexVisibleSpot++)
									{
										LodLightVisibility* pVisibilitySpot =  pLocalVisibilitySpotArray + indexVisibleSpot;
										pVisibility->bucketIndex = pVisibilitySpot->bucketIndex;
										pVisibility->lightIndex = pVisibilitySpot->lightIndex;
									#if !__FINAL
										pVisibility->mapDataSlotIndex		= pVisibilitySpot->mapDataSlotIndex;
										pVisibility->parentMapDataSlotIndex	= pVisibilitySpot->parentMapDataSlotIndex;
									#endif
										pVisibility++;
									}
									for(u32 indexVisiblePoint=0; indexVisiblePoint<localVisibilityPointCount; indexVisiblePoint++)
									{
										LodLightVisibility* pVisibilityPoint =  pLocalVisibilityPointArray + indexVisiblePoint;
										pVisibility->bucketIndex = pVisibilityPoint->bucketIndex;
										pVisibility->lightIndex = pVisibilityPoint->lightIndex;
									#if !__FINAL
										pVisibility->mapDataSlotIndex		= pVisibilityPoint->mapDataSlotIndex;
										pVisibility->parentMapDataSlotIndex	= pVisibilityPoint->parentMapDataSlotIndex;
									#endif
										pVisibility++;
									}
									for(u32 indexVisibleCapsulse=0; indexVisibleCapsulse<localVisibilityCapsuleCount; indexVisibleCapsulse++)
									{
										LodLightVisibility* pVisibilityCapsule =  pLocalVisibilityCapsuleArray + indexVisibleCapsulse;
										pVisibility->bucketIndex = pVisibilityCapsule->bucketIndex;
										pVisibility->lightIndex = pVisibilityCapsule->lightIndex;
									#if !__FINAL
										pVisibility->mapDataSlotIndex		= pVisibilityCapsule->mapDataSlotIndex;
										pVisibility->parentMapDataSlotIndex	= pVisibilityCapsule->parentMapDataSlotIndex;
									#endif
										pVisibility++;
									}

								}

	#if __SPU
								sysDmaLargePutAndWait(pLocalVisibilityArray, (size_t)pVisibilityArrayEA, sizeof(LodLightVisibility)*localVisibilityCount, PUT_DMA_TAG);
	#else
								memcpy(pVisibilityArrayEA, pLocalVisibilityArray, sizeof(LodLightVisibility)*localVisibilityCount);
	#endif
								pVisibilityArrayEA = (LodLightVisibility*)((size_t)pVisibilityArrayEA + (localVisibilityCount * sizeof(LodLightVisibility)));
								localVisibilityCount = 0;

	#if __BANK
								if(pData->m_bPerformSort)
	#endif
								{
									localVisibilitySpotCount = 0;
									localVisibilityPointCount = 0;
									localVisibilityCapsuleCount = 0;
								}

							}

						}

						//Add light to specific light type array
#if __BANK
						if(pData->m_bPerformSort)
#endif
						{
							LodLightVisibility* pVisibility = NULL;
							if(flags.lightType == LIGHT_TYPE_SPOT)
							{
								pVisibility = pLocalVisibilitySpotArray + localVisibilitySpotCount;
								localVisibilitySpotCount++;

							}
							else if(flags.lightType == LIGHT_TYPE_POINT)
							{
								pVisibility = pLocalVisibilityPointArray + localVisibilityPointCount;
								localVisibilityPointCount++;

							}
							else if(flags.lightType == LIGHT_TYPE_CAPSULE)
							{
								pVisibility = pLocalVisibilityCapsuleArray + localVisibilityCapsuleCount;
								localVisibilityCapsuleCount++;
							}
							else
							{
								//assert here as there is no LOD Light as directional light
								Assert(flags.lightType != LIGHT_TYPE_DIRECTIONAL);
							}

							if(pVisibility != NULL)
							{
								pVisibility->bucketIndex = (u16)iBucket;
								pVisibility->lightIndex = (u16)idx;
							#if !__FINAL
								pVisibility->mapDataSlotIndex		=	pBucket->mapDataSlotIndex;
								pVisibility->parentMapDataSlotIndex	=	pBucket->parentMapDataSlotIndex;
							#endif
								localVisibilityCount++;
								localVisibilityCountTotal++;
							}	
						}
#if __BANK
						else
						{
							LodLightVisibility* pVisibility = pLocalVisibilityArray + localVisibilityCount;
							pVisibility->bucketIndex = (u16)iBucket;
							pVisibility->lightIndex = (u16)idx;
						#if !__FINAL
							pVisibility->mapDataSlotIndex		=	pBucket->mapDataSlotIndex;
							pVisibility->parentMapDataSlotIndex	=	pBucket->parentMapDataSlotIndex;
						#endif
							localVisibilityCount++;
							localVisibilityCountTotal++;
						}
#endif						
					}
				}
			}

			numLightsRemaining -= lightCount;
			arrayIdx += lightCount;
		}
	}

	BANK_ONLY(if(Likely(bProcessAsync)))
	{

		if(localCoronaCount > 0)
		{
			bool bDone = false;
			while(!bDone)
			{
				u32 registeredCount = sysInterlockedRead(pCoronaCount);
				u32 total = localCoronaCount + registeredCount;
				u32 newCount = MIN(CORONAS_MAX, total);
				u32 numToCopy = newCount - registeredCount;
#if __BANK
				u32 overflowCount = localOverflowCount + localCoronaCount - numToCopy;
#endif

				if(sysInterlockedCompareExchange(pCoronaCount, newCount, registeredCount) == registeredCount)
				{
					pCoronaArrayEA += registeredCount;
#if __SPU
					sysDmaLargePutAndWait(pLocalCoronaArray, (u32)pCoronaArrayEA, sizeof(Corona_t)*numToCopy, PUT_DMA_TAG);
#else
					memcpy(pCoronaArrayEA, pLocalCoronaArray, sizeof(Corona_t)*numToCopy);
#endif
#if __BANK				
					sysInterlockedAdd(pOverflowCount, overflowCount);
#endif
					bDone = true;
				}
			}
		}
		
#if __BANK
		sysInterlockedExchange(pLODLightCoronaCount, localCoronaCount);
#endif
	}
#if __BANK
	else
	{
		*pLODLightCoronaCount = localCoronaCount;
	}
#endif

#if __BANK
	if(bPerformLightVisibility)
#endif
	{
		if(localVisibilityCount > 0)
		{

#if __BANK
			if(pData->m_bPerformSort)
#endif
			{
				LodLightVisibility* pVisibility = pLocalVisibilityArray;
				for(u32 indexVisibleSpot=0; indexVisibleSpot<localVisibilitySpotCount; indexVisibleSpot++)
				{
					LodLightVisibility* pVisibilitySpot =  pLocalVisibilitySpotArray + indexVisibleSpot;
					pVisibility->bucketIndex = pVisibilitySpot->bucketIndex;
					pVisibility->lightIndex = pVisibilitySpot->lightIndex;
				#if !__FINAL
					pVisibility->mapDataSlotIndex		= pVisibilitySpot->mapDataSlotIndex;
					pVisibility->parentMapDataSlotIndex	= pVisibilitySpot->parentMapDataSlotIndex;
				#endif
					pVisibility++;
				}
				for(u32 indexVisiblePoint=0; indexVisiblePoint<localVisibilityPointCount; indexVisiblePoint++)
				{
					LodLightVisibility* pVisibilityPoint =  pLocalVisibilityPointArray + indexVisiblePoint;
					pVisibility->bucketIndex = pVisibilityPoint->bucketIndex;
					pVisibility->lightIndex = pVisibilityPoint->lightIndex;
				#if !__FINAL
					pVisibility->mapDataSlotIndex		= pVisibilityPoint->mapDataSlotIndex;
					pVisibility->parentMapDataSlotIndex	= pVisibilityPoint->parentMapDataSlotIndex;
				#endif
					pVisibility++;
				}
				for(u32 indexVisibleCapsulse=0; indexVisibleCapsulse<localVisibilityCapsuleCount; indexVisibleCapsulse++)
				{
					LodLightVisibility* pVisibilityCapsule =  pLocalVisibilityCapsuleArray + indexVisibleCapsulse;
					pVisibility->bucketIndex = pVisibilityCapsule->bucketIndex;
					pVisibility->lightIndex = pVisibilityCapsule->lightIndex;
				#if !__FINAL
					pVisibility->mapDataSlotIndex		= pVisibilityCapsule->mapDataSlotIndex;
					pVisibility->parentMapDataSlotIndex	= pVisibilityCapsule->parentMapDataSlotIndex;
				#endif
					pVisibility++;
				}
			}

			BANK_ONLY(if(Likely(bProcessAsync)))
			{
#if __SPU
				sysDmaLargePutAndWait(pLocalVisibilityArray, (u32)pVisibilityArrayEA, sizeof(LodLightVisibility)*localVisibilityCount, PUT_DMA_TAG);
#else
				memcpy(pVisibilityArrayEA, pLocalVisibilityArray, sizeof(LodLightVisibility)*localVisibilityCount);
#endif
			}

		}

		BANK_ONLY(if(Likely(bProcessAsync)))
		{
			sysInterlockedExchange(pVisibilityCount, localVisibilityCountTotal);
		}
#if __BANK
		else
		{
			*pVisibilityCount = localVisibilityCountTotal;
		}
#endif

	}
}

