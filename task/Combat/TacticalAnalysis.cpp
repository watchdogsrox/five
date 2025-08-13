// FILE :    TacticalAnalysis.h

// File header
#include "task/Combat/TacticalAnalysis.h"

// Rage headers
#include "fwmaths/angle.h"
#include "grcore/debugdraw.h"

// Game headers
#include "pathserver/PathServer.h"
#include "Peds/ped.h"
#include "physics/gtaArchetype.h"
#include "physics/WorldProbe/shapetestprobedesc.h"
#include "vehicles/vehicle.h"

// Parser headers
#include "task/Combat/TacticalAnalysis_parser.h"

AI_OPTIMISATIONS()

#define IMPLEMENT_TACTICAL_ANALYSIS_TUNABLES(classname, hash)	IMPLEMENT_TUNABLES_PSO(classname, #classname, hash, "Tactical Analysis", "Game Logic")

//Note: For many of the algorithms that are searching arrays to try to find the best object to update,
//		std::sort probably would have been better/more ideal than the current implementation.  However,
//		it decided to randomly trash memory -- which is decidedly less than ideal.  Because of this,
//		I was forced to revert to crappy linear searches.

////////////////////////////////////////////////////////////////////////////////
// CTacticalAnalysisNavMeshPoints
////////////////////////////////////////////////////////////////////////////////

atFixedArray<Vec3V, CTacticalAnalysisNavMeshPoints::sMaxPointsFromScript> CTacticalAnalysisNavMeshPoints::sm_aPointsFromScript;

CTacticalAnalysisNavMeshPoints::Tunables CTacticalAnalysisNavMeshPoints::sm_Tunables;
IMPLEMENT_TACTICAL_ANALYSIS_TUNABLES(CTacticalAnalysisNavMeshPoints, 0xafc1c094)

CTacticalAnalysisNavMeshPoints::CTacticalAnalysisNavMeshPoints()
: m_pTacticalAnalysis(NULL)
, m_fOffsetForLineOfSightTests(0.0f)
{
	//Set the handles.
	for(int i = 0; i < m_aPoints.GetMaxCount(); ++i)
	{
		m_aPoints[i].m_iHandle = i;
	}

	//Some points are reserved for script.
	for(int i = 0; i < sMaxPointsFromScript; ++i)
	{
		m_aPoints[i].ReserveForScript();
	}
}

CTacticalAnalysisNavMeshPoints::~CTacticalAnalysisNavMeshPoints()
{

}

bool CTacticalAnalysisNavMeshPoints::ReleaseHandle(const CPed* pPed, int iHandle)
{
	//Find the index.
	int iIndex = FindPointForHandle(iHandle);
	if(iIndex < 0)
	{
		return false;
	}

	//Release the point.
	CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[iIndex];
	return ReleasePoint(pPed, rPoint);
}

bool CTacticalAnalysisNavMeshPoints::ReleasePoint(const CPed* pPed, CTacticalAnalysisNavMeshPoint& rPoint)
{
	//Ensure the point can be released by the ped.
	if(!rPoint.CanRelease(pPed))
	{
		return false;
	}

	//Release the point.
	rPoint.Release(pPed);

	return true;
}

bool CTacticalAnalysisNavMeshPoints::ReserveHandle(const CPed* pPed, int iHandle)
{
	//Find the index.
	int iIndex = FindPointForHandle(iHandle);
	if(iIndex < 0)
	{
		return false;
	}

	//Reserve the point.
	CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[iIndex];
	return ReservePoint(pPed, rPoint);
}

bool CTacticalAnalysisNavMeshPoints::ReservePoint(const CPed* pPed, CTacticalAnalysisNavMeshPoint& rPoint)
{
	//Ensure the point can be reserved by the ped.
	if(!rPoint.CanReserve(pPed))
	{
		return false;
	}

	//Reserve the point.
	rPoint.Reserve(pPed);

	return true;
}

bool CTacticalAnalysisNavMeshPoints::AddPointFromScript(Vec3V_In vPosition)
{
	//Ensure the points are not full.
	if(!aiVerifyf(!sm_aPointsFromScript.IsFull(), "Too many points have been added from script."))
	{
		return false;
	}

	//Add the point.
	sm_aPointsFromScript.Push(vPosition);

	//Sync the points from script.
	SyncPointsFromScript();

	return true;
}

void CTacticalAnalysisNavMeshPoints::ClearPointsFromScript()
{
	//Clear the points.
	sm_aPointsFromScript.Reset();

	//Sync the points from script.
	SyncPointsFromScript();
}

void CTacticalAnalysisNavMeshPoints::Activate()
{
	//Initialize the points.
	InitializePoints();

	//Initialize the offset for line of sight tests.
	InitializeOffsetForLineOfSightTests();
}

float CTacticalAnalysisNavMeshPoints::CalculateDistanceSqFromPed(Vec3V_In vPosition) const
{
	//Grab the ped position.
	Vec3V vPedPosition = m_pTacticalAnalysis->GetPedPosition();
	Vec3V vPedPositionXY = vPedPosition;
	vPedPositionXY.SetZ(ScalarV(V_ZERO));

	//Flatten the position.
	Vec3V vPositionXY = vPosition;
	vPositionXY.SetZ(ScalarV(V_ZERO));

	//Calculate the distance.
	ScalarV scDistSq = DistSquared(vPedPositionXY, vPositionXY);

	return scDistSq.Getf();
}

float CTacticalAnalysisNavMeshPoints::CalculateOffsetForLineOfSightTests() const
{
	//Ensure the capsule info is valid.
	atHashWithStringNotFinal s_hPedCapsule("STANDARD_MALE",0x6EE4E672);
	const CBaseCapsuleInfo* pCapsuleInfo = CPedCapsuleInfoManager::GetInfo(s_hPedCapsule.GetHash());
	if(!aiVerifyf(pCapsuleInfo, "The capsule info: %s is invalid.", s_hPedCapsule.GetCStr()))
	{
		return 0.0f;
	}

	//Ensure the capsule info is biped.
	if(!aiVerify(pCapsuleInfo->IsBiped()))
	{
		return 0.0f;
	}

	const CBipedCapsuleInfo* pBipedCapsuleInfo = static_cast<const CBipedCapsuleInfo *>(pCapsuleInfo);
	return pBipedCapsuleInfo->GetHeadHeight();
}

void CTacticalAnalysisNavMeshPoints::Deactivate()
{
	//Reset the points.
	ResetPoints();

	//Reset the line of sight tests.
	ResetLineOfSightTests();
}

void CTacticalAnalysisNavMeshPoints::DecayBadRouteValues(float fTimeStep)
{
	//Iterate over the points.
	int iCount = m_aPoints.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the point is valid.
		CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[i];
		if(!rPoint.IsValid())
		{
			continue;
		}

		//Ensure the bad route value is valid.
		if(rPoint.m_fBadRouteValue <= 0.0f)
		{
			continue;
		}

		//Decay the bad route value.
		float fDecrease = fTimeStep * CTacticalAnalysis::GetTunables().m_BadRoute.m_DecayRate;
		rPoint.m_fBadRouteValue -= fDecrease;
		if(rPoint.m_fBadRouteValue < 0.0f)
		{
			rPoint.m_fBadRouteValue = 0.0f;
		}
	}
}

void CTacticalAnalysisNavMeshPoints::FindNearby()
{
	//Iterate over the points.
	int iMaxNearbyToFindPerFrame = sm_Tunables.m_MaxNearbyToFindPerFrame;
	for(int i = 0; i < iMaxNearbyToFindPerFrame; ++i)
	{
		//Find the next point to find a nearby ped.
		int iIndex = FindNextPointForFindNearby();
		if(iIndex < 0)
		{
			break;
		}

		//Find a nearby ped for the point.
		FindNearby(m_aPoints[iIndex]);
	}
}

void CTacticalAnalysisNavMeshPoints::FindNearby(CTacticalAnalysisNavMeshPoint& rPoint)
{
	//Clear the time since the last attempt to find a nearby ped.
	rPoint.m_fTimeSinceLastAttemptToFindNearby = 0.0f;

	//Find a nearby ped.
	float fRadius = sm_Tunables.m_RadiusForFindNearby;
	rPoint.m_pNearby = m_pTacticalAnalysis->FindNearby(rPoint.GetPosition(), fRadius);
}

int CTacticalAnalysisNavMeshPoints::FindNextPointForFindNearby() const
{
	//Keep track of the best index.
	int iBestIndex = -1;

	//Iterate over the points.
	int iCount = m_aPoints.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the point is valid.
		const CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[i];
		if(!rPoint.IsValid())
		{
			continue;
		}

		//Ensure the time has exceeded the minimum.
		float fTime = rPoint.m_fTimeSinceLastAttemptToFindNearby;
		float fMinTime = sm_Tunables.m_MinTimeBetweenAttemptsToFindNearby;
		if(fTime < fMinTime)
		{
			continue;
		}

		//Check if the best index is valid, and this point is not better.
		if(iBestIndex >= 0 && !IsPointBetterForFindNearby(rPoint, m_aPoints[iBestIndex]))
		{
			continue;
		}

		//Set the best index.
		iBestIndex = i;
	}

	return iBestIndex;
}

int CTacticalAnalysisNavMeshPoints::FindNextPointForFindNewPosition() const
{
	//Keep track of the best index.
	int iBestIndex = -1;

	//Iterate over the points.
	int iCount = m_aPoints.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the point is invalid.
		const CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[i];
		if(rPoint.IsValid())
		{
			continue;
		}

		//Ensure the point is not reserved for script.
		if(rPoint.IsReservedForScript())
		{
			continue;
		}

		//Ensure the time has exceeded the minimum.
		float fTime = rPoint.m_fTimeSinceLastAttemptToFindNewPosition;
		float fMinTime = sm_Tunables.m_MinTimeBetweenAttemptsToFindNewPosition;
		if(fTime < fMinTime)
		{
			continue;
		}

		//Check if the best index is valid, and this point is not better.
		if(iBestIndex >= 0 && !IsPointBetterForFindNewPosition(rPoint, m_aPoints[iBestIndex]))
		{
			continue;
		}

		//Set the best index.
		iBestIndex = i;
	}

	return iBestIndex;
}

int CTacticalAnalysisNavMeshPoints::FindNextPointForLineOfSightTest() const
{
	//Keep track of the best index.
	int iBestIndex = -1;

	//Iterate over the points.
	int iCount = m_aPoints.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the point is invalid.
		const CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[i];
		if(!rPoint.IsValid())
		{
			continue;
		}

		//Ensure the time has exceeded the minimum.
		float fTime = rPoint.m_fTimeSinceLastLineOfSightTest;
		float fMinTime = sm_Tunables.m_MinTimeBetweenLineOfSightTests;
		if(fTime < fMinTime)
		{
			continue;
		}

		//Ensure a line of sight test is not running for this handle.
		if(IsLineOfSightTestRunningForHandle(rPoint.m_iHandle))
		{
			continue;
		}

		//Check if the best index is valid, and this point is not better.
		if(iBestIndex >= 0 && !IsPointBetterForLineOfSightTest(rPoint, m_aPoints[iBestIndex]))
		{
			continue;
		}

		//Set the best index.
		iBestIndex = i;
	}

	return iBestIndex;
}

int CTacticalAnalysisNavMeshPoints::FindPointForHandle(int iHandle) const
{
	//Check if the handle is valid.
	if(iHandle >= 0)
	{
		//Iterate over the points.
		for(int i = 0; i < m_aPoints.GetMaxCount(); ++i)
		{
			//Ensure the handle matches.
			if(iHandle != m_aPoints[i].m_iHandle)
			{
				continue;
			}

			return i;
		}
	}

	return -1;
}

Vec3V_Out CTacticalAnalysisNavMeshPoints::GenerateNewPosition() const
{
	//Choose a random heading.
	float fMinHeading = -PI;
	float fMaxHeading = PI;
	float fHeading = fwRandom::GetRandomNumberInRange(fMinHeading, fMaxHeading);

	//Choose a random distance.
	float fMinDistance = sm_Tunables.m_MinDistance;
	float fMaxDistance = sm_Tunables.m_MaxDistance;
	float fDistance = fwRandom::GetRandomNumberInRange(fMinDistance, fMaxDistance);

	//Calculate the offset.
	Vec3V vDirection(V_Y_AXIS_WZERO);
	vDirection = RotateAboutZAxis(vDirection, ScalarVFromF32(fHeading));
	Vec3V vOffset = Scale(vDirection, ScalarVFromF32(fDistance));

	//Grab the ped position.
	Vec3V vPedPosition = m_pTacticalAnalysis->GetPedPosition();

	//Calculate the position.
	Vec3V vPosition = Add(vPedPosition, vOffset);

	return vPosition;
}

void CTacticalAnalysisNavMeshPoints::InitializeOffsetForLineOfSightTests()
{
	//Calculate the offset.
	m_fOffsetForLineOfSightTests = CalculateOffsetForLineOfSightTests();
}

void CTacticalAnalysisNavMeshPoints::InitializePoints()
{
	//Reset the points.
	ResetPoints();

	//Sync the points from script.
	SyncPointsFromScript();
}

bool CTacticalAnalysisNavMeshPoints::IsLineOfSightTestRunningForHandle(int iHandle) const
{
	//Iterate over the line of sight tests.
	int iCount = m_aLineOfSightTests.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the test is active.
		const LineOfSightTest& rTest = m_aLineOfSightTests[i];
		if(!rTest.m_AsyncProbeHelper.IsProbeActive())
		{
			continue;
		}

		//Ensure the handle matches.
		if(iHandle != rTest.m_iHandle)
		{
			continue;
		}

		return true;
	}

	return false;
}

bool CTacticalAnalysisNavMeshPoints::IsNewPositionValid(Vec3V_In vPosition) const
{
	//Ensure the position is valid.
	if(!IsPositionValid(vPosition))
	{
		return false;
	}

	//Calculate the min distance between positions.
	ScalarV scMinDistWithClearLosSq		= ScalarVFromF32(square(sm_Tunables.m_MinDistanceBetweenPositionsWithClearLineOfSight));
	ScalarV scMinDistWithoutClearLosSq	= ScalarVFromF32(square(!m_pTacticalAnalysis->GetPed()->GetIsInInterior() ?
		sm_Tunables.m_MinDistanceBetweenPositionsWithoutClearLineOfSightInExteriors :
		sm_Tunables.m_MinDistanceBetweenPositionsWithoutClearLineOfSightInInteriors));

	//Iterate over the points.
	int iCount = m_aPoints.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the point is valid.
		const CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[i];
		if(!rPoint.IsValid())
		{
			continue;
		}

		//Choose the min distance.
		ScalarV scMinDistSq = (rPoint.IsLineOfSightClear() ? scMinDistWithClearLosSq : scMinDistWithoutClearLosSq);

		//Ensure the position is not too close.
		ScalarV scDistSq = DistSquared(vPosition, rPoint.GetPosition());
		if(IsLessThanAll(scDistSq, scMinDistSq))
		{
			return false;
		}
	}

	return true;
}

bool CTacticalAnalysisNavMeshPoints::IsPositionValid(Vec3V_In vPosition) const
{
	//Calculate the distance from the ped.
	float fDistSq = CalculateDistanceSqFromPed(vPosition);

	//Ensure the distance exceeds the minimum.
	float fMinDistance = sm_Tunables.m_MinDistance - sm_Tunables.m_BufferDistance;
	float fMinDistanceSq = square(fMinDistance);
	if(fDistSq < fMinDistanceSq)
	{
		return false;
	}

	//Ensure the distance does not exceed the maximum.
	float fMaxDistance = sm_Tunables.m_MaxDistance + sm_Tunables.m_BufferDistance;
	float fMaxDistanceSq = square(fMaxDistance);
	if(fDistSq > fMaxDistanceSq)
	{
		return false;
	}

	return true;
}

bool CTacticalAnalysisNavMeshPoints::IsXYDistanceGreaterThan(Vec3V_In vPositionA, Vec3V_In vPositionB, float fMinDistance) const
{
	//Generate the XY vectors.
	Vec3V vPositionAXY = vPositionA;
	vPositionAXY.SetZ(ScalarV(V_ZERO));
	Vec3V vPositionBXY = vPositionB;
	vPositionBXY.SetZ(ScalarV(V_ZERO));

	//Calculate the distance.
	float fDistanceSq = DistSquared(vPositionAXY, vPositionBXY).Getf();
	float fMinDistanceSq = square(fMinDistance);

	return (fDistanceSq > fMinDistanceSq);
}

void CTacticalAnalysisNavMeshPoints::OnBadRoute(Vec3V_In vPosition, float fValue)
{
	//Iterate over the points.
	int iCount = m_aPoints.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the point is valid.
		CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[i];
		if(!rPoint.IsValid())
		{
			continue;
		}

		//Ensure the distance is valid.
		ScalarV scDistSq = DistSquared(vPosition, rPoint.GetPosition());
		ScalarV scMaxDistSq = ScalarVFromF32(square(CTacticalAnalysis::GetTunables().m_BadRoute.m_MaxDistanceForTaint));
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			continue;
		}

		//Increase the bad route value.
		rPoint.m_fBadRouteValue += fValue;
	}
}

void CTacticalAnalysisNavMeshPoints::ProcessResultsForLineOfSightTests()
{
	//Iterate over the line of sight tests.
	int iCount = m_aLineOfSightTests.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the line of sight test.
		LineOfSightTest& rTest = m_aLineOfSightTests[i];

		//Ensure the test is active.
		if(!rTest.m_AsyncProbeHelper.IsProbeActive())
		{
			continue;
		}

		//Ensure the results are ready.
		ProbeStatus nProbeStatus;
		if(!rTest.m_AsyncProbeHelper.GetProbeResults(nProbeStatus))
		{
			continue;
		}

		//Reset the probe.
		rTest.m_AsyncProbeHelper.ResetProbe();

		//Find the point with the handle.
		int iIndex = FindPointForHandle(rTest.m_iHandle);

		//Clear the handle.
		rTest.m_iHandle = -1;

		//Ensure the index is valid.
		if(iIndex < 0)
		{
			continue;
		}

		//Ensure the point is valid.
		CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[iIndex];
		if(!rPoint.IsValid())
		{
			continue;
		}

		//Set the line of sight status.
		rPoint.m_nLineOfSightStatus = (nProbeStatus == PS_Clear) ? CTacticalAnalysisNavMeshPoint::LOSS_Clear : CTacticalAnalysisNavMeshPoint::LOSS_Blocked;
	}
}

void CTacticalAnalysisNavMeshPoints::ResetInvalidPoints()
{
	//Iterate over the points.
	for(int i = 0; i < m_aPoints.GetMaxCount(); ++i)
	{
		//Ensure the point is valid.
		CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[i];
		if(!rPoint.IsValid())
		{
			continue;
		}

		//Ensure the point is not reserved for script.
		if(rPoint.IsReservedForScript())
		{
			continue;
		}

		//Ensure the position is invalid.
		if(IsPositionValid(rPoint.GetPosition()))
		{
			continue;
		}

		//Reset the point.
		ResetPoint(rPoint);
	}
}

void CTacticalAnalysisNavMeshPoints::ResetLineOfSightTests()
{
	//Iterate over the line of sight tests.
	for(int i = 0; i < m_aLineOfSightTests.GetMaxCount(); ++i)
	{
		//Reset the line of sight test.
		m_aLineOfSightTests[i].m_AsyncProbeHelper.ResetProbe();
	}
}

void CTacticalAnalysisNavMeshPoints::ResetPoint(CTacticalAnalysisNavMeshPoint& rPoint)
{
	//Reset the nearby ped.
	rPoint.m_pNearby = NULL;

	//Reset the reservation.
	rPoint.m_pReservedBy = NULL;

	//Reset the position.
	rPoint.SetPosition(Vec3V(V_ZERO));

	//Reset the line of sight status.
	rPoint.m_nLineOfSightStatus = CTacticalAnalysisNavMeshPoint::LOSS_Unchecked;

	//Reset the timers.
	rPoint.m_fTimeSinceLastAttemptToFindNewPosition	= LARGE_FLOAT;
	rPoint.m_fTimeSinceLastLineOfSightTest			= LARGE_FLOAT;
	rPoint.m_fTimeSinceLastAttemptToFindNearby		= LARGE_FLOAT;
	rPoint.m_fTimeUntilRelease						= 0.0f;
	rPoint.m_fBadRouteValue							= 0.0f;

	//Reset the flags.
	rPoint.m_uFlags.ClearFlag(CTacticalAnalysisNavMeshPoint::F_IsValid);
}

void CTacticalAnalysisNavMeshPoints::ResetPoints()
{
	//Iterate over the points.
	for(int i = 0; i < m_aPoints.GetMaxCount(); ++i)
	{
		//Reset the point.
		ResetPoint(m_aPoints[i]);
	}
}

void CTacticalAnalysisNavMeshPoints::SetPoint(CTacticalAnalysisNavMeshPoint& rPoint, Vec3V_In vPosition)
{
	//Set the nearby ped.
	rPoint.m_pNearby = NULL;

	//Set the reservation.
	rPoint.m_pReservedBy = NULL;

	//Set the position.
	rPoint.SetPosition(vPosition);

	//Set the line of sight status.
	rPoint.m_nLineOfSightStatus = CTacticalAnalysisNavMeshPoint::LOSS_Unchecked;

	//Set the timers.
	rPoint.m_fTimeSinceLastAttemptToFindNewPosition	= 0.0f;
	rPoint.m_fTimeSinceLastLineOfSightTest			= LARGE_FLOAT;
	rPoint.m_fTimeSinceLastAttemptToFindNearby		= LARGE_FLOAT;
	rPoint.m_fTimeUntilRelease						= 0.0f;
	rPoint.m_fBadRouteValue							= 0.0f;

	//Set the flags.
	rPoint.m_uFlags.SetFlag(CTacticalAnalysisNavMeshPoint::F_IsValid);
}

void CTacticalAnalysisNavMeshPoints::StartNewLineOfSightTests()
{
	//Iterate over the line of sight tests.
	int iNumTests = m_aLineOfSightTests.GetMaxCount();
	for(int i = 0; i < iNumTests; ++i)
	{
		//Ensure the test is not active.
		LineOfSightTest& rTest = m_aLineOfSightTests[i];
		if(rTest.m_AsyncProbeHelper.IsProbeActive())
		{
			continue;
		}

		//Find the next point for line of sight test.
		int iIndex = FindNextPointForLineOfSightTest();
		if(iIndex < 0)
		{
			break;
		}

		//Grab the point.
		CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[iIndex];

		//Calculate the position.
		Vec3V vPosition = rPoint.GetPosition();
		vPosition.SetZf(vPosition.GetZf() + m_fOffsetForLineOfSightTests);

		//Start a line of sight test.
		if(!m_pTacticalAnalysis->StartLineOfSightTest(rTest.m_AsyncProbeHelper, vPosition))
		{
			continue;
		}

		//Set the handle.
		rTest.m_iHandle = rPoint.m_iHandle;

		//Set the time since the last line of sight test.
		rPoint.m_fTimeSinceLastLineOfSightTest = 0.0f;
	}
}

void CTacticalAnalysisNavMeshPoints::SetPointsFromScript()
{
	//Reset the points from script.
	for(int i = 0; i < sMaxPointsFromScript; ++i)
	{
		aiAssert(m_aPoints[i].IsReservedForScript());
		ResetPoint(m_aPoints[i]);
	}

	//Set the points from script.
	for(int i = 0; i < sm_aPointsFromScript.GetCount(); ++i)
	{
		aiAssert(m_aPoints[i].IsReservedForScript());
		SetPoint(m_aPoints[i], sm_aPointsFromScript[i]);
	}
}

void CTacticalAnalysisNavMeshPoints::Update(float fTimeStep)
{
	//Update the points.
	UpdatePoints(fTimeStep);
}

void CTacticalAnalysisNavMeshPoints::UpdateInvalidPoints(float fTimeStep)
{
	//Update the timers for the invalid points.
	UpdateTimersForInvalidPoints(fTimeStep);

	//Update the positions.
	UpdatePositions();
}

void CTacticalAnalysisNavMeshPoints::UpdateLineOfSightTests()
{
	//Process the results of the line of sight tests.
	ProcessResultsForLineOfSightTests();

	//Start new line of sight tests.
	StartNewLineOfSightTests();
}

void CTacticalAnalysisNavMeshPoints::UpdatePoints(float fTimeStep)
{
	//Reset the invalid points.
	ResetInvalidPoints();

	//Update the invalid points.
	UpdateInvalidPoints(fTimeStep);

	//Update the valid points.
	UpdateValidPoints(fTimeStep);
}

void CTacticalAnalysisNavMeshPoints::UpdatePositions()
{
	//Check if the closest position helper is active.
	if(m_ClosestPosition.m_Helper.IsSearchActive())
	{
		//Get the search results.
		SearchStatus nSearchStatus;
		s32 iNumPositions;
		static const s32 s_iMaxPositions = 1;
		Vector3 aPositions[s_iMaxPositions];
		if(!m_ClosestPosition.m_Helper.GetSearchResults(nSearchStatus, iNumPositions, aPositions, s_iMaxPositions))
		{
			return;
		}

		//Assert that the search is no longer active.
		aiAssert(!m_ClosestPosition.m_Helper.IsSearchActive());

		//Check if the search was successful.
		if((nSearchStatus == SS_SearchSuccessful) && (iNumPositions > 0))
		{
			//Grab the positions.
			Vec3V vPosition					= m_ClosestPosition.m_vPosition;
			Vec3V vClosestNavMeshPosition	= VECTOR3_TO_VEC3V(aPositions[0]);

			//Check if the closest nav mesh position does not differ from the one we generated in the XY direction, over a certain threshold.
			//Without this, when the target is in an enclosed alley, the points will be clustered around the walls (since we will end up generating
			//a lot of the points inside the walls, and they will get pushed back into the alley when finding the closest nav mesh).
			float fMaxXYDistance = sm_Tunables.m_MaxXYDistanceForNewPosition;
			if(!IsXYDistanceGreaterThan(vPosition, vClosestNavMeshPosition, fMaxXYDistance) && IsNewPositionValid(vClosestNavMeshPosition))
			{
				//Set the point.
				aiAssert(m_ClosestPosition.m_iHandle >= 0);
				CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[m_ClosestPosition.m_iHandle];
				SetPoint(rPoint, vClosestNavMeshPosition);
			}
		}
	}

	//Find the next point to find a new position.
	int iIndex = FindNextPointForFindNewPosition();
	if(iIndex < 0)
	{
		return;
	}

	//Assert that the point is not reserved for script.
	CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[iIndex];
	aiAssert(!rPoint.IsReservedForScript());

	//Clear the time since the last attempt to find a new position.
	rPoint.m_fTimeSinceLastAttemptToFindNewPosition = 0.0f;

	//Generate a new position.
	Vec3V vPosition = GenerateNewPosition();

	//Start the search.
	m_ClosestPosition.m_Helper.StartClosestPositionSearch(VEC3V_TO_VECTOR3(vPosition), sm_Tunables.m_MaxSearchRadiusForNavMesh,
		CNavmeshClosestPositionHelper::Flag_ConsiderExterior | CNavmeshClosestPositionHelper::Flag_ConsiderInterior |
		CNavmeshClosestPositionHelper::Flag_ConsiderOnlyLand, 1.0f, 1.0f, 0.0f, 0, NULL, 1);

	//Set the position.
	m_ClosestPosition.m_vPosition = vPosition;

	//Set the handle.
	m_ClosestPosition.m_iHandle = iIndex;
}


void CTacticalAnalysisNavMeshPoints::UpdateTimersForInvalidPoints(float fTimeStep)
{
	//Iterate over the points.
	int iCount = m_aPoints.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the point is invalid.
		CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[i];
		if(rPoint.IsValid())
		{
			continue;
		}

		//Increase the time since the last attempt to find a new position.
		rPoint.m_fTimeSinceLastAttemptToFindNewPosition += fTimeStep;
	}
}

void CTacticalAnalysisNavMeshPoints::UpdateTimersForValidPoints(float fTimeStep)
{
	//Iterate over the points.
	int iCount = m_aPoints.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the point is valid.
		CTacticalAnalysisNavMeshPoint& rPoint = m_aPoints[i];
		if(!rPoint.IsValid())
		{
			continue;
		}

		//Increase the time since the last line of sight test.
		rPoint.m_fTimeSinceLastLineOfSightTest += fTimeStep;

		//Increase the time since the last attempt to find a nearby ped.
		rPoint.m_fTimeSinceLastAttemptToFindNearby += fTimeStep;

		//Check if the time until release is valid.
		float& fTimeUntilRelease = rPoint.m_fTimeUntilRelease;
		if(fTimeUntilRelease > 0.0f)
		{
			//Check if the time until release has elapsed.
			fTimeUntilRelease -= fTimeStep;
			if(fTimeUntilRelease <= 0.0f)
			{
				//Release the point.
				ReleasePoint(rPoint.GetReservedBy(), rPoint);
			}
		}
	}
}

void CTacticalAnalysisNavMeshPoints::UpdateValidPoints(float fTimeStep)
{
	//Update the timers for the valid points.
	UpdateTimersForValidPoints(fTimeStep);

	//Update the line of sight tests.
	UpdateLineOfSightTests();

	//Find nearby peds.
	FindNearby();

	//Decay the bad route values.
	DecayBadRouteValues(fTimeStep);
}

void CTacticalAnalysisNavMeshPoints::SyncPointsFromScript()
{
	//Iterate over the pool.
	CTacticalAnalysis::Pool* pPool = CTacticalAnalysis::GetPool();
	for(s32 i = 0; i < pPool->GetSize(); ++i)
	{
		//Grab the tactical analysis.
		CTacticalAnalysis* pTacticalAnalysis = pPool->GetSlot(i);
		if(!pTacticalAnalysis)
		{
			continue;
		}

		//Set the points from script.
		pTacticalAnalysis->GetNavMeshPoints().SetPointsFromScript();
	}
}

////////////////////////////////////////////////////////////////////////////////
// CTacticalAnalysisCoverPointSearch
////////////////////////////////////////////////////////////////////////////////

CTacticalAnalysisCoverPointSearch::Tunables CTacticalAnalysisCoverPointSearch::sm_Tunables;
IMPLEMENT_TACTICAL_ANALYSIS_TUNABLES(CTacticalAnalysisCoverPointSearch, 0x31f6688a)

CTacticalAnalysisCoverPointSearch::CTacticalAnalysisCoverPointSearch()
: m_aCoverPoints()
, m_Options()
, m_nState(Idle)
, m_pTacticalAnalysis(NULL)
, m_iIndexToScore(-1)
{

}

CTacticalAnalysisCoverPointSearch::~CTacticalAnalysisCoverPointSearch()
{

}

float CTacticalAnalysisCoverPointSearch::CalculateScore(const CCoverPoint& rCoverPoint) const
{
	//Initialize the score.
	float fScore = 1.0f;

	//Check if the cover point is occupied.
	if(rCoverPoint.IsOccupied())
	{
		fScore *= sm_Tunables.m_Scoring.m_Occupied;
	}

	//Check the cover point type.
	CCoverPoint::eCoverType nType = rCoverPoint.GetType();
	if(nType == CCoverPoint::COVTYPE_SCRIPTED)
	{
		fScore *= sm_Tunables.m_Scoring.m_Scripted;
	}
	else if(nType == CCoverPoint::COVTYPE_POINTONMAP)
	{
		fScore *= sm_Tunables.m_Scoring.m_PointOnMap;
	}

	//Grab the position.
	Vec3V vPosition;
	rCoverPoint.GetCoverPointPosition(RC_VECTOR3(vPosition));

	//Calculate the distance.
	float fDistanceSq = DistSquared(m_pTacticalAnalysis->GetPedPosition(), vPosition).Getf();

	//Check if we are a certain distance from the optimal.
	float fMinDistanceSq = square(sm_Tunables.m_Scoring.m_MinDistanceToBeConsideredOptimal);
	float fMaxDistanceSq = square(sm_Tunables.m_Scoring.m_MaxDistanceToBeConsideredOptimal);
	if((fDistanceSq >= fMinDistanceSq) && (fDistanceSq <= fMaxDistanceSq))
	{
		fScore *= sm_Tunables.m_Scoring.m_Optimal;
	}

	return fScore;
}

void CTacticalAnalysisCoverPointSearch::DoCullExcessive()
{
	//Check if we have exceeded the max results.
	int iMaxResults = m_Options.m_iMaxResults;
	if(m_aCoverPoints.GetCount() > iMaxResults)
	{
		//Sort the cover points by score.
		qsort(m_aCoverPoints.GetElements(), m_aCoverPoints.GetCount(), sizeof(CoverPointWithScore), CTacticalAnalysisCoverPointSearch::SortByScore);

		//Resize the array to the max results.
		m_aCoverPoints.Resize(iMaxResults);
	}

	//Set the state.
	m_nState = Finished;
}

void CTacticalAnalysisCoverPointSearch::DoCullInvalid()
{
	//Iterate over the cover points.
	for(int i = 0; i < m_aCoverPoints.GetCount(); ++i)
	{
		//Ensure the cover point is invalid.
		CCoverPoint* pCoverPoint = m_aCoverPoints[i].m_pCoverPoint;
		if(IsValid(*pCoverPoint))
		{
			continue;
		}

		//Remove the cover point.
		m_aCoverPoints.DeleteFast(i--);
	}

	//Check if we have too many cover points.  If so, score them.
	if(m_aCoverPoints.GetCount() > m_Options.m_iMaxResults)
	{
		//Set the state.
		m_nState = Score;
	}
	else
	{
		//Set the state.
		m_nState = Finished;
	}
}

void CTacticalAnalysisCoverPointSearch::DoFind()
{
	//Determine the min/max positions.
	float fMaxDistance = m_Options.m_fMaxDistance;
	const Vec3V vSize(fMaxDistance, fMaxDistance, fMaxDistance);
	const Vec3V vPos = m_Options.m_vPosition;
	const Vec3V vMin = vPos - vSize;
	const Vec3V vMax = vPos + vSize;

	//Iterate over the cover points grids.
	int iCount = CCoverPointsContainer::GetNumGrids();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the grid cell is valid.
		CCoverPointsGridCell* pGridCell = CCoverPointsContainer::GetGridCellForIndex(i);
		if(!aiVerifyf(pGridCell, "The grid cell is invalid."))
		{
			continue;
		}

		//Ensure the grid cell overlaps the bb.
		if(!pGridCell->DoesAABBOverlapContents(vMin, vMax))
		{
			continue;
		}

		//Add the cover points.
		CCoverPoint* pCoverPoint = pGridCell->m_pFirstCoverPoint;
		while(pCoverPoint)
		{
			//Ensure the cover points are not full.
			if(m_aCoverPoints.IsFull())
			{
				break;
			}

			//Add the cover point.
			CoverPointWithScore& rCoverPoint = m_aCoverPoints.Append();
			rCoverPoint.m_pCoverPoint = pCoverPoint;
			rCoverPoint.m_fScore = 0.0f;

			//Move to the next cover point.
			pCoverPoint = pCoverPoint->m_pNextCoverPoint;
		}
	}

	//Set the state.
	m_nState = CullInvalid;
}

void CTacticalAnalysisCoverPointSearch::DoScore()
{
	//Iterate over the cover points.
	int iScoreCalculationsPerFrame = sm_Tunables.m_ScoreCalculationsPerFrame;
	int iMaxIndexToScoreThisFrame = m_iIndexToScore + iScoreCalculationsPerFrame;
	for(; m_iIndexToScore < iMaxIndexToScoreThisFrame; ++m_iIndexToScore)
	{
		//Ensure the index is valid.
		if(m_iIndexToScore >= m_aCoverPoints.GetCount())
		{
			break;
		}

		//Grab the cover point.
		CoverPointWithScore& rCoverPoint = m_aCoverPoints[m_iIndexToScore];

		//Ensure the cover point is still valid.
		CCoverPoint* pCoverPoint = rCoverPoint.m_pCoverPoint;
		if(!pCoverPoint || (pCoverPoint->GetType() == CCoverPoint::COVTYPE_NONE))
		{
			//Remove the cover point.
			m_aCoverPoints.DeleteFast(m_iIndexToScore--);
			continue;
		}

		//Score the cover point.
		rCoverPoint.m_fScore = CalculateScore(*pCoverPoint);
	}

	//Check if we have too many cover points.  If so, cull them.
	if(m_aCoverPoints.GetCount() > m_Options.m_iMaxResults)
	{
		//Set the state.
		m_nState = CullExcessive;
	}
	else
	{
		//Set the state.
		m_nState = Finished;
	}
}

bool CTacticalAnalysisCoverPointSearch::IsValid(CCoverPoint& rCoverPoint) const
{
	//Ensure the cover point is valid.
	if(rCoverPoint.GetType() == CCoverPoint::COVTYPE_NONE)
	{
		return false;
	}

	//Check if the cover point is thin.
	if(rCoverPoint.GetIsThin())
	{
		return false;
	}

	//Check if the cover position is invalid.
	Vec3V vCoverPosition;
	if(!rCoverPoint.GetCoverPointPosition(RC_VECTOR3(vCoverPosition)))
	{
		return false;
	}

	//Calculate a vector from the cover to the position.
	Vec3V vCoverToPosition = Subtract(m_Options.m_vPosition, vCoverPosition);

	//Ensure the cover point provides cover.
	if(!CCover::DoesCoverPointProvideCoverFromDirection(&rCoverPoint, rCoverPoint.GetArc(), VEC3V_TO_VECTOR3(vCoverToPosition)))
	{
		return false;
	}

	//Calculate the distance.
	ScalarV scDistSq = DistSquared(m_Options.m_vPosition, vCoverPosition);

	//Check if the distance is less than the minimum.
	ScalarV scMinDistSq = ScalarVFromF32(square(m_Options.m_fMinDistance));
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		return false;
	}

	//Check if the distance is greater than the maximum.
	ScalarV scMaxDistSq = ScalarVFromF32(square(m_Options.m_fMaxDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	return true;
}

void CTacticalAnalysisCoverPointSearch::Reset()
{
	//Reset the cover points.
	m_aCoverPoints.Reset();

	//Clear the state.
	m_nState = Idle;

	//Clear the index to score.
	m_iIndexToScore = -1;
}

void CTacticalAnalysisCoverPointSearch::Start(const Options& rOptions)
{
	//Ensure we can start.
	if(!aiVerifyf(CanStart(), "The search can't be started."))
	{
		return;
	}

	//Reset the cover points.
	m_aCoverPoints.Reset();

	//Set the options.
	m_Options = rOptions;

	//Set the state.
	m_nState = Find;

	//Set the index to score.
	m_iIndexToScore = 0;
}

void CTacticalAnalysisCoverPointSearch::Update(float UNUSED_PARAM(fTimeStep))
{
	//Check the state.
	switch(m_nState)
	{
		case Idle:
		{
			break;
		}
		case Find:
		{
			DoFind();
			break;
		}
		case CullInvalid:
		{
			DoCullInvalid();
			break;
		}
		case Score:
		{
			DoScore();
			break;
		}
		case CullExcessive:
		{
			DoCullExcessive();
			break;
		}
		case Finished:
		{
			aiAssert(m_aCoverPoints.GetCount() <= m_Options.m_iMaxResults);
			break;
		}
		default:
		{
			aiAssertf(false, "Unknown state: %d.", m_nState);
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// CTacticalAnalysisCoverPoint
////////////////////////////////////////////////////////////////////////////////

void CTacticalAnalysisCoverPoint::Release(CPed* pPed)
{
	//Assert that the ped can release the point.
	aiAssert(CanRelease(pPed));

	//Release the cover point.
	pPed->ReleaseCoverPoint();

	//Release the desired cover point.
	pPed->ReleaseDesiredCoverPoint();
}

void CTacticalAnalysisCoverPoint::Reserve(CPed* pPed)
{
	//Assert that the ped can reserve the point.
	aiAssert(CanReserve(pPed));

	//Assert that the ped wants to keep their cover point.
	aiAssertf(pPed->GetPedResetFlag(CPED_RESET_FLAG_KeepCoverPoint), "The ped is reserving a cover point, but they are not going to keep it.");

	//Reserve the cover point for the ped.
	m_pCoverPoint->ReserveCoverPointForPed(pPed);

	//Set the cover point.
	pPed->SetCoverPoint(m_pCoverPoint);

	//Release the desired cover point.
	pPed->ReleaseDesiredCoverPoint();
}

////////////////////////////////////////////////////////////////////////////////
// CTacticalAnalysisCoverPoints
////////////////////////////////////////////////////////////////////////////////

CTacticalAnalysisCoverPoints::Tunables CTacticalAnalysisCoverPoints::sm_Tunables;
IMPLEMENT_TACTICAL_ANALYSIS_TUNABLES(CTacticalAnalysisCoverPoints, 0xac3ee942)

CTacticalAnalysisCoverPoints::CTacticalAnalysisCoverPoints()
: m_aPoints()
, m_aLineOfSightTests()
, m_Search()
, m_vLastPositionForSearch(V_ZERO)
, m_pTacticalAnalysis(NULL)
, m_fTimeSinceLastSearch(0.0f)
{

}

CTacticalAnalysisCoverPoints::~CTacticalAnalysisCoverPoints()
{

}

bool CTacticalAnalysisCoverPoints::ReleaseCoverPoint(CPed* pPed, CCoverPoint* pCoverPoint)
{
	//Find the index.
	int iIndex = FindPointForCoverPoint(pCoverPoint);
	if(iIndex < 0)
	{
		return false;
	}

	//Release the point.
	CTacticalAnalysisCoverPoint& rPoint = m_aPoints[iIndex];
	return ReleasePoint(pPed, rPoint);
}

bool CTacticalAnalysisCoverPoints::ReleasePoint(CPed* pPed, CTacticalAnalysisCoverPoint& rPoint)
{
	//Ensure the point can be released by the ped.
	if(!rPoint.CanRelease(pPed))
	{
		return false;
	}

	//Release the point.
	rPoint.Release(pPed);

	return true;
}

bool CTacticalAnalysisCoverPoints::ReserveCoverPoint(CPed* pPed, CCoverPoint* pCoverPoint)
{
	//Find the index.
	int iIndex = FindPointForCoverPoint(pCoverPoint);
	if(iIndex < 0)
	{
		return false;
	}

	//Reserve the point.
	CTacticalAnalysisCoverPoint& rPoint = m_aPoints[iIndex];
	return ReservePoint(pPed, rPoint);
}

bool CTacticalAnalysisCoverPoints::ReservePoint(CPed* pPed, CTacticalAnalysisCoverPoint& rPoint)
{
	//Ensure the point can be reserved by the ped.
	if(!rPoint.CanReserve(pPed))
	{
		return false;
	}

	//Reserve the point.
	rPoint.Reserve(pPed);

	return true;
}

void CTacticalAnalysisCoverPoints::Activate()
{

}

bool CTacticalAnalysisCoverPoints::CanStartSearch() const
{
	//Ensure we can start a search.
	if(!m_Search.CanStart())
	{
		return false;
	}

	return true;
}

void CTacticalAnalysisCoverPoints::Deactivate()
{
	//Reset the points.
	ResetPoints();

	//Reset the line of sight tests.
	ResetLineOfSightTests();

	//Reset the search.
	m_Search.Reset();

	//Clear the last search position.
	m_vLastPositionForSearch = Vec3V(V_ZERO);

	//Clear the time since the last search.
	m_fTimeSinceLastSearch = LARGE_FLOAT;
}

void CTacticalAnalysisCoverPoints::DecayBadRouteValues(float fTimeStep)
{
	//Iterate over the points.
	int iCount = m_aPoints.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the point.
		CTacticalAnalysisCoverPoint& rPoint = m_aPoints[i];

		//Ensure the bad route value is valid.
		if(rPoint.m_fBadRouteValue <= 0.0f)
		{
			continue;
		}

		//Decay the bad route value.
		float fDecrease = fTimeStep * CTacticalAnalysis::GetTunables().m_BadRoute.m_DecayRate;
		rPoint.m_fBadRouteValue -= fDecrease;
		if(rPoint.m_fBadRouteValue < 0.0f)
		{
			rPoint.m_fBadRouteValue = 0.0f;
		}
	}
}

void CTacticalAnalysisCoverPoints::FindNearby()
{
	//Iterate over the points.
	int iMaxNearbyToFindPerFrame = sm_Tunables.m_MaxNearbyToFindPerFrame;
	for(int i = 0; i < iMaxNearbyToFindPerFrame; ++i)
	{
		//Find the next point to find a nearby ped.
		int iIndex = FindNextPointForFindNearby();
		if(iIndex < 0)
		{
			break;
		}

		//Find a nearby ped for the point.
		FindNearby(m_aPoints[iIndex]);
	}
}

void CTacticalAnalysisCoverPoints::FindNearby(CTacticalAnalysisCoverPoint& rPoint)
{
	//Clear the time since the last attempt to find a nearby ped.
	rPoint.m_fTimeSinceLastAttemptToFindNearby = 0.0f;

	//Get the position.
	Vec3V vPosition;
	rPoint.m_pCoverPoint->GetCoverPointPosition(RC_VECTOR3(vPosition));

	//Find a nearby ped.
	float fRadius = sm_Tunables.m_RadiusForFindNearby;
	rPoint.m_pNearby = m_pTacticalAnalysis->FindNearby(vPosition, fRadius);
}

int CTacticalAnalysisCoverPoints::FindNextPointForFindNearby() const
{
	//Keep track of the best index.
	int iBestIndex = -1;

	//Iterate over the points.
	int iCount = m_aPoints.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the point.
		const CTacticalAnalysisCoverPoint& rPoint = m_aPoints[i];

		//Ensure the time has exceeded the minimum.
		float fTime = rPoint.m_fTimeSinceLastAttemptToFindNearby;
		float fMinTime = sm_Tunables.m_MinTimeBetweenAttemptsToFindNearby;
		if(fTime < fMinTime)
		{
			continue;
		}

		//Check if the best index is valid, and this point is not better.
		if(iBestIndex >= 0 && !IsPointBetterForFindNearby(rPoint, m_aPoints[iBestIndex]))
		{
			continue;
		}

		//Set the best index.
		iBestIndex = i;
	}

	return iBestIndex;
}

int CTacticalAnalysisCoverPoints::FindNextPointForLineOfSightTest() const
{
	//Keep track of the best index.
	int iBestIndex = -1;

	//Iterate over the points.
	int iCount = m_aPoints.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the point.
		const CTacticalAnalysisCoverPoint& rPoint = m_aPoints[i];

		//Ensure the time has exceeded the minimum.
		float fTime = rPoint.m_fTimeSinceLastLineOfSightTest;
		float fMinTime = sm_Tunables.m_MinTimeBetweenLineOfSightTests;
		if(fTime < fMinTime)
		{
			continue;
		}

		//Ensure a line of sight test is not running for the cover point.
		if(IsLineOfSightTestRunningForCoverPoint(rPoint.m_pCoverPoint))
		{
			continue;
		}

		//Check if the best index is valid, and this point is not better.
		if(iBestIndex >= 0 && !IsPointBetterForLineOfSightTest(rPoint, m_aPoints[iBestIndex]))
		{
			continue;
		}

		//Set the best index.
		iBestIndex = i;
	}

	return iBestIndex;
}

int CTacticalAnalysisCoverPoints::FindNextPointForStatusUpdate() const
{
	//Keep track of the best index.
	int iBestIndex = -1;

	//Iterate over the points.
	int iCount = m_aPoints.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the point.
		const CTacticalAnalysisCoverPoint& rPoint = m_aPoints[i];

		//Ensure the time has exceeded the minimum.
		float fTime = rPoint.m_fTimeSinceLastStatusUpdate;
		float fMinTime = sm_Tunables.m_MinTimeBetweenStatusUpdates;
		if(fTime < fMinTime)
		{
			continue;
		}

		//Check if the best index is valid, and this point is not better.
		if(iBestIndex >= 0 && !IsPointBetterForStatusUpdate(rPoint, m_aPoints[iBestIndex]))
		{
			continue;
		}

		//Set the best index.
		iBestIndex = i;
	}

	return iBestIndex;
}

int CTacticalAnalysisCoverPoints::FindPointForCoverPoint(CCoverPoint* pCoverPoint) const
{
	//Check if the cover point is valid.
	if(pCoverPoint)
	{
		//Iterate over the points.
		int iCount = m_aPoints.GetCount();
		for(int i = 0; i < iCount; ++i)
		{
			//Ensure the cover point matches.
			if(pCoverPoint != m_aPoints[i].m_pCoverPoint)
			{
				continue;
			}

			return i;
		}
	}

	return -1;
}

Vec3V_Out CTacticalAnalysisCoverPoints::GetSearchPosition() const
{
	return m_pTacticalAnalysis->GetPedPosition();
}

void CTacticalAnalysisCoverPoints::InitializePoint(CCoverPoint* pCoverPoint, CTacticalAnalysisCoverPoint& rPoint)
{
	//Set the cover point.
	rPoint.m_pCoverPoint = pCoverPoint;

	//Set the nearby ped.
	rPoint.m_pNearby = NULL;

	//Set the line of sight status.
	rPoint.m_nLineOfSightStatus = CTacticalAnalysisCoverPoint::LOSS_Unchecked;

	//Set the arc status.
	rPoint.m_nArcStatus = CTacticalAnalysisCoverPoint::AS_Unknown;

	//Set the timers.
	rPoint.m_fTimeSinceLastLineOfSightTest		= LARGE_FLOAT;
	rPoint.m_fTimeSinceLastAttemptToFindNearby	= LARGE_FLOAT;
	rPoint.m_fTimeSinceLastStatusUpdate			= LARGE_FLOAT;
	rPoint.m_fTimeUntilRelease					= 0.0f;
	rPoint.m_fBadRouteValue						= 0.0f;
}

bool CTacticalAnalysisCoverPoints::IsLineOfSightTestRunningForCoverPoint(CCoverPoint* pCoverPoint) const
{
	//Assert that the cover point is valid.
	aiAssert(pCoverPoint);

	//Iterate over the line of sight tests.
	int iCount = m_aLineOfSightTests.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the test is active.
		const LineOfSightTest& rTest = m_aLineOfSightTests[i];
		if(!rTest.m_AsyncProbeHelper.IsProbeActive())
		{
			continue;
		}

		//Ensure the cover point matches.
		if(pCoverPoint != rTest.m_pCoverPoint)
		{
			continue;
		}

		return true;
	}

	return false;
}

bool CTacticalAnalysisCoverPoints::IsPointValid(const CTacticalAnalysisCoverPoint& rPoint) const
{
	//Ensure the cover point is valid.
	CCoverPoint* pCoverPoint = rPoint.m_pCoverPoint;
	if(!pCoverPoint)
	{
		return false;
	}

	//Ensure the cover point is valid.
	if(pCoverPoint->GetType() == CCoverPoint::COVTYPE_NONE)
	{
		return false;
	}

	return true;
}

bool CTacticalAnalysisCoverPoints::LoadPointForCoverPoint(CCoverPoint* pCoverPoint, CTacticalAnalysisCoverPoint& rPoint) const
{
	//Find the index of the point for the cover point.
	int iIndex = FindPointForCoverPoint(pCoverPoint);
	if(iIndex < 0)
	{
		return false;
	}

	//Set the point.
	rPoint = m_aPoints[iIndex];

	return true;
}

void CTacticalAnalysisCoverPoints::MergeSearchResults()
{
	//Assert that the search is active, and finished.
	aiAssertf(m_Search.IsActive(), "The search is not active.");
	aiAssertf(m_Search.IsFinished(), "The search is not finished.");

	//Keep track of the cover point points.
	atFixedArray<CTacticalAnalysisCoverPoint, sMaxPoints> aPoints;

	//Iterate over the search results.
	int iCount = m_Search.m_aCoverPoints.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Ensure the points are not full.
		if(aPoints.IsFull())
		{
			break;
		}

		//Grab the cover point.
		CCoverPoint* pCoverPoint = m_Search.m_aCoverPoints[i].m_pCoverPoint;

		//Add the point.
		CTacticalAnalysisCoverPoint& rPoint = aPoints.Append();

		//Try to load the point for the cover point.
		if(!LoadPointForCoverPoint(pCoverPoint, rPoint))
		{
			//We were unable to load the point.  Initialize the point.
			InitializePoint(pCoverPoint, rPoint);
		}
	}

	//Set the point.
	m_aPoints = aPoints;
}

void CTacticalAnalysisCoverPoints::OnBadRoute(Vec3V_In vPosition, float fValue)
{
	//Iterate over the points.
	int iCount = m_aPoints.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the point.
		CTacticalAnalysisCoverPoint& rPoint = m_aPoints[i];

		//Ensure the cover point is valid.
		const CCoverPoint* pCoverPoint = rPoint.GetCoverPoint();
		if(!pCoverPoint)
		{
			continue;
		}
		else if(pCoverPoint->GetType() == CCoverPoint::COVTYPE_NONE)
		{
			continue;
		}

		//Ensure the cover point position is valid.
		Vec3V vCoverPointPosition;
		if(!pCoverPoint->GetCoverPointPosition(RC_VECTOR3(vCoverPointPosition)))
		{
			continue;
		}

		//Ensure the distance is valid.
		ScalarV scDistSq = DistSquared(vPosition, vCoverPointPosition);
		ScalarV scMaxDistSq = ScalarVFromF32(square(CTacticalAnalysis::GetTunables().m_BadRoute.m_MaxDistanceForTaint));
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			continue;
		}

		//Increase the bad route value.
		rPoint.m_fBadRouteValue += fValue;
	}
}

void CTacticalAnalysisCoverPoints::OnSearchFinished()
{
	//Merge the search results with the cover points.
	MergeSearchResults();

	//Set the last position for the search.
	m_vLastPositionForSearch = m_Search.GetPosition();

	//Clear the time since the last search.
	m_fTimeSinceLastSearch = 0.0f;

	//Reset the search.
	m_Search.Reset();
}

void CTacticalAnalysisCoverPoints::OnSearchStarted()
{

}

void CTacticalAnalysisCoverPoints::ProcessResultsForLineOfSightTests()
{
	//Iterate over the line of sight tests.
	int iCount = m_aLineOfSightTests.GetMaxCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the line of sight test.
		LineOfSightTest& rTest = m_aLineOfSightTests[i];

		//Ensure the test is active.
		if(!rTest.m_AsyncProbeHelper.IsProbeActive())
		{
			continue;
		}

		//Ensure the results are ready.
		ProbeStatus nProbeStatus;
		if(!rTest.m_AsyncProbeHelper.GetProbeResults(nProbeStatus))
		{
			continue;
		}

		//Reset the probe.
		rTest.m_AsyncProbeHelper.ResetProbe();

		//Find the index of the point for the cover point.
		int iIndex = FindPointForCoverPoint(rTest.m_pCoverPoint);

		//Clear the cover point.
		rTest.m_pCoverPoint = NULL;

		//Ensure the index is valid.
		if(iIndex < 0)
		{
			continue;
		}

		//Grab the point.
		CTacticalAnalysisCoverPoint& rPoint = m_aPoints[iIndex];
		rPoint.m_nLineOfSightStatus = (nProbeStatus == PS_Clear) ? CTacticalAnalysisCoverPoint::LOSS_Clear : CTacticalAnalysisCoverPoint::LOSS_Blocked;
	}
}

void CTacticalAnalysisCoverPoints::ReleasePoint(CTacticalAnalysisCoverPoint& rPoint)
{
	//Release the point.
	ReleasePoint(rPoint.GetReservedBy(), rPoint);
}

void CTacticalAnalysisCoverPoints::ReleasePoints()
{
	//Iterate over the points.
	for(int i = 0; i < m_aPoints.GetCount(); ++i)
	{
		//Release the point.
		ReleasePoint(m_aPoints[i]);
	}
}

void CTacticalAnalysisCoverPoints::RemoveInvalidPoints()
{
	//Iterate over the points.
	for(int i = 0; i < m_aPoints.GetCount(); ++i)
	{
		//Ensure the point is invalid.
		CTacticalAnalysisCoverPoint& rPoint = m_aPoints[i];
		if(IsPointValid(rPoint))
		{
			continue;
		}

		//Release the point.
		ReleasePoint(rPoint);

		//Remove the point.
		m_aPoints.DeleteFast(i--);
	}
}

void CTacticalAnalysisCoverPoints::ResetLineOfSightTests()
{
	//Iterate over the line of sight tests.
	for(int i = 0; i < m_aLineOfSightTests.GetMaxCount(); ++i)
	{
		//Reset the line of sight test.
		m_aLineOfSightTests[i].m_AsyncProbeHelper.ResetProbe();
	}
}

void CTacticalAnalysisCoverPoints::ResetPoints()
{
	//Reset the points.
	m_aPoints.Reset();
}

bool CTacticalAnalysisCoverPoints::ShouldStartSearch() const
{
	//Check if we can start a search.
	if(!CanStartSearch())
	{
		return false;
	}

	//Check if we should start a search due to the time.
	if(ShouldStartSearchDueToTime())
	{
		return true;
	}
	//Check if we should start a search due to the distance.
	else if(ShouldStartSearchDueToDistance())
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CTacticalAnalysisCoverPoints::ShouldStartSearchDueToDistance() const
{
	//Grab the search position.
	Vec3V vSearchPosition = GetSearchPosition();

	//Grab the last position for the search.
	Vec3V vLastPositionForSearch = m_vLastPositionForSearch;

	//Ensure the distance has exceeded the threshold.
	ScalarV scDistSq = DistSquared(vSearchPosition, vLastPositionForSearch);
	ScalarV scMinDistSq = ScalarVFromF32(square(sm_Tunables.m_MinDistanceMovedToStartSearch));
	if(IsLessThanAll(scDistSq, scMinDistSq))
	{
		return false;
	}

	return true;
}

bool CTacticalAnalysisCoverPoints::ShouldStartSearchDueToTime() const
{
	//Ensure the time has exceeded the threshold.
	float fMaxTimeBetweenSearches = sm_Tunables.m_MaxTimeBetweenSearches;
	if(m_fTimeSinceLastSearch < fMaxTimeBetweenSearches)
	{
		return false;
	}

	return true;
}

void CTacticalAnalysisCoverPoints::StartNewLineOfSightTests()
{
	//Iterate over the line of sight tests.
	int iNumTests = m_aLineOfSightTests.GetMaxCount();
	for(int i = 0; i < iNumTests; ++i)
	{
		//Ensure the test is not active.
		LineOfSightTest& rTest = m_aLineOfSightTests[i];
		if(rTest.m_AsyncProbeHelper.IsProbeActive())
		{
			continue;
		}

		//Find the next point for line of sight test.
		int iIndex = FindNextPointForLineOfSightTest();
		if(iIndex < 0)
		{
			break;
		}

		//Grab the point.
		CTacticalAnalysisCoverPoint& rPoint = m_aPoints[iIndex];

		//Grab the cover point.
		CCoverPoint* pCoverPoint = rPoint.m_pCoverPoint;
		aiAssert(pCoverPoint);

		//Calculate the vantage position of the cover point.
		Vec3V vCoverVantagePosition;
		if(!m_pTacticalAnalysis->CalculateVantagePosition(pCoverPoint, vCoverVantagePosition))
		{
			continue;
		}

		//Start a line of sight test.
		if(!m_pTacticalAnalysis->StartLineOfSightTest(rTest.m_AsyncProbeHelper, vCoverVantagePosition))
		{
			continue;
		}

		//Set the cover point.
		rTest.m_pCoverPoint = pCoverPoint;

		//Set the time since the last line of sight test.
		rPoint.m_fTimeSinceLastLineOfSightTest = 0.0f;
	}
}

void CTacticalAnalysisCoverPoints::StartSearch()
{
	//Create the options.
	Vec3V vPosition = GetSearchPosition();
	int iMaxResults = sMaxPoints;
	float fMinDistance = sm_Tunables.m_MinDistanceForSearch;
	float fMaxDistance = sm_Tunables.m_MaxDistanceForSearch;
	CTacticalAnalysisCoverPointSearch::Options options(vPosition, iMaxResults, fMinDistance, fMaxDistance);

	//Start the search.
	m_Search.Start(options);

	//Note that a search started.
	OnSearchStarted();
}

void CTacticalAnalysisCoverPoints::Update(float fTimeStep)
{
	//Update the timers.
	UpdateTimers(fTimeStep);

	//Update the search.
	UpdateSearch(fTimeStep);

	//Update the points.
	UpdatePoints(fTimeStep);
}

void CTacticalAnalysisCoverPoints::UpdateArcs()
{
	//The arc updates are time-sliced on a per-frame basis.
	static const int sNumFramesBetweenUpdates = 4;

	//Grab the frame count.
	u32 uFrameCount = fwTimer::GetSystemFrameCount();

	//Grab the ped position.
	Vec3V vPedPosition = m_pTacticalAnalysis->GetPedPosition();

	//Iterate over the points.
	int iCount = m_aPoints.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Time slice the updates, on a per-frame basis.
		bool bUpdateThisFrame = (((i + uFrameCount) % sNumFramesBetweenUpdates) == 0);
		if(!bUpdateThisFrame)
		{
			continue;
		}

		//Grab the point.
		CTacticalAnalysisCoverPoint& rPoint = m_aPoints[i];

		//Grab the cover point.
		CCoverPoint* pCoverPoint = rPoint.m_pCoverPoint;

		//Keep track of whether cover is provided.
		bool bProvidesCover = false;

		//Check if the cover position is valid.
		Vec3V vCoverPosition;
		if(pCoverPoint->GetCoverPointPosition(RC_VECTOR3(vCoverPosition)))
		{
			//Calculate a vector from the cover point to the ped.
			Vec3V vCoverToPed = Subtract(vPedPosition, vCoverPosition);

			//Check if the cover point provides cover.
			bProvidesCover = CCover::DoesCoverPointProvideCoverFromDirection(pCoverPoint, pCoverPoint->GetArc(), VEC3V_TO_VECTOR3(vCoverToPed));
		}

		//Set the arc status.
		rPoint.m_nArcStatus = bProvidesCover ? CTacticalAnalysisCoverPoint::AS_ProvidesCover : CTacticalAnalysisCoverPoint::AS_DoesNotProvideCover;
	}
}

void CTacticalAnalysisCoverPoints::UpdateLineOfSightTests()
{
	//Process the results of the line of sight tests.
	ProcessResultsForLineOfSightTests();

	//Start new line of sight tests.
	StartNewLineOfSightTests();
}

void CTacticalAnalysisCoverPoints::UpdatePoints(float fTimeStep)
{
	//Remove invalid points.
	RemoveInvalidPoints();

	//Update the timers for the points.
	UpdateTimersForPoints(fTimeStep);

	//Update the arcs.
	UpdateArcs();

	//Update the line of sight tests.
	UpdateLineOfSightTests();

	//Update the statuses.
	UpdateStatuses();

	//Find nearby peds.
	FindNearby();

	//Decay the bad route values.
	DecayBadRouteValues(fTimeStep);
}

void CTacticalAnalysisCoverPoints::UpdateSearch(float fTimeStep)
{
	//Check if we should start a search.
	if(ShouldStartSearch())
	{
		//Start a search.
		StartSearch();
	}

	//Check if the search is active.
	if(m_Search.IsActive())
	{
		//Update the search.
		m_Search.Update(fTimeStep);

		//Check if the search is finished.
		if(m_Search.IsFinished())
		{
			//Note that the search finished.
			OnSearchFinished();
		}
	}
}

void CTacticalAnalysisCoverPoints::UpdateStatuses()
{
	//Update the status helper.
	m_StatusHelper.Update();

	//Check if the status helper is finished.
	if(m_StatusHelper.IsFinished())
	{
		//Check if the point is valid.
		CTacticalAnalysisCoverPoint* pPoint = GetPointForCoverPoint(m_StatusHelper.GetCoverPoint());
		if(pPoint)
		{
			//Copy the status.
			if(m_StatusHelper.GetFailed())
			{
				pPoint->SetStatus(0);
			}
			else
			{
				pPoint->SetStatus(m_StatusHelper.GetStatus());
			}
		}

		//Reset the status helper.
		m_StatusHelper.Reset();
	}

	//Check if the status helper is not active.
	if(!m_StatusHelper.IsActive())
	{
		//Find the next point for a status update.
		int iIndex = FindNextPointForStatusUpdate();
		if(iIndex >= 0)
		{
			//Clear the time since the last status update.
			m_aPoints[iIndex].m_fTimeSinceLastStatusUpdate = 0.0f;

			//Start the update.
			m_StatusHelper.Start(m_aPoints[iIndex].GetCoverPoint());
		}
	}
}

void CTacticalAnalysisCoverPoints::UpdateTimers(float fTimeStep)
{
	//Increase the time since the last search.
	m_fTimeSinceLastSearch += fTimeStep;
}

void CTacticalAnalysisCoverPoints::UpdateTimersForPoints(float fTimeStep)
{
	//Iterate over the points.
	int iCount = m_aPoints.GetCount();
	for(int i = 0; i < iCount; ++i)
	{
		//Grab the point.
		CTacticalAnalysisCoverPoint& rPoint = m_aPoints[i];

		//Increase the time since the last line of sight test.
		rPoint.m_fTimeSinceLastLineOfSightTest += fTimeStep;

		//Increase the time since the last attempt to find a nearby ped.
		rPoint.m_fTimeSinceLastAttemptToFindNearby += fTimeStep;

		//Increase the time since the last status update.
		rPoint.m_fTimeSinceLastStatusUpdate += fTimeStep;

		//Check if the time until release is valid.
		float& fTimeUntilRelease = rPoint.m_fTimeUntilRelease;
		if(fTimeUntilRelease > 0.0f)
		{
			//Check if the time until release has elapsed.
			fTimeUntilRelease -= fTimeStep;
			if(fTimeUntilRelease <= 0.0f)
			{
				//Release the point.
				ReleasePoint(rPoint.GetReservedBy(), rPoint);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// CTacticalAnalysis
////////////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL(CTacticalAnalysis, CONFIGURED_FROM_FILE, atHashString("CTacticalAnalysis",0xa203ea4b));

CTacticalAnalysis::Tunables CTacticalAnalysis::sm_Tunables;
IMPLEMENT_TACTICAL_ANALYSIS_TUNABLES(CTacticalAnalysis, 0xa203ea4b)

CTacticalAnalysis::CTacticalAnalysis(const CPed* pPed)
: m_NavMeshPoints()
, m_CoverPoints()
, m_pPed(pPed)
, m_fTimeSinceHadReferences(0.0f)
, m_uReferences(0)
, m_uRunningFlags(0)
{
	//Set the tactical analysis on the nav mesh points.
	m_NavMeshPoints.SetTacticalAnalysis(this);

	//Set the tactical analysis on the cover points.
	m_CoverPoints.SetTacticalAnalysis(this);

	//Set the tactical analysis on the cover point search.
	m_CoverPoints.GetSearch().SetTacticalAnalysis(this);
}

CTacticalAnalysis::~CTacticalAnalysis()
{

}

void CTacticalAnalysis::OnRouteTooCloseTooTarget(Vec3V_In vPosition)
{
	//Note that a bad route occurred.
	OnBadRoute(vPosition, sm_Tunables.m_BadRoute.m_ValueForTooCloseToTarget);
}

void CTacticalAnalysis::OnUnableToFindRoute(Vec3V_In vPosition)
{
	//Note that a bad route occurred.
	OnBadRoute(vPosition, sm_Tunables.m_BadRoute.m_ValueForUnableToFind);
}

void CTacticalAnalysis::Release()
{
	//Remove a reference.
	aiAssertf(m_uReferences > 0, "No references have been added.");
	--m_uReferences;
}

CTacticalAnalysis* CTacticalAnalysis::Find(const CPed& rPed)
{
	//Iterate over the pool.
	CTacticalAnalysis::Pool* pPool = CTacticalAnalysis::GetPool();
	for(s32 i = 0; i < pPool->GetSize(); ++i)
	{
		//Grab the tactical analysis.
		CTacticalAnalysis* pTacticalAnalysis = pPool->GetSlot(i);
		if(!pTacticalAnalysis)
		{
			continue;
		}

		//Ensure the ped matches.
		if(&rPed != pTacticalAnalysis->GetPed())
		{
			continue;
		}

		return pTacticalAnalysis;
	}

	return NULL;
}

void CTacticalAnalysis::Init(unsigned UNUSED_PARAM(initMode))
{
	//Init the pools.
	CTacticalAnalysis::InitPool(MEMBUCKET_GAMEPLAY);
}

bool CTacticalAnalysis::IsActive(const CPed& rPed)
{
	return (Find(rPed) != NULL);
}

CTacticalAnalysis* CTacticalAnalysis::Request(const CPed& rPed)
{
	//Ensure we are enabled.
	if(!sm_Tunables.m_Enabled)
	{
		return NULL;
	}

	//Ensure we can request a tactical analysis for the ped.
	if(!CanRequest(rPed))
	{
		return NULL;
	}

	//Find the tactical analysis for the ped.
	CTacticalAnalysis* pTacticalAnalysis = Find(rPed);
	if(!pTacticalAnalysis)
	{
		//Ensure there is room in the pool.
		if(CTacticalAnalysis::GetPool()->GetNoOfFreeSpaces() <= 0)
		{
			return NULL;
		}

		//Create a new tactical analysis.
		pTacticalAnalysis = rage_new CTacticalAnalysis(&rPed);
	}

	//Request the tactical analysis.
	pTacticalAnalysis->Request();

	return pTacticalAnalysis;
}

void CTacticalAnalysis::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	//Shut down the pools.
	CTacticalAnalysis::ShutdownPool();
}

void CTacticalAnalysis::Update()
{
	//Ensure the pool is valid.
	CTacticalAnalysis::Pool* pPool = CTacticalAnalysis::GetPool();
	if(!pPool)
	{
		return;
	}

	//Grab the time step.
	const float fTimeStep = fwTimer::GetTimeStep();

	//Iterate over the pool.
	for(s32 i = 0; i < pPool->GetSize(); ++i)
	{
		//Grab the tactical analysis.
		CTacticalAnalysis* pTacticalAnalysis = pPool->GetSlot(i);
		if(!pTacticalAnalysis)
		{
			continue;
		}

		//Ensure the ped is valid.
		if(!pTacticalAnalysis->GetPed())
		{
			delete pTacticalAnalysis;
			continue;
		}

		//Update the tactical analysis.
		pTacticalAnalysis->Update(fTimeStep);

		//Check if we haven't had any references for a while.
		float fMaxTimeWithNoReferences = sm_Tunables.m_MaxTimeWithNoReferences;
		float fTimeSinceHadReferences = pTacticalAnalysis->GetTimeSinceHadReferences();
		if(fTimeSinceHadReferences > fMaxTimeWithNoReferences)
		{
			//Free the tactical analysis.
			delete pTacticalAnalysis;
			continue;
		}
	}
}

void CTacticalAnalysis::Activate()
{
	//Assert that we are not active.
	aiAssertf(!IsActive(), "The tactical analysis is active.");

	//Activate the nav mesh points.
	m_NavMeshPoints.Activate();

	//Activate the cover points.
	m_CoverPoints.Activate();

	//Set the flag.
	m_uRunningFlags.SetFlag(RF_IsActive);
}

float CTacticalAnalysis::CalculateSpeedForPed() const
{
	//Get the dominant entity.
	const CPhysical* pPhysical = GetDominantEntity();
	aiAssert(pPhysical);

	//Grab the speed.
	float fSpeed = pPhysical->GetVelocity().XYMag();

	return fSpeed;
}

bool CTacticalAnalysis::CalculateVantagePosition(CCoverPoint* pCoverPoint, Vec3V_InOut vVantagePosition) const
{
	//Grab the ped position.
	Vec3V vPedPosition = m_pPed->GetTransform().GetPosition();

	//Ensure the cover position is valid.
	Vec3V vCoverPosition;
	if(!pCoverPoint->GetCoverPointPosition(RC_VECTOR3(vCoverPosition)))
	{
		return false;
	}

	//Calculate a vector from cover to the ped.
	Vec3V vCoverToPed = Subtract(vPedPosition, vCoverPosition);

	//Ensure the coordinates can be found.
	Vector3 vResultCoors;
	if(!CCover::FindCoordinatesCoverPointNotReserved(pCoverPoint, VEC3V_TO_VECTOR3(vCoverToPed), vResultCoors, &RC_VECTOR3(vVantagePosition)))
	{
		return false;
	}

	return true;
}

bool CTacticalAnalysis::CalculateVantagePositionForPed(Vec3V_InOut vVantagePosition) const
{
	//Check if the ped has a cover point.
	CCoverPoint* pCoverPoint = m_pPed->GetCoverPoint();
	if(pCoverPoint)
	{
		return CalculateVantagePosition(pCoverPoint, vVantagePosition);
	}

	//Grab the head position.
	Vec3V vHeadPosition = VECTOR3_TO_VEC3V(m_pPed->GetBonePositionCached(BONETAG_HEAD));

	//Grab the ped position.
	Vec3V vPedPosition = m_pPed->GetTransform().GetPosition();

	//Set the vantage position.
	vVantagePosition.SetX(vPedPosition.GetX());
	vVantagePosition.SetY(vPedPosition.GetY());
	vVantagePosition.SetZ(vHeadPosition.GetZ());

	return true;
}

void CTacticalAnalysis::Deactivate()
{
	//Assert that we are active.
	aiAssertf(IsActive(), "The tactical analysis is not active.");

	//Deactivate the nav mesh points.
	m_NavMeshPoints.Deactivate();

	//Deactivate the cover points.
	m_CoverPoints.Deactivate();

	//Clear the flag.
	m_uRunningFlags.ClearFlag(RF_IsActive);
}

const CPed* CTacticalAnalysis::FindNearby(Vec3V_In vPosition, float fRadius) const
{
	//Grab the spatial array.
	const CSpatialArray& rSpatialArray = CPed::GetSpatialArray();

	//Search for a ped near the position.
	CSpatialArray::FindResult result[1];
	int iFound = rSpatialArray.FindInSphere(vPosition, fRadius, &result[0], 1);
	if(iFound <= 0)
	{
		return NULL;
	}
	else
	{
		return CPed::GetPedFromSpatialArrayNode(result[0].m_Node);
	}
}

const CPhysical* CTacticalAnalysis::GetDominantEntity() const
{
	//Check if the ped is in a vehicle.
	CVehicle* pVehicle = m_pPed->GetVehiclePedInside();
	if(pVehicle)
	{
		return pVehicle;
	}

	//Check if the ped is on a mount.
	CPed* pMount = m_pPed->GetMyMount();
	if(pMount && m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnMount))
	{
		return pMount;
	}

	return m_pPed;
}

Vec3V_Out CTacticalAnalysis::GetPedPosition() const
{
	return m_pPed->GetTransform().GetPosition();
}

void CTacticalAnalysis::OnBadRoute(Vec3V_In vPosition, float fValue)
{
	//Notify the cover points.
	m_CoverPoints.OnBadRoute(vPosition, fValue);

	//Notify the nav mesh points.
	m_NavMeshPoints.OnBadRoute(vPosition, fValue);
}

bool CTacticalAnalysis::ShouldActivate() const
{
	//Assert that we are not active.
	aiAssertf(!IsActive(), "The tactical analysis is active.");

	//Calculate the speed for the ped.
	float fSpeed = CalculateSpeedForPed();

	//Ensure the speed exceeds the threshold.
	float fMaxSpeed = sm_Tunables.m_MaxSpeedToActivate;
	if(fSpeed > fMaxSpeed)
	{
		return false;
	}

	return true;
}

bool CTacticalAnalysis::ShouldDeactivate() const
{
	//Assert that we are active.
	aiAssertf(IsActive(), "The tactical analysis is not active.");

	//Calculate the speed for the ped.
	float fSpeed = CalculateSpeedForPed();

	//Ensure the speed exceeds the threshold.
	float fMinSpeed = sm_Tunables.m_MinSpeedToDeactivate;
	if(fSpeed < fMinSpeed)
	{
		return false;
	}

	return true;
}

bool CTacticalAnalysis::StartLineOfSightTest(CAsyncProbeHelper& rHelper, Vec3V_In vPosition) const
{
	//Calculate the vantage position of the ped.
	Vec3V vPedVantagePosition;
	if(!CalculateVantagePositionForPed(vPedVantagePosition))
	{
		return false;
	}

	//Calculate the exclude entity.
	const CPhysical* pExcludeEntity = GetDominantEntity();

	//Start the line of sight test.
	WorldProbe::CShapeTestProbeDesc probeData;
	probeData.SetStartAndEnd(VEC3V_TO_VECTOR3(vPosition), VEC3V_TO_VECTOR3(vPedVantagePosition));
	probeData.SetContext(WorldProbe::ELosCombatAI);
	probeData.SetExcludeEntity(pExcludeEntity);
	probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeData.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU); 
	rHelper.StartTestLineOfSight(probeData);

#if DEBUG_DRAW
	//Check if we should render the line of sight test.
	if(sm_Tunables.m_Rendering.m_Enabled && sm_Tunables.m_Rendering.m_LineOfSightTests)
	{
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(vPosition), VEC3V_TO_VECTOR3(vPedVantagePosition), Color_yellow, Color_yellow);
	}
#endif

	return true;
}

#if __BANK
void CTacticalAnalysis::Render()
{
	//Iterate over the pool.
	CTacticalAnalysis::Pool* pPool = CTacticalAnalysis::GetPool();
	for(s32 i = 0; i < pPool->GetSize(); ++i)
	{
		//Grab the tactical analysis.
		CTacticalAnalysis* pTacticalAnalysis = pPool->GetSlot(i);
		if(!pTacticalAnalysis)
		{
			continue;
		}

		//Render the tactical analysis.
		pTacticalAnalysis->RenderDebug();
	}
}
#endif

void CTacticalAnalysis::Request()
{
	//Add a reference.
	++m_uReferences;
	aiAssertf(m_uReferences > 0, "Too many references have been added.");
}

void CTacticalAnalysis::Update(float fTimeStep)
{
	//Update the timers.
	UpdateTimers(fTimeStep);

	//Check if we are not active.
	if(!IsActive())
	{
		//Check if we should activate.
		if(ShouldActivate())
		{
			//Activate.
			Activate();
		}
	}
	else
	{
		//Check if we should deactivate.
		if(ShouldDeactivate())
		{
			//Deactivate.
			Deactivate();
		}
	}

	//Check if we are active.
	if(IsActive())
	{
		//Update the nav mesh points.
		m_NavMeshPoints.Update(fTimeStep);

		//Update the cover points.
		m_CoverPoints.Update(fTimeStep);
	}
}

void CTacticalAnalysis::UpdateTimers(float fTimeStep)
{
	//Check if we have references.
	if(HasReferences())
	{
		//Clear the time since we last had references.
		m_fTimeSinceHadReferences = 0.0f;
	}
	else
	{
		//Increase the time since we had references.
		m_fTimeSinceHadReferences += fTimeStep;
	}
}

#if __BANK
void CTacticalAnalysis::RenderDebug()
{
	//Ensure rendering is enabled.
	if(!sm_Tunables.m_Rendering.m_Enabled)
	{
		return;
	}

	//Check if we should render the cover points.
	if(sm_Tunables.m_Rendering.m_CoverPoints)
	{
		//Iterate over the cover points.
		int iCount = m_CoverPoints.GetNumPoints();
		for(int i = 0; i < iCount; ++i)
		{
			//Grab the cover point.
			const CTacticalAnalysisCoverPoint& rPoint = m_CoverPoints.GetPointForIndex(i);

			//Ensure the cover point is valid.
			CCoverPoint* pCoverPoint = rPoint.GetCoverPoint();
			if(!pCoverPoint)
			{
				continue;
			}
			else if(pCoverPoint->GetType() == CCoverPoint::COVTYPE_NONE)
			{
				continue;
			}

			//Ensure the position is valid.
			Vector3 vPosition;
			if(!pCoverPoint->GetCoverPointPosition(vPosition))
			{
				continue;
			}

			//Keep track of the offset.
			float fOffset = 0.0f;

			//Check if we should render the position.
			if(sm_Tunables.m_Rendering.m_Position)
			{
				//Calculate the color.
				Color32 color = Color_green;

				//Draw a sphere.
				grcDebugDraw::Sphere(vPosition + (ZAXIS * fOffset++), 0.25f, color);
			}

			//Check if we should render the line of sight status.
			if(sm_Tunables.m_Rendering.m_LineOfSightStatus)
			{
				//Calculate the color.
				Color32 color;
				if(rPoint.IsLineOfSightClear())
				{
					color = Color_green;
				}
				else if(rPoint.IsLineOfSightBlocked())
				{
					color = Color_red;
				}
				else
				{
					color = Color_yellow;
				}

				//Draw a sphere.
				grcDebugDraw::Sphere(vPosition + (ZAXIS * fOffset++), 0.1f, color);
			}

			//Check if we should render the arc status.
			if(sm_Tunables.m_Rendering.m_ArcStatus)
			{
				//Calculate the color.
				Color32 color;
				if(rPoint.DoesArcProvideCover())
				{
					color = Color_green;
				}
				else if(rPoint.DoesArcNotProvideCover())
				{
					color = Color_red;
				}
				else
				{
					color = Color_yellow;
				}

				//Draw a sphere.
				grcDebugDraw::Sphere(vPosition + (ZAXIS * fOffset++), 0.1f, color);
			}

			//Check if we should render the reserved.
			if(sm_Tunables.m_Rendering.m_Reserved)
			{
				//Calculate the color.
				Color32 color;
				if(!rPoint.IsReserved())
				{
					color = Color_green;
				}
				else
				{
					color = Color_yellow;
				}

				//Draw a sphere.
				grcDebugDraw::Sphere(vPosition + (ZAXIS * fOffset++), 0.1f, color);
			}

			//Check if we should render the nearby.
			if(sm_Tunables.m_Rendering.m_Nearby)
			{
				//Calculate the color.
				Color32 color;
				if(!rPoint.HasNearby())
				{
					color = Color_green;
				}
				else
				{
					color = Color_red;
				}

				//Draw a sphere.
				grcDebugDraw::Sphere(vPosition + (ZAXIS * fOffset++), 0.1f, color);
			}

			//Check if we should render the bad route value.
			if(sm_Tunables.m_Rendering.m_BadRouteValue)
			{
				//Draw the text.
				char buf[64];
				formatf(buf, "Bad route value: %.2f", rPoint.m_fBadRouteValue);
				grcDebugDraw::Text(vPosition + (ZAXIS * fOffset++), Color_blue, buf);
			}

			//Check if we should render the reservation.
			if(sm_Tunables.m_Rendering.m_Reservations && rPoint.IsReserved())
			{
				//Draw a line.
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(rPoint.GetReservedBy()->GetTransform().GetPosition()), vPosition, Color_LightBlue);
			}
		}
	}

	//Check if we should render nav mesh points.
	if(sm_Tunables.m_Rendering.m_NavMeshPoints)
	{
		//Iterate over the nav mesh points.
		int iCount = m_NavMeshPoints.GetNumPoints();
		for(int i = 0; i < iCount; ++i)
		{
			//Ensure the nav mesh point is valid.
			const CTacticalAnalysisNavMeshPoint& rPoint = m_NavMeshPoints.GetPointForIndex(i);
			if(!rPoint.IsValid())
			{
				continue;
			}

			//Grab the position.
			Vector3 vPosition = VEC3V_TO_VECTOR3(rPoint.GetPosition());

			//Keep track of the offset.
			float fOffset = 0.0f;

			//Check if we should render the position.
			if(sm_Tunables.m_Rendering.m_Position)
			{
				//Calculate the color.
				Color32 color = Color_green;

				//Draw a sphere.
				grcDebugDraw::Sphere(vPosition + (ZAXIS * fOffset++), 0.25f, color);
			}

			//Check if we should render the line of sight status.
			if(sm_Tunables.m_Rendering.m_LineOfSightStatus)
			{
				//Calculate the color.
				Color32 color;
				if(rPoint.IsLineOfSightClear())
				{
					color = Color_green;
				}
				else if(rPoint.IsLineOfSightBlocked())
				{
					color = Color_red;
				}
				else
				{
					color = Color_yellow;
				}

				//Draw a sphere.
				grcDebugDraw::Sphere(vPosition + (ZAXIS * fOffset++), 0.1f, color);
			}

			//Check if we should render the reserved.
			if(sm_Tunables.m_Rendering.m_Reserved)
			{
				//Calculate the color.
				Color32 color;
				if(!rPoint.IsReserved())
				{
					color = Color_green;
				}
				else
				{
					color = Color_yellow;
				}

				//Draw a sphere.
				grcDebugDraw::Sphere(vPosition + (ZAXIS * fOffset++), 0.1f, color);
			}

			//Check if we should render the nearby.
			if(sm_Tunables.m_Rendering.m_Nearby)
			{
				//Calculate the color.
				Color32 color;
				if(!rPoint.HasNearby())
				{
					color = Color_green;
				}
				else
				{
					color = Color_red;
				}

				//Draw a sphere.
				grcDebugDraw::Sphere(vPosition + (ZAXIS * fOffset++), 0.1f, color);
			}

			//Check if we should render the bad route value.
			if(sm_Tunables.m_Rendering.m_BadRouteValue)
			{
				//Draw the text.
				char buf[64];
				formatf(buf, "Bad route value: %.2f", rPoint.m_fBadRouteValue);
				grcDebugDraw::Text(vPosition + (ZAXIS * fOffset++), Color_blue, buf);
			}

			//Check if we should render the reservation.
			if(sm_Tunables.m_Rendering.m_Reservations && rPoint.IsReserved())
			{
				//Draw a line.
				grcDebugDraw::Line(VEC3V_TO_VECTOR3(rPoint.GetReservedBy()->GetTransform().GetPosition()), vPosition, Color_LightBlue);
			}
		}
	}
}
#endif

bool CTacticalAnalysis::CanRequest(const CPed& rPed)
{
	//Ensure the ped is a player.
	if(!rPed.IsPlayer())
	{
		return false;
	}

	return true;
}
