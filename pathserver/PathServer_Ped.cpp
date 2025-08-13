#include "PathServer\PathServer.h"

#ifdef GTA_ENGINE

// Framework headers
#include "fwmaths\Random.h"
#include "fwvehicleai\pathfindtypes.h"
#include "fwscene/search/Search.h"

// Game headers
#include "ai/stats.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/viewports/Viewport.h"
#include "camera/viewports/ViewportManager.h"
#include "game/PopMultiplierAreas.h"
#include "Debug/VectorMap.h"
#include "ModelInfo\PedModelInfo.h"
#include "Peds\Ped.h"
#include "Peds\PedDebugVisualiser.h"
#include "Peds/pedpopulation.h"
#include "Peds/PedIntelligence.h"
#include "scene/world/GameWorld.h"
#include "script\script_areas.h"
#include "task/Movement/TaskGotoPoint.h"
#include "task/Movement/TaskNavBase.h"
#include "VehicleAi\pathfind.h"
#include "Vehicles\vehicle.h"

NAVMESH_OPTIMISATIONS()
AI_NAVIGATION_OPTIMISATIONS()
#define PEDGEN_NUM_NAVMESHES				9
#define __PEDGEN_FLOODFILL_NEARBY_NODES		0

using namespace AIStats;

void CPedGenNavMeshIteratorAmbient::AddNavMesh(CNavMesh * pNavMesh)
{
	Assert(pNavMesh);
	Assert(m_iNumEntries < m_iMaxNumEntries);

	if(m_iNumEntries < m_iMaxNumEntries)
	{
		m_Containers[m_iNumEntries].m_iNavMesh = pNavMesh->GetIndexOfMesh();
		m_Containers[m_iNumEntries].m_iCurrentPoly = 0;

		if(pNavMesh->GetNonResourcedData()->m_iNumPedDensityPolys == -1)
		{
			// Find how many polygons at the beginning of the polys array have non-zero ped density
			// NB: Somewhat fucking annonyingly, there will be occasion 'holes' in this array where I've
			// turned off ped-density during the final flood-fill stage of navmesh construction.
			// We'll just have to live with this.  It makes it all the more important to precalculate
			// the number of ped-density polys & store in the navmesh in future.
			int iLastPedDensityPoly = 0;
			for(int p=0; p<pNavMesh->GetNumPolys(); p++)
			{
				TNavMeshPoly * pPoly = pNavMesh->GetPoly(p);
				if(pPoly->GetPedDensity()!=0)
					iLastPedDensityPoly = p;
			}

			pNavMesh->GetNonResourcedData()->m_iNumPedDensityPolys = iLastPedDensityPoly;
		}

		m_Containers[m_iNumEntries].m_iNumPolysFromMesh = pNavMesh->GetNonResourcedData()->m_iNumPedDensityPolys;

#if __BANK
		// If we're creating peds anywhere, then all polys in the mesh are candidates
		if(CPedPopulation::ms_createPedsAnywhere)
			m_Containers[m_iNumEntries].m_iNumPolysFromMesh = pNavMesh->GetNumPolys();
#endif

		m_iTotalNumPolys += m_Containers[m_iNumEntries].m_iNumPolysFromMesh;

		m_iNumEntries++;
	}
}

const float CPathServerAmbientPedGen::TPedGenVars::m_fMinCreationZDist = 5.0f;
const float CPathServerAmbientPedGen::TPedGenVars::m_fMaxCreationZDist = 30.0f;

// The following two arrays describe for each ped-density level, the search radius used
// to detect nearby peds, and the desired number of peds which we wish to have in that area.
// The generation algorithm then scores each spawn point based on the deficit between the
// actual number and the desired, with greater values being favoured for ped generation.
const float CPathServerAmbientPedGen::TPedGenVars::m_fNearbyPedsDesiredCount[8] =
{
	0.0f,	// density 0, never used for generation
	3.0f,
	4.0f,
	4.0f,
	4.0f,
	4.0f,
	4.0f,
	4.0f
};

const float CPathServerAmbientPedGen::TPedGenVars::m_fNearbyPedsSearchDistances[8] =
{
	0.0f,	// density 0, never used for generation
	75.0f,
	65.0f,
	55.0f,
	45.0f,
	35.0f,
	25.0f,
	20.0f
};

bank_bool CPathServerAmbientPedGen::ms_bPromoteDensityOneToDensityTwo = true;

CPathServerAmbientPedGen::CPathServerAmbientPedGen()
{
	m_iPedGenPercentagePerNavMesh = 15;
	m_bPedGenNeedsProcessing = false;

	m_vPedGenConstraintArea.Zero();
	m_fPedGenConstraintAreaRadiusSqr = 0.0f;
	m_bUsePedGenConstraintArea = false;

	m_PedGenLoadedMeshes.Reserve(128);
}

void CPathServerAmbientPedGen::Reset()
{
	for(s32 i=0; i<MAX_PEDGEN_BLOCKED_AREAS; i++)
	{
		m_PedGenBlockedAreas[i].Clear();
	}
}

float CPathServerAmbientPedGen::GetNumPedsAroundSpawnPoint(const Vector3 & vPos, const float fAreaRadius)
{
	const float fAreaRadiusSqr = fAreaRadius*fAreaRadius;
	static const float fPedScale = 2.0f;
	static const float fMinPedValue = 1.0f;
	static const float fCoincidentPedRadiusSqr = 4.0f;
	static const float fCostForCoincidentPed = 100.0f;
	static const float fMinZDist = 5.0f;
	static const float fMaxZDist = 10.0f;

	float fPedCount = 0.0f;
	for(int p=0; p<m_PedGenVars.m_iNumPedPositions; p++)
	{
		const Vector3 & vPedPos = m_PedGenVars.m_PedPositions[p];
		Vector3 vDiff = vPedPos - vPos;
		const float fXYMag2 = vDiff.XYMag2();
		if(fXYMag2 < fAreaRadiusSqr)
		{
			const float fDeltaZ = Abs(vDiff.z);

			// All peds are considered for the coincident ped penalty
			if(fXYMag2 < fCoincidentPedRadiusSqr && fDeltaZ < 2.0f)
			{
				fPedCount += fCostForCoincidentPed;
			}
			// But in non-coincident cases, only non-scenario peds are added into the contribution of nearby peds
			else if((m_PedGenVars.m_PedFlags[p] & TPedGenParams::FLAG_SCENARIO_PED)==0)
			{
				const float fDistAwayXY = 1.0f / invsqrtf_fast(fXYMag2);
				float fPed = Max(fMinPedValue, (1.0f - (square(fDistAwayXY / fAreaRadius))) * fPedScale);

				// Scale contribution based on Z-distance

				if(fDeltaZ > fMinZDist)
				{
					const float fScale = 1.0f - Clamp((fDeltaZ - fMinZDist) / (fMaxZDist - fMinZDist), 0.0f, 1.0f);
					fPed *= fScale;
				}

				fPedCount += fPed;
			}
		}
	}
	return fPedCount;
}

void CPathServerAmbientPedGen::FindPedGenCoords_StartNewTimeSlice(const atArray<s32> & loadedMeshes)
{
	m_PedGenVars.m_bNavMeshesHaveChanged = false;
	m_PedGenNavMeshIterator.Reset();

	Vector2 vPedGenMin2d, vPedGenMax2d;
	m_PedGenVars.m_PopGenShape.GetBoundingMinMax(vPedGenMin2d, vPedGenMax2d);
	Vector3 vPedGenMin(vPedGenMin2d.x, vPedGenMin2d.y, -MINMAX_MAX_FLOAT_VAL);
	Vector3 vPedGenMax(vPedGenMax2d.x, vPedGenMax2d.y, MINMAX_MAX_FLOAT_VAL);

	m_PedGenVars.m_AreaMinMax.Set(vPedGenMin, vPedGenMax);

	Vector3 vPolyCentroid, vPolyMin, vPolyMax;

	aiNavMeshStore * pStore = CPathServer::GetNavMeshStore(kNavDomainRegular);
	Assert(pStore);

	for(int m=0; m<loadedMeshes.GetCount(); m++)
	{
		const s32 iMesh = loadedMeshes[m];

		if(iMesh != pStore->GetMeshIndexNone())
		{
			CNavMesh * pNavMesh = pStore->GetMeshByIndex(iMesh);

			if(pNavMesh && m_PedGenVars.m_AreaMinMax.IntersectsXY(pNavMesh->GetQuadTree()->m_MinMax))
			{
				m_PedGenNavMeshIterator.AddNavMesh(pNavMesh);
			}
		}
	}

	static dev_s32 iInstantFillPercentage = 50;
	const s32 iPercentage = (CPedPopulation::ShouldUseStartupMode()) ? iInstantFillPercentage : m_iPedGenPercentagePerNavMesh;

	m_PedGenNavMeshIterator.StartNewInteration(iPercentage);
}

void CPathServerAmbientPedGen::FindPedGenCoords_ContinueTimeSlice()
{
	static dev_float fMinScaleZ				= 0.1f;

	Vector2 vKeyholeMin, vKeyHoleMax;
	m_PedGenVars.m_PopGenShape.GetBoundingMinMax(vKeyholeMin, vKeyHoleMax);

	const bool bStartupMode = CPedPopulation::ShouldUseStartupMode();

	Vector3 vPolyCentroid, vPolyMin, vPolyMax;

	CNavMesh * pNavMesh = NULL;
	TNavMeshPoly * pPoly = NULL;

	s32 iNumIters = 0;
	s32 iMaxNumIters = m_PedGenNavMeshIterator.GetTotalNumPolys();

	bool bAbort = false;

	while( (!bAbort) && m_PedGenCoords.HasSpaceLeft() && m_PedGenNavMeshIterator.GetNext(&pNavMesh, &pPoly) )
	{
		if(iNumIters++ >= iMaxNumIters)
		{
			break;
		}

		bAbort = CPathServer::m_bForceAbortCurrentRequest;

#if __BANK
		s32 iPolyPedDensity = CPedPopulation::ms_createPedsAnywhere ? MAX_PED_DENSITY : pPoly->GetPedDensity();
#else
		s32 iPolyPedDensity = pPoly->GetPedDensity();		
#endif

		// url:bugstar:1732800 - experiment to treat density 1 same as density 2, to improve number of peds on next-gen
		if( ms_bPromoteDensityOneToDensityTwo && iPolyPedDensity==1 )
			iPolyPedDensity = 2;


//		if (CPedPopulation::GetInstantFillPopulation() && iPolyPedDensity > 0)
//			iPolyPedDensity = MAX_PED_DENSITY;

		// We may sometimes get polys with zero ped-density.  This is because ped-density is sometimes reset as a final stage
		// of navmesh contruction, after the list has been sorted.  Deal with it! :-s
		if(iPolyPedDensity > 0 && !pPoly->GetIsSwitchedOffForPeds() && !pPoly->GetIsWater() && m_PedGenVars.m_AreaMinMax.IntersectsXY(pPoly->m_MinMax))
		{
			pNavMesh->GetPolyCentroidQuick(pPoly, vPolyCentroid);

			if( CPedPopulation::GetAdjustPedSpawnDensity() != 0 )
			{
				if( vPolyCentroid.IsGreaterOrEqualThan(CPedPopulation::GetAdjustPedSpawnDensityMin()) &&
					CPedPopulation::GetAdjustPedSpawnDensityMax().IsGreaterThan(vPolyCentroid) )
				{
					iPolyPedDensity += CPedPopulation::GetAdjustPedSpawnDensity();
					iPolyPedDensity = Clamp(iPolyPedDensity, 0, MAX_PED_DENSITY);
				}
			}

			if( CThePopMultiplierAreas::GetNumPedAreas() != 0 )
			{
				const float fMultiplier = CThePopMultiplierAreas::GetPedDensityMultiplier(vPolyCentroid, false);
				iPolyPedDensity = (s32) ((float)iPolyPedDensity * fMultiplier);

				iPolyPedDensity = Clamp(iPolyPedDensity, 0, MAX_PED_DENSITY);

				if( iPolyPedDensity == 0 )
				{
					continue;
				}
			}

			const CPopGenShape::GenCategory cat = m_PedGenVars.m_PopGenShape.CategorisePoint(Vector2(vPolyCentroid.x, vPolyCentroid.y));

			if( bStartupMode || CPopGenShape::IsCategoryUsable(cat) )
			{
				const float fPolyMidZ = pPoly->m_MinMax.GetMidZAsFloat();
				const float fDeltaZ = Abs(fPolyMidZ - m_PedGenVars.m_PopGenShape.GetCenter().z);
				float fScoreScaleZ;
				if(fDeltaZ > m_PedGenVars.m_fMinCreationZDist)
				{
					fScoreScaleZ = ((fDeltaZ-m_PedGenVars.m_fMinCreationZDist) / (m_PedGenVars.m_fMaxCreationZDist-m_PedGenVars.m_fMinCreationZDist));
					fScoreScaleZ = 1.0f - Clamp(fScoreScaleZ, 0.0f, 1.0f);
					fScoreScaleZ = MAX(fScoreScaleZ, fMinScaleZ);
				}
				else
				{
					fScoreScaleZ = 1.0f;
				}

				//-----------------------------------------------------------------------------------------

				const float fSearchDimensions = m_PedGenVars.m_fNearbyPedsSearchDistances[iPolyPedDensity];
				const float fDesiredNumPeds = m_PedGenVars.m_fNearbyPedsDesiredCount[iPolyPedDensity] * fScoreScaleZ;
				const float fNumNearbyPeds = GetNumPedsAroundSpawnPoint(vPolyCentroid, fSearchDimensions);
				const float fPedDeficit = (fDesiredNumPeds - fNumNearbyPeds);


				if(fPedDeficit > 0.0f)
				{
					s32 iFlags = pPoly->GetIsInterior() ? CPedGenCoords::PEDGENFLAG_INTERIOR : 0;

					if(cat==CPopGenShape::GC_InFOV_usableIfOccluded)
						iFlags |= CPedGenCoords::PEDGENFLAG_USABLE_IF_OCCLUDED;

					if(!m_PedGenCoords.Insert(vPolyCentroid, iFlags, fPedDeficit ))
					{
						return;
					}

					if(m_PedGenVars.m_iNumPedPositions < MAX_STORED_PED_POSITIONS)
					{
						m_PedGenVars.m_PedPositions[m_PedGenVars.m_iNumPedPositions++] = vPolyCentroid;
					}
				}
			}
		}
	}
}



void CPathServerAmbientPedGen::FindPedGenCoordsBackgroundTask()
{
	// This struct might be in use whilst the main thread copies the spawn-coords
	// into its own data structure for use.  Contention will be brief.
	while(m_PedGenCoords.GetInUse())
	{
		sysIpcYield(1);
	}

	m_PedGenCoords.SetInUse(true);

	//********************************************************************
	//	Try to enter the critical section which protects the navmesh data.

	LOCK_NAVMESH_DATA;


	// Take a copy of the loaded meshes list
	{
		LOCK_STORE_LOADED_MESHES;

		aiNavMeshStore *pStore = CPathServer::GetNavMeshStore(kNavDomainRegular);
		Assert(pStore);
		const atArray<s32> & loadedMeshes = pStore->GetLoadedMeshes();
		while(loadedMeshes.GetCount() > m_PedGenLoadedMeshes.GetCapacity())
		{
			s32 iReserveSize = m_PedGenLoadedMeshes.GetCapacity()*2;
			m_PedGenLoadedMeshes.Reset();
			m_PedGenLoadedMeshes.Reserve(iReserveSize);
		}
		m_PedGenLoadedMeshes.CopyFrom( loadedMeshes.GetElements(), (u16) loadedMeshes.GetCount() );

#ifndef GTA_ENGINE
		UNLOCK_STORE_LOADED_MESHES;
#endif
	}

	// If loaded navmeshes have changed OR we've finished the last timeslice, then start again with a new timeslice
	if( m_PedGenVars.m_bNavMeshesHaveChanged || m_PedGenNavMeshIterator.IsAtEnd() || (!m_PedGenCoords.HasSpaceLeft()) )
	{
		// We can only do this once any existing pedgen coords have been retrieved
		if(m_PedGenCoords.GetIsConsumed())
		{
			m_PedGenCoords.SetIsConsumed(false);
			FindPedGenCoords_StartNewTimeSlice(m_PedGenLoadedMeshes);
		}
		// Otherwise we must return & wait for the coords to be consumed
		else
		{
			m_PedGenCoords.SetIsComplete(true);
			m_PedGenCoords.SetInUse(false);
			return;
		}
	}

	const bool bAbort = CPathServer::m_bForceAbortCurrentRequest;

	if( m_PedGenCoords.HasSpaceLeft() && !bAbort)
	{
		FindPedGenCoords_ContinueTimeSlice();
	}

	m_PedGenCoords.SetInUse(false);
}

bool CPathServerAmbientPedGen::AreCachedPedGenCoordsReady() const
{
	return ( !m_PedGenCoords.GetInUse() && m_PedGenCoords.GetIsComplete() );
}

bool CPathServerAmbientPedGen::GetCachedPedGenCoords(CPedGenCoords & destCachedCoords)
{
	if(m_PedGenCoords.GetInUse())
		return false;

	m_PedGenCoords.SetInUse(true);
	m_PedGenCoords.CopyToPedGenCoords(destCachedCoords);
	m_PedGenCoords.Reset();
	m_PedGenCoords.SetIsConsumed(true);

	return true;
}

bool CPathServerAmbientPedGen::IsActive()
{
	return m_PedGenCoords.GetInUse();
}

void CPathServerAmbientPedGen::ClearCachedPedGenCoords()
{
	m_PedGenCoords.Reset();
	m_PedGenCoords.SetIsConsumed(true);
}

void CPathServerAmbientPedGen::ProcessPedGenBlockedAreas()
{
	if(m_PedGenCoords.GetInUse() || ((fwTimer::GetSystemFrameCount()&7)!=0) )
		return;

	u32 iTime = fwTimer::GetTimeInMilliseconds();

	for(s32 i=0; i<MAX_PEDGEN_BLOCKED_AREAS; i++)
	{
		// Cutscene areas never have a duration, they are explicitly removed when the cutscene exits
		if(m_PedGenBlockedAreas[i].m_bActive && !m_PedGenBlockedAreas[i].IsCutScene())
		{
			// Has this one expired?
			u32 iElapsedTime = iTime - m_PedGenBlockedAreas[i].m_iStartTime;
			if(iElapsedTime > m_PedGenBlockedAreas[i].m_iDuration)
			{
				m_PedGenBlockedAreas[i].m_bActive = false;
			}
			else if (m_PedGenBlockedAreas[i].m_bShrink)
			{
				aiAssert(m_PedGenBlockedAreas[i].m_iType == CPedGenBlockedArea::ESphere);
				float fPercentDone = (static_cast<float>(iElapsedTime) / static_cast<float>(m_PedGenBlockedAreas[i].m_iDuration));
				if (fPercentDone > .5f)
				{
					float fNewRadius = Lerp( (2.f * (fPercentDone - .5f)), m_PedGenBlockedAreas[i].m_Sphere.m_fOriginalRadius, 0.f);
					m_PedGenBlockedAreas[i].m_Sphere.m_fActiveRadius = fNewRadius;
					m_PedGenBlockedAreas[i].m_Sphere.m_fRadiusSqr = rage::square(fNewRadius);
				}
			}
		}
	}
}

#if __ASSERT

void CPathServerAmbientPedGen::PrintDebugPedGenAreas()
{
	Displayf("Dumping all ped gen areas...");
	for(s32 i=0; i<MAX_PEDGEN_BLOCKED_AREAS; i++)
	{
		Displayf("%d:  %s, %s, created at %u with duration %u", i, m_PedGenBlockedAreas[i].m_bActive ? "Is active" : "Not active", 
			m_PedGenBlockedAreas[i].IsCutScene() ? "Created by cutscene" : "Not created by cutscene", m_PedGenBlockedAreas[i].m_iStartTime,
			m_PedGenBlockedAreas[i].m_iDuration);
	}
}

#endif

s32 CPathServerAmbientPedGen::AddPedGenBlockedArea(const Vector3 & vOrigin, const float fRadius, const u32 iDuration, CPedGenBlockedArea::EOwner owner, const bool bShrink)
{
	u32 iStartTime = fwTimer::GetTimeInMilliseconds();
	static dev_s32 iMaxStallTime = 2000;

	while(m_PedGenCoords.GetInUse())
	{
		if(fwTimer::GetTimeInMilliseconds()-iStartTime > iMaxStallTime)
			break;

		sysIpcYield(1);
	}

	for(s32 i=0; i<MAX_PEDGEN_BLOCKED_AREAS; i++)
	{
		if(!m_PedGenBlockedAreas[i].m_bActive)
		{
			m_PedGenBlockedAreas[i].Init(vOrigin, fRadius, iDuration, owner, bShrink);
			return i;
		}
	}

	// Failed to add
	return -1;
}

s32 CPathServerAmbientPedGen::AddPedGenBlockedArea(const Vector3 * pPts, const float fTopZ, const float fBottomZ, const u32 iDuration, CPedGenBlockedArea::EOwner owner)
{
	while(m_PedGenCoords.GetInUse())
	{
		sysIpcYield(1);
	}

	bool bCutscene = owner == CPedGenBlockedArea::ECutscene;
	for(s32 i=0; i<MAX_PEDGEN_BLOCKED_AREAS; i++)
	{
		// Find a non-active slot.  Or, if this is a cutscene then find any non-cutscene slot.
		if(!m_PedGenBlockedAreas[i].m_bActive || (bCutscene && !m_PedGenBlockedAreas[i].IsCutScene()))
		{
			const float fExtraMinZ = (bCutscene) ? 1.0f : 0.0f;
			m_PedGenBlockedAreas[i].Init(pPts, fTopZ, fBottomZ - fExtraMinZ, iDuration, owner);
			return i;
		}
	}

	// Failed to add
	return -1;
}

void CPathServerAmbientPedGen::UpdatePedGenBlockedArea(s32 iIndex, const Vector3 & vOrigin, const float fRadius, const u32 iDuration)
{
	if (aiVerify(iIndex >= 0 && iIndex < MAX_PEDGEN_BLOCKED_AREAS))
	{
		m_PedGenBlockedAreas[iIndex].m_bActive = true;
		m_PedGenBlockedAreas[iIndex].Init(vOrigin, fRadius, iDuration, m_PedGenBlockedAreas[iIndex].GetOwner(), m_PedGenBlockedAreas[iIndex].m_bShrink);
	}
}

bool CPathServerAmbientPedGen::IsInPedGenBlockedArea(const Vector3 & vPos)
{
	for(s32 i=0; i<MAX_PEDGEN_BLOCKED_AREAS; i++)
	{
		if(m_PedGenBlockedAreas[i].m_bActive)
		{
			Assert(m_PedGenBlockedAreas[i].m_iType != CPedGenBlockedArea::ENone);

			if(m_PedGenBlockedAreas[i].m_iType==CPedGenBlockedArea::ESphere)
			{
				if(m_PedGenBlockedAreas[i].m_Sphere.LiesWithin(vPos))
					return true;
			}
			else if(m_PedGenBlockedAreas[i].m_iType==CPedGenBlockedArea::EAngledArea)
			{
				if(m_PedGenBlockedAreas[i].m_Area.LiesWithin(vPos))
					return true;
			}
		}
	}

	return false;
}


// Clear all the blocking areas; optionally elect to only clear those defined for a cutscene.
void CPathServerAmbientPedGen::ClearAllPedGenBlockingAreas(const bool bCutsceneOnly, const bool bNonCutsceneOnly)
{
	Assert(!(bCutsceneOnly && bNonCutsceneOnly));

	while(m_PedGenCoords.GetInUse())
	{
		sysIpcYield(1);
	}

	for(s32 i=0; i<MAX_PEDGEN_BLOCKED_AREAS; i++)
	{
		if(m_PedGenBlockedAreas[i].m_bActive)
		{
			if( (!bCutsceneOnly || m_PedGenBlockedAreas[i].IsCutScene()) && (!bNonCutsceneOnly || !m_PedGenBlockedAreas[i].IsCutScene()) )
			{
				m_PedGenBlockedAreas[i].m_iDuration = 0;
				m_PedGenBlockedAreas[i].m_bActive = false;
			}
		}
	}
}


void CPathServerAmbientPedGen::ClearAllPedGenBlockingAreasWithSpecificOwner(CPedGenBlockedArea::EOwner owner)
{
	while(m_PedGenCoords.GetInUse())
	{
		sysIpcYield(1);
	}

	for(s32 i=0; i<MAX_PEDGEN_BLOCKED_AREAS; i++)
	{
		if(m_PedGenBlockedAreas[i].m_bActive)
		{
			if(m_PedGenBlockedAreas[i].GetOwner() == owner)
			{
				m_PedGenBlockedAreas[i].m_iDuration = 0;
				m_PedGenBlockedAreas[i].m_bActive = false;
			}
		}
	}
}


void CPathServerAmbientPedGen::DefinePedGenConstraintArea(const Vector3 & vPos, const float fRadius)
{
	Assertf(!m_bUsePedGenConstraintArea, "DefinePedGenConstraintArea - there is already an area defined, this must be destroyed first.");

	m_vPedGenConstraintArea = vPos;
	m_fPedGenConstraintAreaRadiusSqr = fRadius*fRadius;
	m_bUsePedGenConstraintArea = true;
}

void CPathServerAmbientPedGen::DestroyPedGenConstraintArea()
{
	Assertf(m_bUsePedGenConstraintArea, "DefinePedGenConstraintArea - there is no area defined.");
	m_bUsePedGenConstraintArea = false;
}


//***************************************************************************************
//	SetPedGenParams
//	This function is only called from the main game thread.  It sets up the parameters
//	which the pathserver thread then uses in order to generate potential ped-generation
//	coordinates for the ped population system.

void CPathServerAmbientPedGen::SetPedGenParams(const TPedGenParams & params, const CPopGenShape & popGenShape)
//	const int iNumPedPositions,
//	const Vector3 * pPedPositionsArray,
//	,
//	const float * pPedsPerMeterDensityLookupArray,
//	const float fMinZDistance, const float fMaxZDistance)
{
	if(m_PedGenCoords.GetInUse())
		return;

	m_bPedGenNeedsProcessing = true;

	m_PedGenVars.m_iNumPedPositions = MIN(params.m_iNumPedPositions, MAXNOOFPEDS);
	m_PedGenVars.m_iNumPedPositions = MAX(params.m_iNumPedPositions, 0);

	sysMemCpy(m_PedGenVars.m_PedPositions, params.m_pPedPositionsArray, sizeof(Vector3)*m_PedGenVars.m_iNumPedPositions);
	sysMemCpy(m_PedGenVars.m_PedFlags, params.m_iPedFlagsArray, sizeof(u32)*m_PedGenVars.m_iNumPedPositions);
	
	sysMemCpy(&m_PedGenVars.m_PopGenShape, &popGenShape, sizeof(CPopGenShape));
}


//--------------------------------------------------------------------------
//
// CPathServerMultiplayerPedGen
//
//


CPathServerOpponentPedGen::CPathServerOpponentPedGen()
{
	m_vSearchOrigin.Zero();
	m_fSearchRadiusXY = 0.0f;
	m_fDistZ = 0.0f;

	m_iState = STATE_IDLE;

	m_PedGenLoadedMeshes.Reserve(512);
}

bool CPathServerOpponentPedGen::StartSearch(const Vector3 & vOrigin, const float fRadiusXY, const float fDistZ, const u32 iFlags, const float fMinimumSpacing, const u32 iTimeOutMs)
{
#if __BANK
	Displayf("CPathServerOpponentPedGen::StartSearch() - cylinder (%.1f,%.1f,%.1f), %.1f, %.1f, %u, %.1f",
		vOrigin.x, vOrigin.y, vOrigin.z, fRadiusXY, fDistZ,
		iFlags, fMinimumSpacing);
#endif

	Assertf(m_iState == STATE_IDLE, "You cannot start a search when one is already active, or complete (but not yet cancelled");

	if(aiVerify(m_iState == STATE_IDLE))
	{
		m_iSearchVolumeType = SEARCH_VOLUME_SPHERE;
		m_vSearchOrigin = vOrigin;
		m_fSearchRadiusXY = fRadiusXY;
		m_fDistZ = fDistZ;
		m_fMinimumSpacing = fMinimumSpacing;
		m_AreaMinMax.Set(vOrigin - Vector3(m_fSearchRadiusXY, m_fSearchRadiusXY, m_fDistZ), vOrigin + Vector3(m_fSearchRadiusXY, m_fSearchRadiusXY, m_fDistZ));

		m_iSearchFlags = iFlags;
		Assertf((m_iSearchFlags & (FLAG_MAY_SPAWN_IN_INTERIOR | FLAG_MAY_SPAWN_IN_EXTERIOR))!=0, "You need to specify at least one of FLAG_MAY_SPAWN_IN_EXTERIOR and FLAG_MAY_SPAWN_IN_INTERIOR.");

		m_iNumCoords = 0;
		m_PedGenLoadedMeshes.clear();
		m_PedGenNavMeshIterator.Reset();
		m_bNavMeshesHaveChanged = true;

		m_iState = STATE_ACTIVE;

		m_iTimeOutMs = iTimeOutMs;

		return true;
	}
	return false;
}

bool CPathServerOpponentPedGen::StartSearch(const Vector3 & vAngledAreaPt1, const Vector3 & vAngledAreaPt2, const float fAngledAreaWidth, const u32 iFlags, const float fMinimumSpacing, const u32 iTimeOutMs)
{
#if __BANK
	Displayf("CPathServerOpponentPedGen::StartSearch() - angled area (%.1f,%.1f,%.1f), (%.1f,%.1f,%.1f), %.1f, %u, %.1f",
		vAngledAreaPt1.x, vAngledAreaPt1.y, vAngledAreaPt1.z,
		vAngledAreaPt2.x, vAngledAreaPt2.y, vAngledAreaPt2.z,
		fAngledAreaWidth, iFlags, fMinimumSpacing);
#endif

	Assertf(m_iState == STATE_IDLE, "You cannot start a search when one is already active, or complete (but not yet cancelled");

	if(aiVerify(m_iState == STATE_IDLE))
	{
		m_iSearchVolumeType = SEARCH_VOLUME_ANGLED_AREA;

		Vector3 vMinMax[2];
		m_SearchAngledArea = CScriptAngledArea(vAngledAreaPt1, vAngledAreaPt2, fAngledAreaWidth, vMinMax);

		// Expand minmax so we have chance of visiting floor polygons if designers have placed points on ground
		vMinMax[0].z = Min(vAngledAreaPt1.z, vAngledAreaPt2.z) - 1.0f;
		vMinMax[1].z = Max(vAngledAreaPt1.z, vAngledAreaPt2.z) + 0.5f;

		m_AreaMinMax.Set(vMinMax[0], vMinMax[1]);

		m_fMinimumSpacing = fMinimumSpacing;

		m_iSearchFlags = iFlags;
		Assertf((m_iSearchFlags & (FLAG_MAY_SPAWN_IN_INTERIOR | FLAG_MAY_SPAWN_IN_EXTERIOR))!=0, "You need to specify at least one of FLAG_MAY_SPAWN_IN_EXTERIOR and FLAG_MAY_SPAWN_IN_INTERIOR.");

		m_iNumCoords = 0;
		m_PedGenLoadedMeshes.clear();
		m_PedGenNavMeshIterator.Reset();
		m_bNavMeshesHaveChanged = true;

		m_iState = STATE_ACTIVE;

#if __BANK
		LOCK_IMMEDIATE_DATA;
		bool bAllLoaded = true;

		Vector3 vTestPos = vMinMax[0];
		const float fStep = CPathServerExtents::GetSizeOfNavMesh();

		for(vTestPos.x=vMinMax[0].x; vTestPos.x<=vMinMax[1].x; vTestPos.x+=fStep)
		{
			for(vTestPos.y=vMinMax[0].y; vTestPos.y<=vMinMax[1].y; vTestPos.y+=fStep)
			{
				const TNavMeshIndex iMesh = CPathServer::GetNavMeshIndexFromPosition(vTestPos, kNavDomainRegular);;
				CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iMesh, kNavDomainRegular);
				if(!pNavMesh)
				{
					bAllLoaded = false;
				}
			}
		}

		if(!bAllLoaded)
		{
			Displayf("CPathServerOpponentPedGen::StartSearch() - Not all navmeshes are loaded within area (%.1f,%.1f,%.1f) to (%.1f,%.1f,%.1f)", vMinMax[0].x, vMinMax[0].y, vMinMax[0].z, vMinMax[1].x, vMinMax[1].y, vMinMax[1].z);
		}
#endif

		m_iTimeOutMs = iTimeOutMs;

		return true;
	}
	return false;
}

bool CPathServerOpponentPedGen::IsSearchActive() const
{
	return (m_iState != STATE_IDLE);
}

bool CPathServerOpponentPedGen::IsSearchComplete() const
{
	Assertf(m_iState != STATE_IDLE, "Search completion state was queried, but system was idle.");

	return (m_iState == STATE_COMPLETE);
}

bool CPathServerOpponentPedGen::IsSearchFailed() const
{
	Assertf(m_iState != STATE_IDLE, "Search failed state was queried, but system was idle.");

	return (m_iState == STATE_FAILED);
}

bool CPathServerOpponentPedGen::CancelSeach()
{
#if __BANK
	Displayf("CPathServerOpponentPedGen::CancelSeach()");
#endif
	m_iState = STATE_IDLE;
	m_iNumCoords = 0;

	return true;
}

s32 CPathServerOpponentPedGen::GetNumResults() const
{
#if __BANK
	Displayf("CPathServerOpponentPedGen::GetNumResults() - %i", m_iNumCoords);
#endif
	Assertf(m_iState == STATE_COMPLETE, "Cannot retrieve number of results when the search is idle");
	if(aiVerify(m_iState == STATE_COMPLETE))
	{
		return m_iNumCoords;
	}
	return 0;
}

bool CPathServerOpponentPedGen::GetResult(const s32 i, Vector3 & vPos) const
{
	Assertf(m_iState==STATE_COMPLETE, "Cannot retrieve a result when the search is idle");
	Assertf(i < m_iNumCoords, "Attempt to retrieve result %i, but we only have %i results", i, m_iNumCoords);
	Assert(i >= 0);

	if(aiVerify(m_iState==STATE_COMPLETE && i<m_iNumCoords))
	{
		vPos = m_SpawnCoords[i].m_vPos;
		return true;
	}

	vPos.Zero();
	return false;
}

bool CPathServerOpponentPedGen::GetResultFlags(const s32 i, u32 & iFlags) const
{
	Assertf(m_iState==STATE_COMPLETE, "Cannot retrieve result flags when the search is idle");
	Assertf(i < m_iNumCoords, "Attempt to retrieve result flags %i, but we only have %i results", i, m_iNumCoords);
	Assert(i >= 0);

	if(aiVerify(m_iState==STATE_COMPLETE && i<m_iNumCoords))
	{
		iFlags = m_SpawnCoords[i].m_iFlags;
		return true;
	}

	iFlags = 0;
	return false;
}

void CPathServerOpponentPedGen::FindSpawnCoordsBackgroundTask()
{
	if(m_iState != STATE_ACTIVE)
		return;

	//********************************************************************
	//	Try to enter the critical section which protects the navmesh data.

	LOCK_NAVMESH_DATA;

	// Take a copy of the loaded meshes list
	{
		LOCK_STORE_LOADED_MESHES;

		aiNavMeshStore *pStore = CPathServer::GetNavMeshStore(kNavDomainRegular);
		Assert(pStore);
		const atArray<s32> & loadedMeshes = pStore->GetLoadedMeshes();
		m_PedGenLoadedMeshes.CopyFrom( loadedMeshes.GetElements(), (u16) loadedMeshes.GetCount() );

#ifndef GTA_ENGINE
		UNLOCK_STORE_LOADED_MESHES;
#endif
	}

	// If loaded navmeshes have changed then start again with a new timeslice
	
	if( m_bNavMeshesHaveChanged )
	{
#if __BANK
		Displayf("CPathServerOpponentPedGen::FindSpawnCoordsBackgroundTask - m_bNavMeshesHaveChanged, starting new timeslice");
#endif
		FindSpawnCoords_StartNewTimeSlice(m_PedGenLoadedMeshes);
		m_iNumCoords = 0;
	}

#if __BANK
	Displayf("CPathServerOpponentPedGen::FindSpawnCoordsBackgroundTask() - iNumCoords: %i, iCurrentNavmesh: %i/%i, iNumRemaningInNavmesh: %i, iTotalPolys: %i",
		m_iNumCoords,
		m_PedGenNavMeshIterator.GetCurrentIndex(),
		m_PedGenNavMeshIterator.GetNumEntries(),
		m_PedGenNavMeshIterator.GetNumRemainingInNavmesh(),
		m_PedGenNavMeshIterator.GetTotalNumPolys()
		);
#endif

	if( m_iNumCoords < ms_iMaxCoords && !CPathServer::m_bForceAbortCurrentRequest )
	{
		FindSpawnCoords_ContinueTimeSlice();
	}

	if(m_iState == STATE_FAILED)
	{
#if __BANK
		Displayf("CPathServerOpponentPedGen::FindSpawnCoordsBackgroundTask - STATE_FAILED");
#endif
		return;
	}

	if( CPathServer::m_bForceAbortCurrentRequest )
	{
#if __BANK
		Displayf("CPathServerOpponentPedGen::FindSpawnCoordsBackgroundTask - forced abort current search, restarting next update");
#endif
		// If the search was aborted - perhaps due to the streaming having to place a navmesh resource,
		// then ensure that we don't complete the search.  Instead we flag it to be restarted next update.
		m_bNavMeshesHaveChanged = true;
	}
	else
	{
		// If iteration has completed the final timeslice, the go to complete state
		if( m_PedGenNavMeshIterator.IsAtEnd() || m_iNumCoords == ms_iMaxCoords )
		{
#if __BANK
			Displayf("CPathServerOpponentPedGen::FindSpawnCoordsBackgroundTask - STATE_COMPLETE");
#endif
			m_iState = STATE_COMPLETE;
		}
		else
		{
#if __BANK
			Displayf("CPathServerOpponentPedGen::FindSpawnCoordsBackgroundTask - IsAtEnd(): %s, (%i/%i)", m_PedGenNavMeshIterator.IsAtEnd() ? "true":"false", m_iNumCoords, ms_iMaxCoords);
#endif
		}
	}
}

void CPathServerOpponentPedGen::FindSpawnCoords_StartNewTimeSlice(const atArray<s32> & loadedMeshes)
{
	m_iSearchStartTime = fwTimer::GetTimeInMilliseconds();

	m_bNavMeshesHaveChanged = false;
	m_PedGenNavMeshIterator.Reset();

	aiNavMeshStore * pStore = CPathServer::GetNavMeshStore(kNavDomainRegular);
	Assert(pStore);

	for(int m=0; m<loadedMeshes.GetCount(); m++)
	{
		const s32 iMesh = loadedMeshes[m];

		if(iMesh != pStore->GetMeshIndexNone())
		{
			CNavMesh * pNavMesh = pStore->GetMeshByIndex(iMesh);

			if(pNavMesh && m_AreaMinMax.IntersectsXY(pNavMesh->GetQuadTree()->m_MinMax))
			{
				m_PedGenNavMeshIterator.AddNavMesh(pNavMesh);
			}
		}
	}

	m_PedGenNavMeshIterator.StartNewInteration(100);
}

void CPathServerOpponentPedGen::FindSpawnCoords_ContinueTimeSlice()
{
	Assert( m_iSearchVolumeType==SEARCH_VOLUME_SPHERE || m_iSearchVolumeType==SEARCH_VOLUME_ANGLED_AREA );

	static const ScalarV fPenaltyXY( V_ONE );
	static const ScalarV fPenaltyZ( V_THREE );

	ScalarV fSearchRadiusXY( m_fSearchRadiusXY );
	ScalarV fSearchRadiusSqrXY( m_fSearchRadiusXY*m_fSearchRadiusXY );
	ScalarV fMaxDistZ( m_fDistZ );

	Vector3 vPolyPoint, vPolyMin, vPolyMax;
	CNavMesh * pNavMesh = NULL;
	TNavMeshPoly * pPoly = NULL;

	Vector3 vPolyPoints[NAVMESHPOLY_MAX_NUM_VERTICES];
	Vector3 vPointsInPoly[NUM_CLOSE_PTS_IN_POLY];

	Vector3 vCoverEdgeSegA;
	Vector3 vCoverEdgeSegAtoB;

	while(!CPathServer::m_bForceAbortCurrentRequest && m_PedGenNavMeshIterator.GetNext(&pNavMesh, &pPoly))
	{
		if(m_iTimeOutMs != 0)
		{
			s32 iDeltaMS = fwTimer::GetTimeInMilliseconds() - m_iSearchStartTime;
			if(iDeltaMS > m_iTimeOutMs)
			{
#if __BANK
				Displayf("CPathServerOpponentPedGen::FindSpawnCoords_ContinueTimeSlice - exceeded %i ms, setting state to STATE_FAILED with %i coords found", m_iTimeOutMs, m_iNumCoords);
				Displayf("iNumCoords: %i, iCurrentNavmesh: %i/%i, iNumRemaningInNavmesh: %i, iTotalPolys: %i",
					m_iNumCoords,
					m_PedGenNavMeshIterator.GetCurrentIndex(),
					m_PedGenNavMeshIterator.GetNumEntries(),
					m_PedGenNavMeshIterator.GetNumRemainingInNavmesh(),
					m_PedGenNavMeshIterator.GetTotalNumPolys()
					);
#endif
				m_iState = STATE_FAILED;
				return;
			}
		}

		// Prohibit visiting polys not marked as "network spawn candidate", unless explicitly allowed to
		if(!pPoly->GetIsNetworkSpawnCandidate() && ((m_iSearchFlags & FLAG_ALLOW_NOT_NETWORK_SPAWN_CANDIDATE_POLYS)==0))
			continue;

		// Prohibit visiting polys marked as "isolated", unless explicitly allowed
		if(pPoly->GetIsIsolated() && (m_iSearchFlags & FLAG_ALLOW_ISOLATED_POLYS)==0)
			continue;

		// Prohibit visiting polys marked as "road", unless explicitly allowed
		if(pPoly->GetIsRoad() && (m_iSearchFlags & FLAG_ALLOW_ROAD_POLYS)==0)
			continue;


		if( (pPoly->GetIsInterior() && ((m_iSearchFlags & FLAG_MAY_SPAWN_IN_INTERIOR)!=0)) ||
			(!pPoly->GetIsInterior() && ((m_iSearchFlags & FLAG_MAY_SPAWN_IN_EXTERIOR)!=0)) )
		{
			if(m_AreaMinMax.Intersects(pPoly->m_MinMax))
			{
				s32 p;

				// Obtain polygon point
				for(p=0; p<pPoly->GetNumVertices(); p++)
					pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, p), vPolyPoints[p]);

				// Create candidate points within polygon
				const s32 iNumPoints = CreatePointsInPolyNormal(pNavMesh, pPoly, vPolyPoints, POINTSINPOLY_NORMAL, vPointsInPoly, true);

				// If only points against edges, obtain plane eq of longest cover edge (if any)
				if((m_iSearchFlags & FLAG_ONLY_POINTS_AGAINST_EDGES)!=0)
				{
					static const float fMinLengthSqr = 1.0f*1.0f;
					float fLongsetEdgeSqr = -1.0f;
					s32 iLongestEdge = -1;

					for(p=0; p<pPoly->GetNumVertices(); p++)
					{
						const TAdjPoly & adjPoly = pNavMesh->GetAdjacentPoly( pPoly->GetFirstVertexIndex() + p );
						if(adjPoly.GetEdgeProvidesCover())
						{
							const float fEdgeLenSqr = (vPolyPoints[(p+1)%pPoly->GetNumVertices()] - vPolyPoints[p]).Mag2();
							if(fEdgeLenSqr > fMinLengthSqr && fEdgeLenSqr > fLongsetEdgeSqr)
							{
								fLongsetEdgeSqr = fEdgeLenSqr;
								iLongestEdge = p;
							}
						}
					}

					// If FLAG_ONLY_POINTS_AGAINST_EDGES was specified and this polygon has no cover edges, then quit out for this polygon
					if(iLongestEdge==-1)
						continue;

					// Store off the edge segment vertices
					vCoverEdgeSegA = vPolyPoints[iLongestEdge];
					vCoverEdgeSegAtoB = vPolyPoints[(iLongestEdge+1)%pPoly->GetNumVertices()] - vCoverEdgeSegA;
				}

				for(p=0; p<iNumPoints; p++)
				{
					vPolyPoint = vPointsInPoly[p];

					// Check for optional minimum spacing between points
					if(m_fMinimumSpacing > 0.0f && !CheckSpacing(vPolyPoint))
						continue;

					// Calculate optional score for proximity to a cover edge
					static dev_float fCoverEdgeWeighting = 1.0f;
					static dev_float fCoverEdgeCutoffDist = 2.0f;
					static dev_float fCoverEdgeFalloffDist = 1.0f;
					ScalarV fEdgeProximityScore(V_ZERO);
					if((m_iSearchFlags & FLAG_ONLY_POINTS_AGAINST_EDGES)!=0)
					{
						const float fDistToEdgeSeg = geomDistances::DistanceSegToPoint(vCoverEdgeSegA, vCoverEdgeSegAtoB, vPolyPoint);
						// If any points are beyond the cutoff distance, discard
						if(fDistToEdgeSeg > fCoverEdgeCutoffDist)
							continue;
						float fScore = fDistToEdgeSeg * fCoverEdgeWeighting;
						if(fDistToEdgeSeg > fCoverEdgeFalloffDist)
							fScore += square(fDistToEdgeSeg * fCoverEdgeWeighting);
						fEdgeProximityScore = ScalarV(fScore);
					}

					if(m_iSearchVolumeType == SEARCH_VOLUME_SPHERE)
					{
						Vec3V vToSearchOrigin = RCC_VEC3V(m_vSearchOrigin) - RCC_VEC3V(vPolyPoint);
						ScalarV fDistSqrXY = ScalarV( Vec::V3MagXYSquaredV(vToSearchOrigin.GetIntrin128()) );

						if( (fDistSqrXY < fSearchRadiusSqrXY).Getb() )
						{
							ScalarV fDistXY = SqrtFast(fDistSqrXY);

							ScalarV fDiffZ = Abs(vToSearchOrigin.GetZ());
							ScalarV fRatioXY = fDistXY / fSearchRadiusXY;

							if( (fDiffZ < fMaxDistZ).Getb() )
							{
								u32 iFlags = 0;
								if(pPoly->GetIsPavement())
									iFlags |= TSpawnPosition::FLAG_PAVEMENT;
								if(pPoly->GetIsInterior())
									iFlags |= TSpawnPosition::FLAG_INTERIOR;

								ScalarV fScore = (fRatioXY * fPenaltyXY) + (fDiffZ * fPenaltyZ);
								fScore += fEdgeProximityScore;
								TSpawnPosition pos(vPolyPoint, fScore.Getf(), iFlags);
								InsertSpawnPos(pos);
							}
						}
					}
					else if(m_iSearchVolumeType == SEARCH_VOLUME_ANGLED_AREA)
					{
						if( m_SearchAngledArea.TestPoint(vPolyPoint) )
						{
							u32 iFlags = 0;
							if(pPoly->GetIsPavement())
								iFlags |= TSpawnPosition::FLAG_PAVEMENT;
							if(pPoly->GetIsInterior())
								iFlags |= TSpawnPosition::FLAG_INTERIOR;

							// Maybe we can get away without calculating a score for how suitable a point is wrt an angled area
							// If scripters use the desired spacing param then can hopefully spread points out enough
							const ScalarV fScore( fwRandom::GetRandomNumberInRange(0.0f, 1.0f) );
							TSpawnPosition pos(vPolyPoint, (fScore + fEdgeProximityScore).Getf(), iFlags);
							InsertSpawnPos(pos);
						}
					}
				}
			}
		}
	}
}

bool CPathServerOpponentPedGen::CheckSpacing(const Vector3 & vPos) const
{
	const float fSpacingSqr = m_fMinimumSpacing*m_fMinimumSpacing;
	for(s32 i=0; i<m_iNumCoords; i++)
	{
		if((m_SpawnCoords[i].m_vPos - vPos).Mag2() < fSpacingSqr)
			return false;
	}
	return true;
}

bool CPathServerOpponentPedGen::InsertSpawnPos(const CPathServerOpponentPedGen::TSpawnPosition & pos)
{
	Assertf( !pos.m_vPos.IsClose(VEC3_ZERO, 0.01f), "spawn-point being added at the origin.  probably an error.." );

	for(s32 i=0; i<m_iNumCoords; i++)
	{
		if(pos.m_fScore < m_SpawnCoords[i].m_fScore)
		{
			for(s32 d=m_iNumCoords-1; d>=i; d--)
			{
				m_SpawnCoords[d+1] = m_SpawnCoords[d];
			}

			m_SpawnCoords[i] = pos;

			if(m_iNumCoords < ms_iMaxCoords)
				m_iNumCoords++;
			return true;
		}
	}
	if(m_iNumCoords < ms_iMaxCoords)
	{
		m_SpawnCoords[m_iNumCoords++] = pos;
		return true;
	}
	return false;
}


//--------------------------------------------------------------------------
//
// Ped-generation methods within CPathServer
//
//

void CPathServer::ForcePerformThreadHousekeeping(bool b)
{
	m_PathServerThread.m_bForcePerformThreadHousekeeping = b;
}

void CPathServer::SwitchPedsOnInArea(const Vector3 & vMin, const Vector3 & vMax, scrThreadId iScriptUID)
{
	SwitchNavMeshInArea(vMin, vMax, SWITCH_NAVMESH_ENABLE_AMBIENT_PEDS, iScriptUID);
}


void CPathServer::SwitchPedsOffInArea(const Vector3 & vMin, const Vector3 & vMax, scrThreadId iScriptUID)
{
	SwitchNavMeshInArea(vMin, vMax, SWITCH_NAVMESH_DISABLE_AMBIENT_PEDS, iScriptUID);

	CPed::Pool * pPedPool = CPed::GetPool();
	s32 iPoolSize = pPedPool->GetSize();
	while(iPoolSize--)
	{
		CPed * pPed = pPedPool->GetSlot(iPoolSize);
		if(pPed)
		{
			pPed->GetPedIntelligence()->RestartNavigationThroughRegion(vMin, vMax);
		}
	}
}

void CPathServer::DisableNavMeshInArea(const Vector3 & vMin, const Vector3 & vMax, scrThreadId iScriptUID)
{
	SwitchNavMeshInArea(vMin, vMax, SWITCH_NAVMESH_DISABLE_POLYGONS, iScriptUID);

	CPed::Pool * pPedPool = CPed::GetPool();
	s32 iPoolSize = pPedPool->GetSize();
	while(iPoolSize--)
	{
		CPed * pPed = pPedPool->GetSlot(iPoolSize);
		if(pPed)
		{
			pPed->GetPedIntelligence()->RestartNavigationThroughRegion(vMin, vMax);
		}
	}
}


void CPathServer::ReEnableNavMeshInArea(const Vector3 & vMin, const Vector3 & vMax, scrThreadId iScriptUID)
{
	SwitchNavMeshInArea(vMin, vMax, SWITCH_NAVMESH_ENABLE_POLYGONS, iScriptUID);
}

//**********************************************************************************
//	SwitchPedsInArea
//	Switches on/off ped generation & wandering in the given area.
//	This is done by resetting/setting the NAVMESHPOLY_SWITCHED_OFF_FOR_AMBIENT_PEDS
//	flag for navmesh polys whose centroid lies within the region.
//	Regions are remembered so that they can persist even if the streaming system
//	evicts/reloads a navmesh.  If a region completely encompasses any existing
//	region, then the existing region is removed.
//	The function ClearAllPedSwitchAreas() completely removes all areas, and resets
//	the NAVMESHPOLY_SWITCHED_OFF_FOR_AMBIENT_PEDS flag for all polys in loaded navmeshes.
//**********************************************************************************

void CPathServer::SwitchNavMeshInArea(const Vector3 & vMin, const Vector3 & vMax, ESWITCH_NAVMESH_POLYS eSwitch, scrThreadId iScriptUID)
{
	Assert( !((eSwitch&SWITCH_NAVMESH_ENABLE_AMBIENT_PEDS) && (eSwitch&SWITCH_NAVMESH_DISABLE_AMBIENT_PEDS)) );
	Assert( !((eSwitch&SWITCH_NAVMESH_ENABLE_POLYGONS) && (eSwitch&SWITCH_NAVMESH_DISABLE_POLYGONS)) );

	// Try to enter the critical section
	LOCK_NAVMESH_DATA;
	LOCK_IMMEDIATE_DATA;
	LOCK_STORE_LOADED_MESHES;

	// These area switches only apply to the regular nav domain at this time.
	aiNavMeshStore* pStore = m_pNavMeshStores[kNavDomainRegular];

	u32 n;
	// Go through all the navmeshes which are loaded, and apply the changes.
	// This could be slightly more optimal, but it will not be called frequently so never mind..
	for(n=0; n<pStore->GetMaxMeshIndex(); n++)
	{
		CNavMesh * pNavMesh = pStore->GetMeshByIndex(n);
		if(pNavMesh)
		{
			// Switch all the polys in the region
			pNavMesh->SwitchPolysInArea(vMin, vMax, eSwitch);
		}
	}
#if __HIERARCHICAL_NODES_ENABLED
	for(n=0; n<m_pNavNodesStore->GetMaxMeshIndex(); n++)
	{
		CHierarchicalNavData * pNavNodes = CPathServer::GetHierarchicalNavFromNavMeshIndex(n);
		if(pNavNodes)
		{
			pNavNodes->SwitchPedSpawningInArea(vMin, vMax, (eSwitch==SWITCH_NAVMESH_ENABLE_AMBIENT_PEDS));
		}
	}
#endif // __HIERARCHICAL_NODES_ENABLED

	// Go through the list of stored region switches, and see if we need to add this new region
	// or remove any old ones which are wholly contained by this new one.	
	for(s32 s=0; s<m_iNumPathRegionSwitches; s++)
	{
		TPathRegionSwitch * pRegion = &m_PathRegionSwitches[s];

		if(pRegion->m_vMin.x >= vMin.x && pRegion->m_vMin.y >= vMin.y && pRegion->m_vMin.z >= vMin.z &&
			pRegion->m_vMax.x <= vMax.x && pRegion->m_vMax.y <= vMax.y && pRegion->m_vMax.z <= vMax.z)
		{
			// Remove this region, since it is the same as (or wholly contained by) the newly added region
			for(s32 i=s; i<m_iNumPathRegionSwitches-1; i++)
			{
				m_PathRegionSwitches[i] = m_PathRegionSwitches[i+1];
			}

			m_iNumPathRegionSwitches--;

			// Decrement 's' before continuing the loop, since we've just shifted everything down by one index in the array
			s--;
		}
	}

	// Add this new switch
	Assert(m_iNumPathRegionSwitches < MAX_NUM_PATH_REGION_SWITCHES);

	m_PathRegionSwitches[m_iNumPathRegionSwitches].m_vMin = vMin;
	m_PathRegionSwitches[m_iNumPathRegionSwitches].m_vMax = vMax;
	m_PathRegionSwitches[m_iNumPathRegionSwitches].m_Switch = eSwitch;
	m_PathRegionSwitches[m_iNumPathRegionSwitches].m_iScriptUID = iScriptUID;
	m_iNumPathRegionSwitches++;

}

bool CPathServer::IsCoordInPedSwitchedOffArea(const Vector3 & vPos)
{
	for(s32 i=0; i<m_iNumPathRegionSwitches; i++)
	{
		if(m_PathRegionSwitches[i].m_Switch == SWITCH_NAVMESH_DISABLE_AMBIENT_PEDS &&
			m_PathRegionSwitches[i].m_vMax.IsGreaterOrEqualThan( vPos ) &&
			vPos.IsGreaterOrEqualThan( m_PathRegionSwitches[i].m_vMin ) )
		{
			return true;
		}
	}
	return false;
}

void CPathServer::ClearPedSwitchAreasForScript(scrThreadId iScriptUID)
{
	// Try to enter the critical section
#ifdef GTA_ENGINE
	LOCK_NAVMESH_DATA;
	LOCK_STORE_LOADED_MESHES;
#endif

	u32 n;

	// The ped switch areas only apply to the regular data set.
	const aiNavDomain domain = kNavDomainRegular;
	aiNavMeshStore* pStore = m_pNavMeshStores[domain];

	//*******************************************************************************************
	// Go through all the navmeshes which are loaded, and mark all polys therein as switched on

	for(n=0; n<pStore->GetMaxMeshIndex(); n++)
	{
		CNavMesh * pNavMesh = pStore->GetMeshByIndex(n);
		if(pNavMesh)
		{
			// Switch all polys in navmesh on
			pNavMesh->SwitchAllPolysOnOffForPeds( SWITCH_NAVMESH_ENABLE_AMBIENT_PEDS + SWITCH_NAVMESH_ENABLE_POLYGONS );
		}
	}
#if __HIERARCHICAL_NODES_ENABLED
	for(n=0; n<m_pNavNodesStore->GetMaxMeshIndex(); n++)
	{
		CHierarchicalNavData * pNavNodes = CPathServer::GetHierarchicalNavFromNavMeshIndex(n);
		if(pNavNodes)
		{
			// Switch all nodes on
			pNavNodes->SwitchPedSpawningInArea(pNavNodes->GetMin() - Vector3(1.0f,1.0f,1.0f), pNavNodes->GetMax() + Vector3(1.0f,1.0f,1.0f), true);
		}
	}
#endif // __HIERARCHICAL_NODES_ENABLED

	//*******************************************************************************************
	// Remove all regions created by this script

	for(s32 s=0; s<m_iNumPathRegionSwitches; s++)
	{
		TPathRegionSwitch * pRegion = &m_PathRegionSwitches[s];

		if(pRegion->m_iScriptUID == iScriptUID)
		{
			// Remove this region
			for(s32 i=s; i<m_iNumPathRegionSwitches-1; i++)
			{
				m_PathRegionSwitches[i] = m_PathRegionSwitches[i+1];
			}

			m_iNumPathRegionSwitches--;

			// Decrement 's' before continuing the loop, since we've just shifted everything down by one index in the array
			s--;
		}
	}

	//*******************************************************************************************
	// Go through all remaining regions, and switch peds

	for(u32 n=0; n<pStore->GetMaxMeshIndex(); n++)
	{
		CNavMesh * pNavMesh = pStore->GetMeshByIndex(n);
		if(pNavMesh)
		{
			ApplyPedAreaSwitchesToNewlyLoadedNavMesh(pNavMesh, domain);
		}
	}
}

void CPathServer::ClearPedSwitchAreas(const Vector3 & vMin, const Vector3 & vMax)
{
	// Try to enter the critical section
#ifdef GTA_ENGINE
	LOCK_NAVMESH_DATA;
	LOCK_STORE_LOADED_MESHES;
#endif

	// Reset the number of stored switches
	m_iNumPathRegionSwitches = 0;
	u32 n;

	// The ped switch areas only apply to the regular data set at this point.
	aiNavMeshStore* pStore = m_pNavMeshStores[kNavDomainRegular];

	// Go through all the navmeshes which are loaded, and apply the changes.
	// This could be slightly more optimal, but it will not be called frequently so never mind..
	for(n=0; n<pStore->GetMaxMeshIndex(); n++)
	{
		CNavMesh * pNavMesh = pStore->GetMeshByIndex(n);
		if(pNavMesh)
		{
			// Switch all the polys in the region on
			pNavMesh->SwitchPolysInArea( vMin, vMax, SWITCH_NAVMESH_ENABLE_AMBIENT_PEDS | SWITCH_NAVMESH_ENABLE_POLYGONS );
		}
	}
#if __HIERARCHICAL_NODES_ENABLED
	for(n=0; n<m_pNavNodesStore->GetMaxMeshIndex(); n++)
	{
		CHierarchicalNavData * pNavNodes = CPathServer::GetHierarchicalNavFromNavMeshIndex(n);
		if(pNavNodes)
		{
			// Switach all nodes on
			pNavNodes->SwitchPedSpawningInArea(pNavNodes->GetMin() - Vector3(1.0f,1.0f,1.0f), pNavNodes->GetMax() + Vector3(1.0f,1.0f,1.0f), true);
		}
	}
#endif // __HIERARCHICAL_NODES_ENABLED
}


void
CPathServer::ClearAllPedSwitchAreas(void)
{
	// Construct a min/max which will encompass the whole world
	Vector3 vMin(CPathServerExtents::GetSectorToWorldX(0), CPathServerExtents::GetSectorToWorldY(0), -1000.0f);
	Vector3 vMax(CPathServerExtents::GetSectorToWorldX((int)(CPathServerExtents::GetWorldWidthInSectors()+1)), CPathServerExtents::GetSectorToWorldY((int)(CPathServerExtents::GetWorldDepthInSectors()+1)), 1000.0f);

	ClearPedSwitchAreas(vMin, vMax);
}

//****************************************************************************
//	It is up to the caller to ensure that this is _only_ called within the
//	'm_NavMeshDataCriticalSection' critical section protecting the navmeshes
//****************************************************************************

void
CPathServer::ApplyPedAreaSwitchesToNewlyLoadedNavMesh(CNavMesh * pNavMesh, aiNavDomain domain)
{
	// The way things are set up now, the TPathRegionSwitches only apply to
	// the normal mesh, not the heightmesh.
	if(domain != kNavDomainRegular)
	{
		return;
	}

	for(s32 s=0; s<m_iNumPathRegionSwitches; s++)
	{
		TPathRegionSwitch * pRegion = &m_PathRegionSwitches[s];

		// Switch the polys in this navmesh which are intersecting the region.
		// ( NB : There is an early-out in the SwitchPolysInArea() function, which tests
		// the bounding areas of the region and the quadtree)
		pNavMesh->SwitchPolysInArea(pRegion->m_vMin, pRegion->m_vMax, pRegion->m_Switch);
	}
}
void
CPathServer::ApplyPedAreaSwitchesToNewlyLoadedNavNodes(CHierarchicalNavData * pNavNodes)
{
	for(s32 s=0; s<m_iNumPathRegionSwitches; s++)
	{
		TPathRegionSwitch * pRegion = &m_PathRegionSwitches[s];

		pNavNodes->SwitchPedSpawningInArea(pRegion->m_vMin, pRegion->m_vMax, (pRegion->m_Switch & SWITCH_NAVMESH_ENABLE_AMBIENT_PEDS)!=0);
	}
}
#endif	// GTA_ENGINE - from top of file


bool
CPathServer::TestNavMeshPolyUnderPosition(const Vector3 & vPosition, Vector3& pOutPos, const bool bNotIsolated, const bool ASSERT_ONLY(bClearOfDynamicObjects), aiNavDomain domain)
{
	Assertf(!bClearOfDynamicObjects, "TestNavMeshPolyUnderPosition - not yet implemented to handle the 'bClearOfDynamicObjects' flag");

#ifdef GTA_ENGINE
	sysCriticalSection navMeshImmediateModeDataCriticalSection(m_NavMeshImmediateAccessCriticalSectionToken);
#endif

	CNavMesh * pNavMesh;
	TNavMeshPoly * pPoly;
	EPathServerErrorCode eRet = CPathServer::m_PathServerThread.GetNavMeshPolyForRouteEndPoints(vPosition, NULL, pNavMesh, pPoly, &pOutPos, CPathServerThread::ms_fNormalDistBelowToLookForPoly, 0.0f, false, false, domain);
	if(eRet==PATH_NO_ERROR)
	{
		if(bNotIsolated && pPoly->GetIsIsolated())
			return false;

		return true;
	}

	return false;
}


EGetClosestPosRet
CPathServer::GetClosestPositionForPed(
	const Vector3 & vNearThisPosition,
	Vector3 & vecOut,
	const float fMaxSearchRadius,
	const u32 iFlags,
	const bool UNUSED_PARAM(bFailIfNoThreadAccess),
	const int iNumAvoidSpheres,
	const spdSphere * pAvoidSpheresArray,
	aiNavDomain GTA_ENGINE_ONLY(domain))
{
#if !__FINAL
	if(m_bDisableGetClosestPositionForPed)
	{
		vecOut = vNearThisPosition;
		return EPositionFoundOnMainMap;
	}
#endif

#ifdef GTA_ENGINE
	const bool bClearOfDynamicObjects = ((iFlags & GetClosestPos_ClearOfObjects)!=0);
	EGetClosestPosRet retVal = ENoPositionFound;
	if(bClearOfDynamicObjects)
	{
		//LOCK_NAVMESH_DATA;
		LOCK_DYNAMIC_OBJECTS_DATA;
		sysCriticalSection navMeshImmediateModeDataCriticalSection(m_NavMeshImmediateAccessCriticalSectionToken);

		retVal = GetClosestPositionForPedNoLock(domain, vNearThisPosition, vecOut, fMaxSearchRadius, iFlags, iNumAvoidSpheres, pAvoidSpheresArray);
	}
	else
	{
		//LOCK_NAVMESH_DATA;
		sysCriticalSection navMeshImmediateModeDataCriticalSection(m_NavMeshImmediateAccessCriticalSectionToken);

		retVal = GetClosestPositionForPedNoLock(domain, vNearThisPosition, vecOut, fMaxSearchRadius, iFlags, iNumAvoidSpheres, pAvoidSpheresArray);
	}

	// move position up from the navmesh to the height above the ground where the ped stands
	if(retVal != ENoPositionFound)
	{
		vecOut.z += PED_HUMAN_GROUNDTOROOTOFFSET;
	}

	return retVal;
#else
	vNearThisPosition; vecOut; fMaxSearchRadius; iNumAvoidSpheres; pAvoidSpheresArray; iFlags;
	return ENoPositionFound;
#endif
}

u32 g_iGetClosestPosFlags = 0;

bool GetClosestPosCallBack(const CNavMesh * UNUSED_PARAM(pNavMesh), const TNavMeshPoly * pPoly)
{
	if(((g_iGetClosestPosFlags & GetClosestPos_OnlyPavement)!=0) && !pPoly->GetIsPavement())
		return false;
	if(((g_iGetClosestPosFlags & GetClosestPos_OnlyNonIsolated)!=0) && pPoly->GetIsIsolated())
		return false;
	if(((g_iGetClosestPosFlags & GetClosestPos_NotInteriors)!=0) && pPoly->GetIsInterior())
		return false;
	if(((g_iGetClosestPosFlags & GetClosestPos_OnlyInteriors)!=0) && !pPoly->GetIsInterior())
		return false;
	if(((g_iGetClosestPosFlags & GetClosestPos_NotWater)!=0) && pPoly->GetIsWater())
		return false;
	if(((g_iGetClosestPosFlags & GetClosestPos_ClearOfObjects)!=0) && CPathServer::GetPathServerThread()->DoesMinMaxIntersectAnyDynamicObjectsApproximate(pPoly->m_MinMax))
		return false;
	if(((g_iGetClosestPosFlags & GetClosestPos_OnlyNetworkSpawn)!=0) && !pPoly->GetIsNetworkSpawnCandidate())
		return false;
	if(((g_iGetClosestPosFlags & GetClosestPos_NotSheltered)!=0) && pPoly->GetIsSheltered())
		return false;
	if(((g_iGetClosestPosFlags & GetClosestPos_OnlySheltered)!=0) && !pPoly->GetIsSheltered())
		return false;

	return true;
}

EGetClosestPosRet
CPathServer::GetClosestPositionForPedNoLock(const aiNavDomain domain, const Vector3 & vNearThisPosition, Vector3 & vecOut, const float fMaxSearchRadius, const u32 iFlags, const int iNumAvoidSpheres, const spdSphere * pAvoidSpheresArray)
{
	g_iGetClosestPosFlags = iFlags;

	//const bool bClearOfDynamicObjects = ((iFlags & GetClosestPos_ClearOfObjects)!=0);
	const bool bExtraThorough = ((iFlags & GetClosestPos_ExtraThorough)!=0);
	//const bool bCalledFromPathfinder = ((iFlags & GetClosestPos_CalledFromPathfinder)!=0);

	// Only update dynamic objects if required.
	// They will already have been locked by the calling function if necessary.
	// We don't do this when this function is called from within ProcessPathRequest (ie.bCalledFromPathfinder==true)
	// because in that case the positions will have been updated at the start of the pathfinding.
	/*
	// JB: Note of warning.  Since I have removed LOCK_NAVMESH_DATA call at the top of the calling FN, this can now
	// be called at any time, even during a pathrequest.  This means that objects may be flagged for re-insertion
	// into grid cells, which subsequently invalidates the grid-cells - and then asserts.  This is dangerous!
	// So I've opted to remove this call, and hope that pathfinding will be done frequently enough that the object
	// positions will always be reasonably up-to-date.  Perhaps this should be done as part of the DoHousekeeping
	// task which occurs once a second?...
	if(bClearOfDynamicObjects && !bCalledFromPathfinder)
	{
		CPathServer::GetPathServerThread()->UpdateAllDynamicObjectsPositionsNoLock();
	}
	*/

	if((iFlags & GetClosestPos_UseFloodFill)!=0)
	{
		//Assert(!bCalledFromPathfinder);
		const EGetClosestPosRet retVal = FloodFillForClosestPosition(vNearThisPosition, vecOut, fMaxSearchRadius, iFlags, iNumAvoidSpheres, pAvoidSpheresArray, domain);
		return retVal;
	}

	const Vector3 vOffsets[8] =
	{
		Vector3(-fMaxSearchRadius, 0.0f, 0.0f),
		Vector3(-fMaxSearchRadius * 0.707f, fMaxSearchRadius * 0.707f, 0.0f),
		Vector3(0.0f, fMaxSearchRadius, 0.0f),
		Vector3(fMaxSearchRadius * 0.707f, fMaxSearchRadius * 0.707f, 0.0f),
		Vector3(fMaxSearchRadius, 0.0f, 0.0f),
		Vector3(fMaxSearchRadius * 0.707f, -fMaxSearchRadius * 0.707f, 0.0f),
		Vector3(0.0f, -fMaxSearchRadius, 0.0f),
		Vector3(-fMaxSearchRadius * 0.707f, -fMaxSearchRadius * 0.707f, 0.0f)
	};

	TNavMeshIndex indices[9];
	indices[0] = GetNavMeshIndexFromPosition(vNearThisPosition, domain);
	int iNumIndices = 1;

	// Add indices of surrounding navmeshes.  Make sure that we don't store any duplicates.
	int o,o2;

	for(o=0; o<8; o++)
	{
		TNavMeshIndex index = GetNavMeshIndexFromPosition(vNearThisPosition + vOffsets[o], domain);
		for(o2=0; o2<iNumIndices; o2++)
		{
			if(indices[o2]==index)
				break;
		}
		if(o2==iNumIndices)
		{
			indices[iNumIndices++] = index;
		}
	}

	float fClosestDistSqr = FLT_MAX;
	EGetClosestPosRet retVal = ENoPositionFound;

	for(o=0; o<iNumIndices; o++)
	{
		CNavMesh * pNavMesh = GetNavMeshFromIndex(indices[o], domain);
		if(pNavMesh)
		{
			Vector3 vClosestPosInThisNavMesh;
			TNavMeshPoly * pClosestPoly;
			
			if(bExtraThorough)
			{
				pClosestPoly = pNavMesh->GetClosestPointInPoly(vNearThisPosition, fMaxSearchRadius, vClosestPosInThisNavMesh, GetClosestPosCallBack, NULL, iFlags, iNumAvoidSpheres, pAvoidSpheresArray);
			}
			else
			{
				pClosestPoly = pNavMesh->GetClosestNavMeshPolyEdge(vNearThisPosition, fMaxSearchRadius, vClosestPosInThisNavMesh, GetClosestPosCallBack, NULL, true, iFlags, iNumAvoidSpheres, pAvoidSpheresArray);
			}

			if(pClosestPoly)
			{
				const float fDistSqr = (vClosestPosInThisNavMesh - vNearThisPosition).Mag2();
				if(fDistSqr < fClosestDistSqr)
				{
					fClosestDistSqr = fDistSqr;
					vecOut = vClosestPosInThisNavMesh;
					retVal = pClosestPoly->GetIsInterior() ? EPositionFoundInAnInterior : EPositionFoundOnMainMap;
				}
			}
		}
	}

	return retVal;
}


Vector3	  g_FloodFillClosest_vCentrePos;
float	  g_FloodFillClosest_fRadius;
float	  g_FloodFillClosest_fRadiusSqr;
float	  g_FloodFillClosest_fBestDistSqrSoFar;
TNavMeshIndex g_FloodFillClosest_iBestNavMeshIndex;
u32	  g_FloodFillClosest_iBestPolyIndex;
Vector3	  g_FloodFillClosest_vBestFoundPos;
TNavMeshPoly * g_FloodFillClosest_pBestFoundPoly;
u32	  g_FloodFillClosest_iFlags;
bool	  g_FloodFillClosest_bOnlyPavement;
bool	  g_FloodFillClosest_bNotIsolated;
bool	  g_FloodFillClosest_bNotInterior;
bool	  g_FloodFillClosest_bOnlyInterior;
bool	  g_FloodFillClosest_bNotWater;
bool	  g_FloodFillClosest_bNotIntersectingObjects;
bool	  g_FloodFillClosest_bNotSheltered;
bool	  g_FloodFillClosest_bOnlySheltered;
int		  g_FloodFillClosest_iNumAvoidSpheres;
const spdSphere*  g_FloodFillClosest_pAvoidSpheresArray;
TShortMinMax g_FloodFillClosest_minMax;
Vector3	  g_FloodFillClosest_vPolyPts[NAVMESHPOLY_MAX_NUM_VERTICES];
Vector3	  g_FloodFillClosest_vPtsInPoly[NUM_CLOSE_PTS_IN_POLY];

int g_FloodFillClosest_iStackPtr = 0;
#define FLOOD_FILL_FOR_CLOSEST_STACKSIZE 256
TFloodFillForClosest g_FloodFillClosest_Stack[FLOOD_FILL_FOR_CLOSEST_STACKSIZE];

EGetClosestPosRet
CPathServer::FloodFillForClosestPosition(const Vector3 & vNearThisPosition, Vector3 & vecOut, const float fMaxSearchRadius, const u32 iFlags, const int iNumAvoidSpheres, const spdSphere * pAvoidSpheresArray, aiNavDomain domain)
{
	g_FloodFillClosest_bOnlyPavement = (iFlags & GetClosestPos_OnlyPavement)!=0;
	g_FloodFillClosest_bNotIsolated = (iFlags & GetClosestPos_OnlyNonIsolated)!=0;
	g_FloodFillClosest_bNotInterior = (iFlags & GetClosestPos_NotInteriors)!=0;
	g_FloodFillClosest_bOnlyInterior = (iFlags & GetClosestPos_OnlyInteriors)!=0;
	g_FloodFillClosest_bNotWater = (iFlags & GetClosestPos_NotWater)!=0;
	g_FloodFillClosest_bNotIntersectingObjects = (iFlags & GetClosestPos_ClearOfObjects)!=0;
	g_FloodFillClosest_bNotSheltered = (iFlags & GetClosestPos_NotSheltered)!=0;
	g_FloodFillClosest_bOnlySheltered = (iFlags & GetClosestPos_OnlySheltered)!=0;

	//******************************************************************
	//	Make an array of the navmesh indices which we will need to test

	const float fMeshSize = m_pNavMeshStores[domain]->GetMeshSize();
	const float fAdjNavMesh = Min(fMaxSearchRadius, fMeshSize);

	const Vector3 vOffsets[5] =
	{
		vNearThisPosition,
		vNearThisPosition - Vector3(fAdjNavMesh, 0.0f, 0.0f),
		vNearThisPosition + Vector3(fAdjNavMesh, 0.0f, 0.0f),
		vNearThisPosition - Vector3(0.0f, fAdjNavMesh, 0.0f),
		vNearThisPosition + Vector3(0.0f, fAdjNavMesh, 0.0f)
	};

	TNavMeshIndex iMeshes[5];
	int iNumNavMeshes = 0;
	int n,n2;

	for(n=0; n<5; n++)
	{
		const TNavMeshIndex iMesh = GetNavMeshIndexFromPosition(vOffsets[n], domain);
		for(n2=0; n2<iNumNavMeshes; n2++)
		{
			if(iMeshes[n2]==iMesh)
				break;
		}
		if(n2==iNumNavMeshes)
		{
			iMeshes[iNumNavMeshes++] = iMesh;
		}
	}

	//********************************************************************
	//	Find which navmesh is directly underfoot, or closest to start-pos

	TNavMeshIndex iNavMeshIndex = NAVMESH_NAVMESH_INDEX_NONE;
	u32 iPolyIndex = NAVMESH_POLY_INDEX_NONE;
	Vector3 vIsectPos;

	for(n=0; n<iNumNavMeshes; n++)
	{
		iNavMeshIndex = iMeshes[n];
		CNavMesh * pNavMesh = GetNavMeshFromIndex(iNavMeshIndex, domain);
		if(!pNavMesh)
			continue;

		iPolyIndex = pNavMesh->GetPolyBelowPoint(vNearThisPosition, vIsectPos, 4.0f);
		if(iPolyIndex==NAVMESH_POLY_INDEX_NONE)
			continue;

		TNavMeshPoly * pPoly = pNavMesh->GetPoly(iPolyIndex);
		if(!pPoly)
			continue;

		if((g_FloodFillClosest_bOnlyPavement && !pPoly->GetIsPavement()) ||
			(g_FloodFillClosest_bNotIsolated && pPoly->GetIsIsolated()) ||
			(g_FloodFillClosest_bNotInterior && pPoly->GetIsInterior()) ||
			(g_FloodFillClosest_bOnlyInterior && !pPoly->GetIsInterior()) ||
			(g_FloodFillClosest_bNotWater && pPoly->GetIsWater()) ||
			(g_FloodFillClosest_bNotSheltered && pPoly->GetIsSheltered()) ||
			(g_FloodFillClosest_bOnlySheltered && !pPoly->GetIsSheltered()))
		{
			iPolyIndex = NAVMESH_POLY_INDEX_NONE;
			continue;
		}

		if(iPolyIndex != NAVMESH_POLY_INDEX_NONE)
		{
			// We've found a polygon exactly under this position
			break;
		}
	}

	//****************************************************************************************
	//	If we didn't find a poly directly under the input pos, then search for a closest poly

	if(iPolyIndex == NAVMESH_POLY_INDEX_NONE)
	{
		g_iGetClosestPosFlags = iFlags;

		float fClosestDistSqr = FLT_MAX;
		for(n=0; n<iNumNavMeshes; n++)
		{
			const TNavMeshIndex iIndex = iMeshes[n];
			CNavMesh * pNavMesh = GetNavMeshFromIndex(iIndex, domain);
			if(!pNavMesh)
				continue;

			TNavMeshPoly * pPoly = pNavMesh->GetClosestNavMeshPolyEdge(vNearThisPosition, fMaxSearchRadius, vIsectPos, GetClosestPosCallBack, NULL, false, iFlags);

			if(pPoly)
			{
				const u32 iPoly = pNavMesh->GetPolyIndex(pPoly);
				const float fDist2 = (vIsectPos - vNearThisPosition).Mag2();
				if(fDist2 < fClosestDistSqr)
				{
					iNavMeshIndex = iIndex;
					iPolyIndex = iPoly;
					fClosestDistSqr = fDist2;
				}
			}
		}
	}

	//*****************************************************
	// If we couldn't find any suitable poly, then quit..

	if(iPolyIndex == NAVMESH_POLY_INDEX_NONE)
	{
		return ENoPositionFound;
	}

	Assert(iNavMeshIndex != NAVMESH_NAVMESH_INDEX_NONE);

	//*****************************************************************
	// We have a starting poly, so recursively visit neighbours until
	// we have found a suitable position.

	CNavMesh * pStartingNavMesh = CPathServer::GetNavMeshFromIndex(iNavMeshIndex, domain);
	if(!pStartingNavMesh)
		return ENoPositionFound;

	TNavMeshPoly * pStartingPoly = pStartingNavMesh->GetPoly(iPolyIndex);

	g_FloodFillClosest_pBestFoundPoly		= NULL;
	g_FloodFillClosest_vCentrePos			= vNearThisPosition;
	g_FloodFillClosest_fRadius				= fMaxSearchRadius;
	g_FloodFillClosest_fRadiusSqr			= fMaxSearchRadius*fMaxSearchRadius;
	g_FloodFillClosest_iFlags				= iFlags;
	g_FloodFillClosest_iNumAvoidSpheres		= iNumAvoidSpheres;
	g_FloodFillClosest_pAvoidSpheresArray	= pAvoidSpheresArray;

	g_FloodFillClosest_minMax.SetFloat(
		vNearThisPosition.x - fMaxSearchRadius, vNearThisPosition.y - fMaxSearchRadius, vNearThisPosition.z - fMaxSearchRadius,
		vNearThisPosition.x + fMaxSearchRadius, vNearThisPosition.y + fMaxSearchRadius, vNearThisPosition.z + fMaxSearchRadius);

	g_FloodFillClosest_fBestDistSqrSoFar = FLT_MAX;
	g_FloodFillClosest_iBestNavMeshIndex = NAVMESH_NAVMESH_INDEX_NONE;
	g_FloodFillClosest_iBestPolyIndex = NAVMESH_POLY_INDEX_NONE;

	// Set up our pseudo stack which will avoid recursion & potential stack overflows
	pStartingPoly->m_iImmediateModeFlags = 1;
	m_pImmediateModeVisitedPolys[m_iImmediateModeNumVisitedPolys++] = pStartingPoly;

	g_FloodFillClosest_iStackPtr = 1;
	g_FloodFillClosest_Stack[0].pNavMesh = pStartingNavMesh;
	g_FloodFillClosest_Stack[0].pPoly = pStartingPoly;

	FloodFillForClosestPositionNonRecursive(domain);

	CPathServer::ResetImmediateModeVisitedPolys();

	if(g_FloodFillClosest_iBestPolyIndex != NAVMESH_POLY_INDEX_NONE)
	{
		vecOut = g_FloodFillClosest_vBestFoundPos;
		Assert(g_FloodFillClosest_pBestFoundPoly);
		return g_FloodFillClosest_pBestFoundPoly->GetIsInterior() ? EPositionFoundInAnInterior : EPositionFoundOnMainMap;
	}
	else
	{
		return ENoPositionFound;
	}
}


void
CPathServer::FloodFillForClosestPositionNonRecursive(aiNavDomain domain)
{

POP_STACK:

	if(g_FloodFillClosest_iStackPtr <= 0)
		return;

	if(m_iImmediateModeNumVisitedPolys >= IMMEDIATE_MODE_QUERY_MAXNUMVISITEDPOLYS)
		return;

	g_FloodFillClosest_iStackPtr--;
	CNavMesh * pNavMesh = g_FloodFillClosest_Stack[g_FloodFillClosest_iStackPtr].pNavMesh;
	TNavMeshPoly * pPoly = g_FloodFillClosest_Stack[g_FloodFillClosest_iStackPtr].pPoly;

	// By this point the poly should be marked with the current timestamp
	Assert(pNavMesh);
	Assert(pPoly);
	Assert(pPoly->GetNavMeshIndex()==pNavMesh->GetIndexOfMesh());
	Assert(pPoly->m_iImmediateModeFlags == 1);

	bool bCandidatePoly = true;

	if((g_FloodFillClosest_bOnlyPavement && !pPoly->GetIsPavement()) ||
		(g_FloodFillClosest_bNotIsolated && pPoly->GetIsIsolated()) ||
		(g_FloodFillClosest_bNotInterior && pPoly->GetIsInterior()) ||
		(g_FloodFillClosest_bNotWater && pPoly->GetIsWater()) ||
		(g_FloodFillClosest_bNotSheltered && pPoly->GetIsSheltered()) ||
		(g_FloodFillClosest_bOnlySheltered && !pPoly->GetIsSheltered()))
	{
		bCandidatePoly = false;
	}

	//*************************************************************************
	// Ok, pAdjPoly is a neighbouring poly which is within the search extents
	// and which we've not yet visited.  Generate a number of positions within.

	if(bCandidatePoly)
	{
		const int iNumVerts = pPoly->GetNumVertices();
		for(int v=0; v<iNumVerts; v++)
		{
			pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, v), g_FloodFillClosest_vPolyPts[v] );
		}

		const int iNumPositions = CreatePointsInPoly(
			pNavMesh,
			pPoly,
			g_FloodFillClosest_vPolyPts,
			POINTSINPOLY_NORMAL,
			g_FloodFillClosest_vPtsInPoly
		);

		//***********************************************************************************
		// For each of the candidate positions, see whether it is clear of all avoid spheres

		for(int p=0; p<iNumPositions; p++)
		{
			const Vector3 & vCandidatePos = g_FloodFillClosest_vPtsInPoly[p];
			const Vector3 vDiff = vCandidatePos - g_FloodFillClosest_vCentrePos;
			const float fDistSqr = vDiff.Mag2();
			if(fDistSqr > g_FloodFillClosest_fRadiusSqr)
				continue;

			if(g_FloodFillClosest_bNotIntersectingObjects)
			{
				TShortMinMax ptMinMax;
				ptMinMax.SetFloat(
					vCandidatePos.x - PATHSERVER_PED_RADIUS, vCandidatePos.y - PATHSERVER_PED_RADIUS, vCandidatePos.z - 1.5f,
					vCandidatePos.x + PATHSERVER_PED_RADIUS, vCandidatePos.y + PATHSERVER_PED_RADIUS, vCandidatePos.z + 1.5f);

				if(CPathServer::GetPathServerThread()->DoesMinMaxIntersectAnyDynamicObjectsApproximate(ptMinMax))
					continue;
			}

			const Vec3V pos = RCC_VEC3V(vCandidatePos);
			int s;
			for(s=0; s<g_FloodFillClosest_iNumAvoidSpheres; s++)
			{
				// Close in XY
				if (g_FloodFillClosest_pAvoidSpheresArray[s].ContainsPointFlat(pos))
				{
					// Closer than a ped's height
					const ScalarV dz = Abs(pos - g_FloodFillClosest_pAvoidSpheresArray[s].GetCenter()).GetZ();
					if (IsLessThanAll(dz, ScalarV(V_THREE)) != 0)
						break;
				}
			}

			//********************************************************************************************
			// if (s==g_FloodFillClosest_iNumAvoidSpheres) then we didn't intersect any avoid-spheres..

			if(s==g_FloodFillClosest_iNumAvoidSpheres)
			{
				if(fDistSqr < g_FloodFillClosest_fBestDistSqrSoFar)
				{
					g_FloodFillClosest_pBestFoundPoly = pPoly;
					g_FloodFillClosest_fBestDistSqrSoFar = fDistSqr;
					g_FloodFillClosest_iBestNavMeshIndex = pPoly->GetNavMeshIndex();
					g_FloodFillClosest_iBestPolyIndex = pNavMesh->GetPolyIndex(pPoly);
					g_FloodFillClosest_vBestFoundPos = vCandidatePos;
				}
			}
		}
	}

	//********************************
	//	Now visit the adjacent polys

	TNavMeshIndex iAdjNavMesh;

	for(u32 a=0; a<pPoly->GetNumVertices(); a++)
	{
		const TAdjPoly & adjPoly = pNavMesh->GetAdjacentPoly(pPoly->GetFirstVertexIndex() +  a);
		iAdjNavMesh = adjPoly.GetOriginalNavMeshIndex(pNavMesh->GetAdjacentMeshes());
		if(iAdjNavMesh==NAVMESH_NAVMESH_INDEX_NONE)
			continue;

		CNavMesh * pAdjNavMesh = GetNavMeshFromIndex(iAdjNavMesh, domain);
		if(!pAdjNavMesh)
			continue;

		TNavMeshPoly * pAdjPoly = pAdjNavMesh->GetPoly(adjPoly.GetOriginalPolyIndex());

		if(pAdjPoly->m_iImmediateModeFlags == 1)
			continue;

		// Have to prevent the flood-fill from ever passing over an interior polygon because
		// some artists have littered the floor of their interiors with non-interior objects. 
		// Doh! This makes things very difficult indeed when trying to rule out interiors..
		if(g_FloodFillClosest_bNotInterior && pAdjPoly->GetIsInterior())
			continue;

		if(g_FloodFillClosest_bNotWater && pAdjPoly->GetIsWater())
			continue;

		if(!pAdjPoly->m_MinMax.Intersects(g_FloodFillClosest_minMax))
			continue;

		if(g_FloodFillClosest_iStackPtr < FLOOD_FILL_FOR_CLOSEST_STACKSIZE)
		{
			if(m_iImmediateModeNumVisitedPolys >= IMMEDIATE_MODE_QUERY_MAXNUMVISITEDPOLYS)
			{
				goto POP_STACK;
			}

			pAdjPoly->m_iImmediateModeFlags = 1;
			m_pImmediateModeVisitedPolys[m_iImmediateModeNumVisitedPolys++] = pAdjPoly;


			g_FloodFillClosest_Stack[g_FloodFillClosest_iStackPtr].pNavMesh = pAdjNavMesh;
			g_FloodFillClosest_Stack[g_FloodFillClosest_iStackPtr].pPoly = pAdjPoly;
			g_FloodFillClosest_iStackPtr++;
		}
		else
		{
			goto POP_STACK;	// goto's are your friend
		}
	}

	goto POP_STACK;
}


#ifdef GTA_ENGINE

//*********************************************************************************

EGetClosestPosRet
CPathServerGta::GetClosestPositionForPedOnDynamicNavmesh(CEntity * pEntity, const Vector3 & vNearThisPosition, const float fMaxSearchRadius, Vector3 & vOut)
{
	LOCK_NAVMESH_DATA;
	LOCK_DYNAMIC_OBJECTS_DATA;

	Assert(pEntity->GetIsTypeVehicle());	// Currently only vehicles have their own navmeshes

	if(!m_iNumDynamicNavMeshesInExistence)
	{
		return ENoPositionFound;
	}

	CVehicle * pVehicle = (CVehicle*)pEntity;
	Assert(pVehicle->m_nVehicleFlags.bHasDynamicNavMesh);
	const int iModelIndex = pVehicle->GetModelIndex();

	//*********************************************************************************************************************
	// Iterate through list until we find the entity.  We could have an index in with each vehicle, but there's likely to
	// be so few entities which do own navmeshes thats its probably just a waste of memory.

	const CModelInfoNavMeshRef * pRef = m_DynamicNavMeshStore.GetByModelIndex(iModelIndex);
	Assert(pRef->m_pBackUpNavmeshCopy);

	if(!pRef->m_pBackUpNavmeshCopy)
		return ENoPositionFound;

	CNavMesh * pNavMesh = pRef->m_pBackUpNavmeshCopy;
	pNavMesh->SetMatrix( MAT34V_TO_MATRIX34(pEntity->GetMatrix()) );

	if(!pNavMesh)
	{
		return ENoPositionFound;
	}

	EGetClosestPosRet retVal = ENoPositionFound;
	float fClosestDistSqr = FLT_MAX;
	Vector3 vClosestPosInThisNavMesh;

	TNavMeshPoly * pClosestPoly = pNavMesh->GetClosestNavMeshPolyEdge(vNearThisPosition, fMaxSearchRadius, vClosestPosInThisNavMesh);
	if(pClosestPoly)
	{
		float fDistSqr = (vClosestPosInThisNavMesh - vNearThisPosition).Mag2();
		if(fDistSqr < fClosestDistSqr)
		{
			fClosestDistSqr = fDistSqr;
			vOut = vClosestPosInThisNavMesh;
			retVal = pClosestPoly->GetIsInterior() ? EPositionFoundInAnInterior : EPositionFoundOnMainMap;
		}
	}

	return retVal;
}

static const float fClearPosRadius = 1.0f;
static bool g_bThisNodeIsClear = true;

bool IsPositionClearCB(CEntity* pEntity, void* data)
{
	spdSphere * pSphere = (spdSphere*)data;

	// Just a simple radius check for now (maybe no need for this, since this will be the reason why this callback was called anyway)
	// [SPHERE-OPTIMISE]
	const Vec3V vDiff = pSphere->GetCenter() - pEntity->GetTransform().GetPosition();
	const float fCombinedRadius = fClearPosRadius + pEntity->GetBoundRadius();
	if(MagSquared(vDiff).Getf() < fCombinedRadius*fCombinedRadius)
	{
		g_bThisNodeIsClear = false;
		return false;
	}
	return true;
}

// Finds a car node close to the input position which is clear of nearby objects, vehicles, etc.
bool CPathServer::GetClosestClearCarNodeForPed(const Vector3 & vNearThisPosition, Vector3 & vecOut, const float fMaxSearchRadius)
{
	static const int iNumTries = 50;
	
	for(int n=0; n<iNumTries; n++)
	{
		const CNodeAddress nodeAddr = ThePaths.FindNthNodeClosestToCoors(vNearThisPosition, fMaxSearchRadius, false, n);
		if(nodeAddr.IsEmpty())
			continue;

		const CPathNode * pNode = ThePaths.FindNodePointer(nodeAddr);
		if(!pNode)
			continue;

		Vector3 vPos;
		pNode->GetCoors(vPos);

		// Now check that this node position is clear of objects & vehicles.
		g_bThisNodeIsClear = true;
		fwIsSphereIntersecting cullSphere(spdSphere(RCC_VEC3V(vPos), ScalarV(fClearPosRadius)));
		CGameWorld::ForAllEntitiesIntersecting(&cullSphere, IsPositionClearCB, &cullSphere, ENTITY_TYPE_MASK_VEHICLE|ENTITY_TYPE_MASK_OBJECT|ENTITY_TYPE_MASK_DUMMY_OBJECT, SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS);

		if(g_bThisNodeIsClear)
		{
			vecOut = vPos;
			return true;
		}
	}

	return false;
}



#if __TRACK_PEDS_IN_NAVMESH
void
CPathServer::UpdatePedTracking()
{
#if !__FINAL
	m_TrackObjectsTimer->Reset();
	m_TrackObjectsTimer->Start();
#endif

	// Wait for any streaming, etc access to navmeshes to complete
	CHECK_FOR_THREAD_STALLS(m_NavMeshImmediateAccessCriticalSectionToken);
	sysCriticalSection navMeshImmediateModeDataCriticalSection(m_NavMeshImmediateAccessCriticalSectionToken);

#if !__FINAL
	m_TrackObjectsTimer->Stop();
	m_fTimeToLockDataForTracking = m_TrackObjectsTimer->GetTimeMS();
	m_TrackObjectsTimer->Reset();
	m_TrackObjectsTimer->Start();
#endif

	PF_FUNC(UpdatePedNavMeshTracking);
	PF_START_TIMEBAR("UpdatePedNavMeshTracking");

	// Update the tracking for all the peds
	CPed::Pool * pPool = CPed::GetPool();
	for(int p=0; p<pPool->GetSize(); p++)
	{
		CPed * pPed = pPool->GetSlot(p);
		if(pPed)
		{
			CNavMeshTrackedObject & navMeshTracker = pPed->GetNavMeshTracker();

			// Peds carrying portable pickups must always track the nav mesh as this is needed to keep track of whether the pickup is in an accessible location or not
			if(pPed->GetIsInVehicle() && !pPed->IsPlayer() && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_HasPortablePickupAttached))
			{
				navMeshTracker.SetInvalid();
				continue;
			}

			const Vector3 vPreviousPos = navMeshTracker.GetLastPosition();
			const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

			// If the position of the object has changed over some threshold we will rescan
			bool bNeedToReScan = TrackedObjectNeedsRescan(navMeshTracker, vPedPos);

			// Check to see if we should skip this update
			if( !bNeedToReScan && !CPedAILodManager::ShouldDoNavMeshTrackerUpdate(*pPed) )
			{
				continue;
			}

			const bool bIsOnDynamicNavmesh = pPed->GetGroundPhysical() && pPed->GetGroundPhysical()->GetIsTypeVehicle() && ((CVehicle*)pPed->GetGroundPhysical())->m_nVehicleFlags.bHasDynamicNavMesh;

			// Peds on dynamic navmeshes don't track their position on dynamic navmeshes; ensure position is set to invalid.
			if(bIsOnDynamicNavmesh)
			{
				navMeshTracker.SetInvalid();
			}
			// Regular tracking update for peds using physics
			else
			{
				UpdateTrackedObject(navMeshTracker, vPedPos, CNavMeshTrackedObject::DEFAULT_RESCAN_PROBE_LENGTH, bNeedToReScan);
			}

			CNavMesh * pNavMesh = GetNavMeshFromIndex(navMeshTracker.m_NavMeshAndPoly.m_iNavMeshIndex, kNavDomainRegular);
			if(pNavMesh)
			{
				TNavMeshPoly * pStartPoly = pNavMesh->GetPoly(navMeshTracker.m_NavMeshAndPoly.m_iPolyIndex);

				// Calculate polygon normal
				const dev_float fUpdateNormalDeltaSrqXY = 0.0f; //0.125f * 0.125f;
				if((vPreviousPos - vPedPos).XYMag2() >= fUpdateNormalDeltaSrqXY)
				{
					if( !pNavMesh->GetPolyNormal(navMeshTracker.m_vPolyNormal, pStartPoly) )
						navMeshTracker.m_vPolyNormal.Zero();
				}

				const u32 iTime = fwTimer::GetTimeInMilliseconds();
				for ( CNavMeshTrackedObject::NavMeshLosChecksArray::iterator j = navMeshTracker.m_NavMeshLosChecks.begin(); j != navMeshTracker.m_NavMeshLosChecks.end();  )
				{
					static u32 s_Maxtime = 1500;
					if ( iTime - (*j).m_iTimeChecked > s_Maxtime )
					{
						j = navMeshTracker.m_NavMeshLosChecks.erase(j);
					}						
					else
					{
						j++;
					}
				}
				
				for ( int i = 0; i < navMeshTracker.m_NavMeshLosRequests.GetCount(); i++ )
				{
					CNavMeshTrackedObject::sNavMeshLosCheck check;
					check.m_iTimeChecked = iTime;
					if ( CPathServer::TestShortLineOfSightImmediate_RG(check.m_Vertex1, check.m_Vertex2, navMeshTracker.m_vLastPosition
						, navMeshTracker.m_vLastPosition + navMeshTracker.m_NavMeshLosRequests[i], *pStartPoly, kNavDomainRegular) )
					{
						bool insert = true;

						// check for same edge
						for ( int j = 0; j < navMeshTracker.m_NavMeshLosChecks.GetCount(); j++ )
						{
							if ( navMeshTracker.m_NavMeshLosChecks[j].m_Vertex1 == check.m_Vertex1
								&& navMeshTracker.m_NavMeshLosChecks[j].m_Vertex2 == check.m_Vertex2 )
							{			
								insert = false;
								navMeshTracker.m_NavMeshLosChecks[j].m_iTimeChecked = iTime;
							}						
						}

						if ( insert )
						{
							if ( ! navMeshTracker.m_NavMeshLosChecks.GetAvailable() )
							{								
								int oldestIndex = -1;
								u32 oldestTime = UINT_MAX;
								for ( int j = 0; j < navMeshTracker.m_NavMeshLosChecks.GetCount(); j++ )
								{
									if ( navMeshTracker.m_NavMeshLosChecks[j].m_iTimeChecked < oldestTime )
									{
										oldestIndex = j;
										oldestTime = navMeshTracker.m_NavMeshLosChecks[j].m_iTimeChecked;
									}						
								}

								if ( oldestIndex < navMeshTracker.m_NavMeshLosChecks.GetCount() )
								{
									navMeshTracker.m_NavMeshLosChecks.DeleteFast(oldestIndex);
								}
							}							
							navMeshTracker.m_NavMeshLosChecks.Push(check);
						}
					}
					CPathServer::ResetImmediateModeVisitedPolys();	
				}
				navMeshTracker.m_NavMeshLosRequests.Reset();
			}

			if(navMeshTracker.m_bAvoidanceLeftIsBlocked != 0)
			{
				navMeshTracker.m_bAvoidanceLeftIsBlocked--;
			}
			if(navMeshTracker.m_bAvoidanceRightIsBlocked != 0)
			{
				navMeshTracker.m_bAvoidanceRightIsBlocked--;
			}
			if(navMeshTracker.m_bLosAheadIsBlocked != 0)
			{
				navMeshTracker.m_bLosAheadIsBlocked--;
			}

			if(navMeshTracker.m_bPerformAvoidanceLineTests)
			{
				//static dev_float fProbeDist = 0.35f;
				float fVelMagSqr = pPed->GetVelocity().XYMag2();
				if(fVelMagSqr > 0.01f)
				{
					float fVelMag = sqrt(fVelMagSqr);
					float fProbeDistance = Lerp( fVelMag / MOVEBLENDRATIO_SPRINT, CTaskMoveGoToPoint::ms_fPedAvoidanceNavMeshLineProbeDistanceMin, CTaskMoveGoToPoint::ms_fPedAvoidanceNavMeshLineProbeDistanceMax );
					Vector2 vInitialTestVec(0.0f, fProbeDistance);

					navMeshTracker.m_bAvoidanceLeftIsBlocked = 2;
					navMeshTracker.m_bAvoidanceRightIsBlocked = 2;

					if(navMeshTracker.m_NavMeshAndPoly.m_iNavMeshIndex != NAVMESH_NAVMESH_INDEX_NONE && navMeshTracker.m_NavMeshAndPoly.m_iPolyIndex != NAVMESH_POLY_INDEX_NONE)
					{
						CNavMesh * pNavMesh = GetNavMeshFromIndex(navMeshTracker.m_NavMeshAndPoly.m_iNavMeshIndex, kNavDomainRegular);
						if(pNavMesh)
						{							
							Vector3 vIntersectPos;
							const float fAvoidanceRotation = CTaskMoveGoToPoint::ms_fPedAvoidanceNavMeshLineProbeAngle * DtoR;

							TNavMeshPoly * pStartPoly = pNavMesh->GetPoly(navMeshTracker.m_NavMeshAndPoly.m_iPolyIndex);
			
							// Test to the left
							Vector2 vTestVec = vInitialTestVec;
							vTestVec.Rotate( pPed->GetCurrentHeading() + TWO_PI + fAvoidanceRotation );					
							TNavMeshPoly * pEndPoly = CPathServer::TestShortLineOfSightImmediate(navMeshTracker.m_vLastPosition, vTestVec, pStartPoly, &vIntersectPos, NULL, NULL, kNavDomainRegular);
							if(pEndPoly)
								navMeshTracker.m_bAvoidanceLeftIsBlocked = 0;

#if __BANK && defined(DEBUG_DRAW)
							if(CNavMeshTrackedObject::ms_bDrawPedTrackingProbes)
							{
								grcDebugDraw::Line(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())+Vector3(vTestVec.x,vTestVec.y,0.0f), Color_white, pEndPoly ? Color_green : Color_red);
							}
#endif

							// Test to the right
							vTestVec = vInitialTestVec;
							vTestVec.Rotate( pPed->GetCurrentHeading() + TWO_PI - fAvoidanceRotation );					
							pEndPoly = CPathServer::TestShortLineOfSightImmediate(navMeshTracker.m_vLastPosition, vTestVec, pStartPoly, &vIntersectPos, NULL, NULL, kNavDomainRegular);
							if(pEndPoly)
								navMeshTracker.m_bAvoidanceRightIsBlocked = 0;

#if __BANK && defined(DEBUG_DRAW)
							if(CNavMeshTrackedObject::ms_bDrawPedTrackingProbes)
							{
								grcDebugDraw::Line(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())+Vector3(vTestVec.x,vTestVec.y,0.0f), Color_white, pEndPoly ? Color_green : Color_red);
							}
#endif
						}
					}
				}
			}

			if(navMeshTracker.m_bPerformLosAheadLineTest && pPed->GetPedResetFlag(CPED_RESET_FLAG_FollowingRoute) && !pPed->GetIsSwimming())
			{
				//static dev_float fProbeDist = 0.35f;
				//float fVelMagSqr = pPed->GetVelocity().XYMag2();
				float fVelMagSqr = pPed->GetAnimatedVelocity().XYMag2();
				if(fVelMagSqr > 0.01f)
				{
					Vector2 vInitialTestVec(0.0f, CTaskMoveGoToPoint::ms_fPedLosAheadNavMeshLineProbeDistance);

					navMeshTracker.m_bLosAheadIsBlocked = 2;

					if(navMeshTracker.m_NavMeshAndPoly.m_iNavMeshIndex != NAVMESH_NAVMESH_INDEX_NONE && navMeshTracker.m_NavMeshAndPoly.m_iPolyIndex != NAVMESH_POLY_INDEX_NONE)
					{
						CTaskMove * pSimplestMove = pPed->GetPedIntelligence()->GetActiveSimplestMovementTask();
						if(pSimplestMove && pSimplestMove->GetTaskType()==CTaskTypes::TASK_MOVE_GO_TO_POINT)
						{
							Assert(pSimplestMove->HasTarget());

							const Vector3 & vTarget = (pSimplestMove->GetLookAheadTarget().IsClose(VEC3_LARGE_FLOAT, 0.1f) ? pSimplestMove->GetTarget() : pSimplestMove->GetLookAheadTarget());
							
						//	const float fFrames = 2.f;
						//	const Vector3 vPedPosNextFrame = vPedPos + pPed->GetVelocity() * fwTimer::GetTimeStep() * fFrames;
							Vector2 vLosVec( vTarget.x - vPedPos.x, vTarget.y - vPedPos.y );

							static const float fMaxDist = CTaskNavBase::ms_fGotoTargetLookAheadDist + 0.5f;	// Use nav capabilites here!
							//Assert(vLosVec.Mag2() < fMaxDist*fMaxDist);
							if(vLosVec.Mag2() > fMaxDist*fMaxDist)
							{
								vLosVec.Normalize();
								vLosVec.Scale(fMaxDist);
							}

							CNavMesh * pNavMesh = GetNavMeshFromIndex(navMeshTracker.m_NavMeshAndPoly.m_iNavMeshIndex, kNavDomainRegular);
							if(pNavMesh)
							{
								Vector3 vIntersectPos;
								TNavMeshPoly * pStartPoly = pNavMesh->GetPoly(navMeshTracker.m_NavMeshAndPoly.m_iPolyIndex);

								// Test directly ahead
								//Vector2 vTestVec = vInitialTestVec;
								//vTestVec.Rotate( pPed->GetCurrentHeading() );

								TNavMeshPoly * pEndPoly = CPathServer::TestShortLineOfSightImmediate(vPedPos, vLosVec, pStartPoly, &vIntersectPos, NULL, NULL, kNavDomainRegular);
								if(pEndPoly)
									navMeshTracker.m_bLosAheadIsBlocked = 0;

#if __BANK && defined(DEBUG_DRAW)
								if(CNavMeshTrackedObject::ms_bDrawPedTrackingProbes)
								{
									grcDebugDraw::Line(vPedPos, vPedPos+Vector3(vLosVec.x,vLosVec.y,0.0f), Color_white, pEndPoly ? Color_green : Color_red);
								}
#endif
							}
						}
					}
				}
			}

			if(navMeshTracker.GetIsValid() && !navMeshTracker.GetNavPolyData().m_bIsolated)
			{
				pPed->GetPedIntelligence()->SetLastMainNavMeshPosition( navMeshTracker.GetLastPosition() );
			}

			// Update metric which gives some measure of how much free space surrounds this ped
			// TODO: Don't query for the navmesh/poly under the tracker each time
			pNavMesh = GetNavMeshFromIndex(navMeshTracker.m_NavMeshAndPoly.m_iNavMeshIndex, kNavDomainRegular);
			if(pNavMesh)
			{
				float fFreeSpace = 0.0f;
				TNavMeshPoly * pPoly = pNavMesh->GetPoly(navMeshTracker.m_NavMeshAndPoly.m_iPolyIndex);
				for(u32 a=0; a<pPoly->GetNumVertices(); a++)
				{
					const TAdjPoly & adjPoly = pNavMesh->GetAdjacentPoly(pPoly->GetFirstVertexIndex()+a);
					fFreeSpace += adjPoly.GetFreeSpaceAroundVertex();
				}
				fFreeSpace /= (float)pPoly->GetNumVertices();
				fFreeSpace /= TAdjPoly::ms_fMaxFreeSpaceAroundVertex;
				Assert(fFreeSpace >= 0.0f && fFreeSpace <= 1.0f);

				navMeshTracker.SetFreeSpaceMetric( (navMeshTracker.GetFreeSpaceMetric() * 0.5f) + (fFreeSpace * 0.5f) );
			}
		}
	}

#if !__FINAL	
	m_TrackObjectsTimer->Stop();
	m_fTimeToTrackAllPeds = m_TrackObjectsTimer->GetTimeMS();
#endif
}

// NAME : InvalidateTrackers
// PURPOSE : Invalidate all navmesh trackers which are associated with this navmesh
// Ideally we should have some central list of trackers but for now we just iterate the ped pool
void CPathServer::InvalidateTrackers(const u32 iNavMesh)
{
	// Update the tracking for all the peds
	CPed::Pool * pPool = CPed::GetPool();
	for(int p=0; p<pPool->GetSize(); p++)
	{
		CPed * pPed = pPool->GetSlot(p);
		if(pPed)
		{
			CNavMeshTrackedObject & navMeshTracker = pPed->GetNavMeshTracker();
			if(iNavMesh == NAVMESH_NAVMESH_INDEX_NONE || navMeshTracker.GetNavMeshAndPoly().m_iNavMeshIndex == iNavMesh)
			{
				navMeshTracker.SetInvalid();
			}
		}
	}
}


#endif // __TRACK_PEDS_IN_NAVMESH

#endif

