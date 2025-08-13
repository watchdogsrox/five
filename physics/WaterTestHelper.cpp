// Class header.
#include "physics/WaterTestHelper.h"

// Framework headers:
#include "fwscene/stores/staticboundsstore.h"

// Game headers:
#include "renderer/River.h"
#include "renderer/Water.h"
#include "scene/world/GameWorldHeightMap.h"

// RAGE headers:

PHYSICS_OPTIMISATIONS()

// Statics ////////////////////////////////////////////////////////////////
#if __BANK
bool CWaterTestHelper::ms_bDisableRiverProbes = false;
#endif // __BANK

float CWaterTestHelper::ms_fDistanceThresholdForRiverProbe = 0.3f;
const float CWaterTestHelper::ms_fProbeVerticalOffset = 1.0f;

//AABB covering the casino interior area underneath the rooftop swimming pool 
static spdAABB waterExclusionVolume_Casino = spdAABB(Vec3V(896.3f, -18.2f, 77.5f), Vec3V(1009.5f, 89.9f, 108.2f));
static spdAABB waterExclusionVolume_MusicStudio = spdAABB(Vec3V(-856.3f, -217.1f, 37.0f), Vec3V(-855.0f, -215.0f, 42.0f));
///////////////////////////////////////////////////////////////////////////


// Helper function to return the top and bottom of a given river entity.
void GetBoundingBoxZForRiverEntity(s32 nRiverEntIdx, float& fMinZ, float& fMaxZ)
{
	const CEntity* pRiverEnt = River::GetRiverEntity(nRiverEntIdx);
	Assert(pRiverEnt);
	const CBaseModelInfo* pModelInfo = pRiverEnt->GetBaseModelInfo();
	spdAABB localBBox = pModelInfo->GetBoundingBox();
	spdAABB worldAABB = TransformAABB(pRiverEnt->GetTransform().GetMatrix(),localBBox);

	fMinZ = worldAABB.GetMin().GetZf();
	fMaxZ = worldAABB.GetMax().GetZf();
}


CWaterTestHelper::CWaterTestHelper()
: m_vLastAsyncRiverProbePosXYZRiverHitStoredW(V_ZERO),
m_vLastRiverHitNormal(VEC3_ZERO),
m_vLastRiverHitPos(VEC3_ZERO),
m_vLastRiverVelocity(VEC3_ZERO),
m_vLastWaterNormal(VEC3_ZERO)
{
}


bool CWaterTestHelper::IsNearRiver(Vec3V_In vPos, bool bUseWidestToleranceBoundingBoxTest)
{
	s32 nRiverEntIdx=-1;
	float fMaxZ=0.0f, fMinZ=0.0f;

	bool bNearRiver = bUseWidestToleranceBoundingBoxTest ?
		g_StaticBoundsStore.NearRiverBounds(vPos,REJECTIONABOVEWATER) : River::IsPositionNearRiver(vPos,&nRiverEntIdx);

	float xPos = vPos.GetXf();
	float yPos = vPos.GetYf();
	if(bUseWidestToleranceBoundingBoxTest)
	{
		fMaxZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(xPos, yPos);
		fMinZ = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(xPos, yPos);

		const float kfVerticalTolerance = 3.0f;
		if(vPos.GetZf() > fMaxZ+kfVerticalTolerance)
			bNearRiver = false;
	}

	// [HACK_GTAV]
	// B*5805044 - "Casino - Underwater filter applied when panning camera"
	{
		if(waterExclusionVolume_Casino.ContainsPoint(vPos))
		{
			bNearRiver = false;
		}
		// B*7395071 - Music studio swimming pool
		else if(waterExclusionVolume_MusicStudio.ContainsPoint(vPos))
		{
			bNearRiver = false;
		}
	}
	// END HACK

	return bNearRiver;

}

bool CWaterTestHelper::GetRiverHeightAtPosition(const Vector3& vPos, float* pfRiverHeightAtPos, bool bUseWidestToleranceBoundingBoxTest)
{
	// If the position is within the bounding box of a river entity, issue a vertical probe to find out the z-coord of
	// the river at this position.
	if(IsNearRiver(RCC_VEC3V(vPos), bUseWidestToleranceBoundingBoxTest))
	{
		// Use the bounding box z-extents to set the start and end points of the probe.
		float fMaxZ, fMinZ;
		fMaxZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vPos.x, vPos.y);
		fMinZ = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(vPos.x, vPos.y);
		///GetBoundingBoxZForRiverEntity(nRiverEntIdx, fMinZ, fMaxZ);
		Vector3 vStart = Vector3(vPos.x, vPos.y, fMaxZ+ms_fProbeVerticalOffset);
		Vector3 vEnd = Vector3(vPos.x, vPos.y, fMinZ-ms_fProbeVerticalOffset);

		WorldProbe::CShapeTestHitPoint isect;
		WorldProbe::CShapeTestResults results(isect);

		if(ProbeForRiver(vStart, vEnd, &results))
		{
			 if(pfRiverHeightAtPos)
			 {
				 Assertf(results.GetNumHits()==1, "Num hits detected=%d/%d", results.GetNumHits(), results.GetSize());
				 *pfRiverHeightAtPos = results[0].GetHitPosition().z;
			 }
			return true;
		}
	}

	return false;
}

bool CWaterTestHelper::ProbeForRiver(const Vector3& vStartPos, const Vector3& vEndPos, WorldProbe::CShapeTestResults* pProbeResults)
{
	// Perform a synchronous line probe against river surface polygons.
	WorldProbe::CShapeTestProbeDesc probeTest;
	probeTest.SetStartAndEnd(vStartPos, vEndPos);
	probeTest.SetResultsStructure(pProbeResults);
	probeTest.SetContext(WorldProbe::ERiverBoundQuery);
	probeTest.SetIncludeFlags(ArchetypeFlags::GTA_RIVER_TYPE);
	probeTest.SetIsDirected(true);

	return WorldProbe::GetShapeTestManager()->SubmitTest(probeTest);
}

void CWaterTestHelper::SubmitAsyncRiverBoundProbe(Vector3& vPos, bool bUseWidestToleranceBoundingBoxTest)
{
#if __BANK
	if(!ms_bDisableRiverProbes)
#endif // __BANK
	{
		// Criteria for spawning a new probe:
		// * Must be in vicinity of a river; we find this out by checking if we are in the bounding box of a river entity.
		// * No point in firing off another probe if we are still waiting on the last one.
		// * Check if this CPhysical has moved more than a threshold distance since we sent the
		//   last probe; if it hasn't we rely on the cached result.
		// * Make sure this CPhysical is still in the physics level since we add it to the probe's exclusion list.
		// * Only issue a new probe if the object is below the maximum height for rivers in the level.

		bool bLastProbeOutOfDate = vPos.Dist2(GetLastAsyncRiverProbePos()) > ms_fDistanceThresholdForRiverProbe*ms_fDistanceThresholdForRiverProbe;

		if(bLastProbeOutOfDate && !m_probeTestHelper.IsProbeActive())
		{
			s32 nRiverEntIdx=-1;
			float fMaxZ=0.0f, fMinZ=0.0f;

			bool bNearRiver = bUseWidestToleranceBoundingBoxTest ?
				g_StaticBoundsStore.NearRiverBounds(RCC_VEC3V(vPos),REJECTIONABOVEWATER) : River::IsPositionNearRiver(RCC_VEC3V(vPos),&nRiverEntIdx);
			if(bUseWidestToleranceBoundingBoxTest)
			{
				fMaxZ = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(vPos.x, vPos.y);
				fMinZ = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(vPos.x, vPos.y);
			
				float kfVerticalTolerance = 3.0f;

				// [HACK_GTAV]
				// B*5805044 - "Casino - Underwater filter applied when panning camera"
				{
					Vec3V testPos = VECTOR3_TO_VEC3V(vPos);
					if(waterExclusionVolume_Casino.ContainsPoint(testPos))
					{
						bNearRiver = false;
					}
					// B*7395071 - Music studio swimming pool
					else if(waterExclusionVolume_MusicStudio.ContainsPoint(testPos))
					{
						bNearRiver = false;
					}
				}
				// END HACK

				if(vPos.z > fMaxZ+kfVerticalTolerance)
					bNearRiver = false;
			}
			if(bNearRiver)
			{
				// Use the bounding box z-extents to set the start and end points of the probe.
				if(!bUseWidestToleranceBoundingBoxTest)
				{
					GetBoundingBoxZForRiverEntity(nRiverEntIdx, fMinZ, fMaxZ);
				}
				Vector3 vStart = Vector3(vPos.x, vPos.y, fMaxZ+ms_fProbeVerticalOffset);
				Vector3 vEnd = Vector3(vPos.x, vPos.y, fMinZ-ms_fProbeVerticalOffset);

				WorldProbe::CShapeTestProbeDesc probeData;
				probeData.SetStartAndEnd(vStart,vEnd);
				probeData.SetContext(WorldProbe::ERiverBoundQuery);
				probeData.SetIncludeFlags(ArchetypeFlags::GTA_RIVER_TYPE);
				probeData.SetIsDirected(true);

				m_probeTestHelper.StartTestLineOfSight(probeData);

				// Cache the position of the object for this probe.
				SetLastAsyncRiverProbePos(vPos);
			}
			else
			{
				SetRiverHitStored(false);
			}
		}
	}
}

bool CWaterTestHelper::GetAsyncRiverBoundProbeResult(Vector3& vRiverHitPos, Vector3& vRiverHitNormal)
{
	// Don't try to retrieve results for a non-existent probe.
	if(m_probeTestHelper.IsProbeActive())
	{
		// Retrieve the results from the river bound probe if available.
		ProbeStatus probeStatus;
		if(m_probeTestHelper.GetProbeResultsWithIntersectionAndNormal(probeStatus, &vRiverHitPos, &vRiverHitNormal))
		{
			physicsAssert(probeStatus==PS_Blocked || probeStatus==PS_Clear);

			// Store the fresh results. These will be reused next time if the next probe is late.
			SetRiverHitStored(probeStatus==PS_Blocked);
			m_vLastRiverHitPos = vRiverHitPos;
			m_vLastRiverHitNormal = vRiverHitNormal;
		}
		else
		{
			// Probe not done yet. Just return the cached values.
			vRiverHitPos = m_vLastRiverHitPos;
			vRiverHitNormal = m_vLastRiverHitNormal;
		}
		return GetRiverHitStored();
	}
	else
	{
		// We are not waiting on any probe at the moment, just return the cached values.
		vRiverHitPos = m_vLastRiverHitPos;
		vRiverHitNormal = m_vLastRiverHitNormal;
		return GetRiverHitStored();
	}
}

bool CWaterTestHelper::ForceUpdateRiverProbeResultSynchronously(Vector3& vPos)
{
	float fRiverHeight = 0.0f;
	if(GetRiverHeightAtPosition(vPos, &fRiverHeight))
	{
		vPos.z = fRiverHeight;
		m_vLastRiverHitPos = vPos;
		m_vLastRiverHitNormal = ZAXIS; // Wrong but better than nothing.
		SetRiverHitStored(true);
		return true;
	}

	return false;
}

bool CWaterTestHelper::GetCachedRiverBoundProbeResult(Vector3& vRiverHitPos, Vector3& vRiverHitNormal) const
{
	// Just return the last probe results.
	vRiverHitPos = m_vLastRiverHitPos;
	vRiverHitNormal = m_vLastRiverHitNormal;

	return GetRiverHitStored();
}

WaterTestResultType CWaterTestHelper::GetWaterLevelIncludingRivers(const Vector3& vPos, float* pWaterZ, bool bForceResult, float fPoolDepth, float fRejectionAboveWater,
											 bool* pShouldPlaySeaSounds, const CPhysical* pContextEntity, bool bHighDetail) const
{
	Vector3 vRiverHitPos, vRiverHitNormal;

	bool bInRiver = GetCachedRiverBoundProbeResult(vRiverHitPos, vRiverHitNormal) && (vRiverHitPos.z+fRejectionAboveWater) > vPos.z;
	bool bInWater = Water::GetWaterLevel(vPos, pWaterZ, bForceResult, fPoolDepth, fRejectionAboveWater, pShouldPlaySeaSounds, pContextEntity, bHighDetail);

	if(pWaterZ && bInRiver)
	{
		*pWaterZ = vRiverHitPos.GetZ();
	}

#if __BANK
	if(pWaterZ)
	{
		*pWaterZ += Water::GetWaterTestOffset();
	}
#endif // __BANK

	if (bInRiver)
	{
		return WATERTEST_TYPE_RIVER;
	}
	else if (bInWater)
	{
		return WATERTEST_TYPE_OCEAN;
	}
	else
	{
		return WATERTEST_TYPE_NONE;
	}
}

WaterTestResultType CWaterTestHelper::GetWaterLevelIncludingRiversNoWaves(const Vector3& vPos, float* pWaterZ, float fPoolDepth, float fRejectionAboveWater,
													bool* pShouldPlaySeaSounds, const CPhysical* pContextEntity)
{
	Vector3 vRiverHitPos, vRiverHitNormal;

	bool bInRiver = GetCachedRiverBoundProbeResult(vRiverHitPos, vRiverHitNormal)  && (vRiverHitPos.z+fRejectionAboveWater) > vPos.z;
	bool bInWater = Water::GetWaterLevelNoWaves(vPos, pWaterZ, fPoolDepth, fRejectionAboveWater, pShouldPlaySeaSounds, pContextEntity);

	if(pWaterZ && bInRiver)
	{
		*pWaterZ = vRiverHitPos.GetZ();
	}

#if __BANK
	if(pWaterZ)
	{
		*pWaterZ += Water::GetWaterTestOffset();
	}
#endif // __BANK

	if (bInRiver)
	{
		return WATERTEST_TYPE_RIVER;
	}
	else if (bInWater)
	{
		return WATERTEST_TYPE_OCEAN;
	}
	else
	{
		return WATERTEST_TYPE_NONE;
	}
}

// Find the height of any water at the given position (horizontal position only is relevant). Uses synchronous river probe.
bool CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(const Vector3& vPos, float* pfRiverHeightAtPos, const bool bConsiderWaves,
															   const float fRejectionAboveWaterOverride, bool bUseWidestToleranceBoundingBoxTest,
															   bool bHighDetail)
{
	float fRejectionAboveWater = fRejectionAboveWaterOverride >= 0.0f ? fRejectionAboveWaterOverride : REJECTIONABOVEWATER;

	// First check for the presence of any lake/ocean water; we might avoid doing a river probe this way.
	if(Likely(bConsiderWaves))
	{
		if(Water::GetWaterLevel(vPos, pfRiverHeightAtPos, true/*unused_param at time of writing*/, POOL_DEPTH, fRejectionAboveWater, NULL, NULL, bHighDetail))
		{
			return true;
		}
	}
	else
	{
		if(Water::GetWaterLevelNoWaves(vPos, pfRiverHeightAtPos, POOL_DEPTH, fRejectionAboveWater, NULL))
		{
			return true;
		}
	}

	// If we haven't found any lake/ocean water, now we can test for rivers.
	if(GetRiverHeightAtPosition(vPos, pfRiverHeightAtPos, bUseWidestToleranceBoundingBoxTest))
	{
		return true;
	}

	// Didn't find any water if we got here.
	return false;
}
