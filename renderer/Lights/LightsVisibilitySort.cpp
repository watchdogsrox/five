// LightsVisibilitySort
// athomas: 03/15/2013

#include "LightsVisibilitySort.h"

#include <algorithm>
#include "../../basetypes.h"
#include "LightCommon.h"
#include "LightSource.h"
#include "../Shadows/ShadowTypes.h"
#include "system/interlocked.h"
#include "atl/array.h"

#define PUT_DMA_TAG		(1)

__forceinline eLightShadowType GetShadowType(const CLightSource& lightSrc, float tanFOV)
{
	eLightShadowType type = (lightSrc.GetType()==LIGHT_TYPE_POINT) ? SHADOW_TYPE_POINT:SHADOW_TYPE_SPOT;
	if (tanFOV > HEMISPHERE_SHADOW_CUTOFF && type == SHADOW_TYPE_SPOT) 
		type = SHADOW_TYPE_HEMISPHERE;

	return type;
}

struct LightDataAsync
{
#if __BE
	u32   m_intLoc;
	float m_size;			 // should be an estimate of screen size. for now it's just 1/distance
#else
	float m_size;			 // should be an estimate of screen size. for now it's just 1/distance
	u32   m_intLoc;
#endif
};

struct LightSortData
{
	u16 index;
	union 
	{
		s64 m_intLocAndSize;
		LightDataAsync m_data;
	};
};


int LightListCompare(const LightSortData* A, const LightSortData* B)
{
	Assertf(*((u32*)&A->m_data.m_size) == (A->m_intLocAndSize & 0xFFFFFFFF), "Fail size 1");
	Assertf(*((u32*)&B->m_data.m_size) == (B->m_intLocAndSize & 0xFFFFFFFF), "Fail size 2");

	s64 diff = A->m_intLocAndSize - B->m_intLocAndSize;
	return (diff < 0) ? -1 : ((diff > 0) ? 1: 0);
}

bool LightsVisibilitySort::RunFromDependency(const sysDependency& dependency)
{
	void* scratchMemory = dependency.m_Params[0].m_AsPtr;
	fwStepAllocator scratchAllocator;
	scratchAllocator.Init(scratchMemory, LIGHTSVISIBILITYSORT_SCRATCH_SIZE);
	LightsVisibilityOutput *pLocalVisibilityArray = scratchAllocator.Alloc<LightsVisibilityOutput>(MAX_STORED_LIGHTS, 16);	

	atFixedArray<LightSortData, MAX_STORED_LIGHTS> sortedLights;   
	atFixedBitSet<MAX_STORED_LIGHTS> localCamInsideBitField; 
	atFixedBitSet<MAX_STORED_LIGHTS> localVisibleLastFrameBitField; 
	localCamInsideBitField.Reset();
	localVisibleLastFrameBitField.Reset();

	CLightSource* lights = static_cast<CLightSource*>(dependency.m_Params[1].m_AsPtr);
	CLightSource* prevLights = static_cast<CLightSource*>(dependency.m_Params[2].m_AsPtr);

	LightsVisibilitySortData* pData = static_cast<LightsVisibilitySortData*>(dependency.m_Params[3].m_AsPtr);
	const spdTransposedPlaneSet8 mainViewportCullPlanes = pData->m_cullPlanes;

	u32* pDependencyRunning = static_cast<u32*>(dependency.m_Params[4].m_AsPtr);	

	LightsVisibilityOutput* pLightsVisibileSortEA = pData->m_pVisibilitySortedArray;
	atFixedBitSet<MAX_STORED_LIGHTS>* pLightsVisibleCamInsideOutput = pData->m_pCamInsideOutput;
	atFixedBitSet<MAX_STORED_LIGHTS>* pLightsVisibleLastFrameOutput = pData->m_pVisibleLastFrameOutput;
	u32* pVisibilitySortedCount = pData->m_pVisibilitySortedCount;
	Vector4 farclip = pData->m_farClip;
	Vector3 cameraPos = pData->m_CameraPos;
	float nearPlaneBoundIncrease = pData->m_nearPlaneBoundsIncrease;

	u32 localVisibilityCount = 0;
	u32 lightStartIndex = pData->m_startLightIndex;
	u32 lightEndIndex = pData->m_endLightIndex;
	u32 prevLightCount = pData->m_prevLightCount;

	LightsVisibilityOutput* pVisibileLightSource = pLocalVisibilityArray;
	for(u32 index = lightStartIndex; index < lightEndIndex; index++)
	{
		CLightSource& currentLightSource = lights[index];

		if (currentLightSource.GetType() != LIGHT_TYPE_POINT && 
			currentLightSource.GetType() != LIGHT_TYPE_SPOT &&
			currentLightSource.GetType() != LIGHT_TYPE_CAPSULE)
			continue;

		// Get the lights params.
		const float dist = (-farclip.Dot3(currentLightSource.GetPosition())) - farclip.w;
		const float radius = currentLightSource.GetBoundingSphereRadius();
		Vec3V vLightPosition = VECTOR3_TO_VEC3V(currentLightSource.GetPosition());
		ScalarV vLightRadius = ScalarV(radius);
		spdSphere boundSphere(vLightPosition, vLightRadius);
		if( dist < (-radius) && mainViewportCullPlanes.IntersectsSphere(boundSphere)
			&& !currentLightSource.GetFlag(LIGHTFLAG_ONLY_IN_REFLECTION))
		{
			fwUniqueObjId shadowTrackingId = currentLightSource.GetShadowTrackingId();
			
			if(shadowTrackingId!=0 && currentLightSource.GetFlag(LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS|LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS))
			{
				for(u32 prevLightIndex = 0; prevLightIndex < prevLightCount; prevLightIndex++)
				{
					if(prevLights[prevLightIndex].GetShadowTrackingId() == shadowTrackingId)
					{
						localVisibleLastFrameBitField.Set(index,true);
						break;
					}
				}
			}
			// calc relative screen size estimate			
			Vector3 dir = (cameraPos - currentLightSource.GetPosition());
			float dist = dir.Mag();
			float size = dist>0 ? radius / dist : 100000.0f; // avoid divide by zero case

			bool cameraInside = currentLightSource.IsPointInLight(cameraPos, nearPlaneBoundIncrease);	
			if (cameraInside)
			{
				size += 100000.0f; // move all the inside ones together at the end (this make debugging easier, but since we make two passes over the list, it in not strictly necessary)
				localCamInsideBitField.Set(index, true);
			}

			LightSortData newLight;
			newLight.index = (u16)index;
			newLight.m_data.m_size = size;
			newLight.m_data.m_intLoc = currentLightSource.GetInteriorLocation().GetAsUint32();

			sortedLights.Append() = newLight;
		}
	}

	if (sortedLights.GetCount() > 0)
	{
		sortedLights.QSort(0, -1, LightListCompare);
	}

	for(int i=0; i<sortedLights.GetCount(); i++)
	{
		//Add the light
		pVisibileLightSource->index = sortedLights[i].index;
		pVisibileLightSource++;
		localVisibilityCount++;
	}

#if __SPU
	sysDmaLargePutAndWait(pLocalVisibilityArray, (size_t)pLightsVisibileSortEA, sizeof(LightsVisibilityOutput)*localVisibilityCount, PUT_DMA_TAG);
#else
	memcpy(pLightsVisibileSortEA, pLocalVisibilityArray, sizeof(LightsVisibilityOutput)*localVisibilityCount);
#endif

#if __SPU
	sysDmaLargePutAndWait((&localCamInsideBitField), (size_t)pLightsVisibleCamInsideOutput, sizeof(atFixedBitSet<MAX_STORED_LIGHTS>), PUT_DMA_TAG);
#else
	memcpy(pLightsVisibleCamInsideOutput, (&localCamInsideBitField), sizeof(atFixedBitSet<MAX_STORED_LIGHTS>));
#endif

#if __SPU
	sysDmaLargePutAndWait((&localVisibleLastFrameBitField), (size_t)pLightsVisibleLastFrameOutput, sizeof(atFixedBitSet<MAX_STORED_LIGHTS>), PUT_DMA_TAG);
#else
	memcpy(pLightsVisibleLastFrameOutput, (&localVisibleLastFrameBitField), sizeof(atFixedBitSet<MAX_STORED_LIGHTS>));
#endif

	sysInterlockedExchange(pVisibilitySortedCount, localVisibilityCount);
	sysInterlockedExchange(pDependencyRunning, 0);
	return true;
}


