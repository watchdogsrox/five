//
// task/climbdetector.cpp
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

// File header
#include "task/movement/climbing/climbdetector.h"

// Rage headers
#include "phbound/boundcomposite.h"

// Framework headers
#include "fwmaths/geomutil.h"

// Game headers
#include "camera/caminterface.h"
#include "modelinfo/pedmodelinfo.h"
#include "network/NetworkInterface.h"
#include "peds/ped.h"
#include "peds/PedCapsule.h"
#include "peds/PedIntelligence.h"
#include "physics/physics.h"
#include "system/control.h"
#include "task/Combat/Cover/TaskCover.h"
#include "task/movement/climbing/climbhandhold.h"
#include "task/Scenario/ScenarioPoint.h"

#include "objects/door.h"

#include "Task/Vehicle/TaskCarUtils.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/Movement/Climbing/TaskVault.h"

#include "vehicles/heli.h"

#include "physics/WaterTestHelper.h"

AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()

#if __ASSERT
	#define CLIMB_DEBUGF(fmt,...)		ASSERT_ONLY(taskDebugf2(fmt,##__VA_ARGS__))
#else
	#define CLIMB_DEBUGF(fmt,...)
#endif

PF_PAGE(GTAClimbDetector, "GTA climb detector");
PF_GROUP(ClimbDetector);
PF_LINK(GTAClimbDetector, ClimbDetector);

PF_TIMER(ClimbAll, ClimbDetector);
PF_TIMER(ClimbSearch, ClimbDetector);
PF_TIMER(ClimbCompute, ClimbDetector);
PF_TIMER(ClimbPedTest, ClimbDetector);
PF_TIMER(ClimbValidate, ClimbDetector);

PF_TIMER_OFF(ClimbGetMatrix, ClimbDetector);
PF_TIMER_OFF(ClimbEdgeDetection, ClimbDetector);
PF_TIMER_OFF(ClimbEdgeSort, ClimbDetector);

PF_TIMER(ClimbSphereTest, ClimbDetector);
PF_TIMER(ClimbCapsuleTest, ClimbDetector);
PF_TIMER(ClimbLineTest, ClimbDetector);

// Static initialization
#if __BANK
bank_bool			CClimbDetector::ms_bDebugDrawSearch			= false;
bank_bool			CClimbDetector::ms_bDebugDrawIntersections	= false;
bank_bool			CClimbDetector::ms_bDebugDrawCompute		= false;
bank_bool			CClimbDetector::ms_bDebugDrawValidate		= false;
bank_bool			CClimbDetector::ms_bDebugDraw				= false;
Color32				CClimbDetector::ms_debugDrawColour			= Color_white;
CDebugDrawStore*	CClimbDetector::ms_pDebugDrawStore			= NULL;
bank_float			CClimbDetector::ms_fHeadingModifier			= 0.0f;
#endif // __BANK

// Constants
const u32			CLIMB_DETECTOR_DEFAULT_TEST_FLAGS		= ArchetypeFlags::GTA_MAP_TYPE_MOVER	| 
																ArchetypeFlags::GTA_GLASS_TYPE		| 
																ArchetypeFlags::GTA_VEHICLE_TYPE	| 
																ArchetypeFlags::GTA_OBJECT_TYPE		|
																ArchetypeFlags::GTA_PED_TYPE		|
																ArchetypeFlags::GTA_RAGDOLL_TYPE	|
																ArchetypeFlags::GTA_HORSE_RAGDOLL_TYPE;
#if __BANK
dev_u32				CLIMB_DETECTOR_DEBUG_EXPIRY_TIME		= 3000;
#endif // __BANK

static dev_float ms_fAutoVaultHorizontalTestDistances[CLIMB_DETECTOR_MAX_HORIZ_TESTS] = 
{
	2.5f,
	2.5f,
	1.5f,
	1.5f,
	1.5f,
	1.5f,
};

static dev_float ms_fOnHorseHorizontalTestDistances[CLIMB_DETECTOR_MAX_HORIZ_TESTS] = 
{
	15.0f,
	15.0f,
	15.0f,
	15.0f,
	0.0f,
	0.0f,
};

static const float ms_DefaultHorizontalTestDistance = 5.0f;

static const float CLIMB_EPSILON = 0.000001f;

bool IsIntersectionClimbable(const WorldProbe::CShapeTestHitPoint& intersection)
{
	TUNE_GROUP_BOOL(VAULTING,bIgnoreNotClimbableFlag,false);
	return !PGTAMATERIALMGR->GetPolyFlagNotClimbable(intersection.GetMaterialId()) || bIgnoreNotClimbableFlag;
}

bool CanFindFurtherIntersections(const WorldProbe::CShapeTestHitPoint& intersection)
{
	//! Don't find intersections behind non climbable geometry.
	if(!IsIntersectionClimbable(intersection))
	{
		return false;
	}

	//! Don't find intersections behind a ped - don't ever want the possibility of climbing through peds.
	const phInst* pHitInst = intersection.GetHitInst();
	if(pHitInst)
	{
		const CEntity* pEntity = intersection.GetHitEntity();

		if(pEntity && pEntity->GetIsTypePed())
		{
			bool bLargePed = false;
			if(pHitInst->GetClassType() == PH_INST_FRAG_PED && static_cast<const fragInstNMGta*>(pHitInst)->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH)
			{
				bLargePed = static_cast<const fragInstNMGta*>(pHitInst)->GetTypePhysics()->GetChild(intersection.GetHitComponent())->GetUndamagedMass() >= CPed::ms_fLargeComponentMass;
			}

			const CPed* pPed = static_cast<const CPed*>(pEntity);
			// Consider large components on large ped ragdolls as valid ground
			if(pHitInst->GetClassType() != PH_INST_FRAG_PED || pPed->GetDeathState() != DeathState_Dead || !pPed->ShouldBeDead() || 
				pHitInst->GetArchetype()->GetMass() < CPed::ms_fLargePedMass || 
				bLargePed)
			{
				return false;
			}
		}
	}

	return true;
}

CClimbDetector::sShapeTestResult::sShapeTestResult()
: pProbeResult(NULL)
{
	Reset();
}

void CClimbDetector::sShapeTestResult::Reset()
{
	if(pProbeResult)
		pProbeResult->Reset();

#if __BANK
	iFrameDelay = 0;
#endif
}

CClimbDetector::sShapeTestResult::~sShapeTestResult()
{
	if(pProbeResult)
		delete pProbeResult;
}

////////////////////////////////////////////////////////////////////////////////

CClimbDetector::CClimbDetector(CPed& ped)
: m_Ped(ped)
, m_flags(0)
, m_eHorizontalTestPhase(eATS_Free)
, m_bDoHorizontalLineTests(false)
, m_bDoAsyncTests(false)
, m_uLastTimeDetectedHandhold(0)
, m_uLastOnStairsTime(0)
, m_uAutoVaultDetectedStartTimeMs(0)
, m_uAutoVaultCooldownDelayMs(0)
, m_uAutoVaultCooldownEndTimeMs(0)
, m_pLastClimbPhysical(NULL)
, m_uAutoVaultPhysicalDelayEndTime(0)
, m_bBlockJump(false)
{
}

////////////////////////////////////////////////////////////////////////////////

void CClimbDetector::Process(Matrix34 *overRideMtx, sPedTestParams *pPedParams, sPedTestResults *pPedResults, bool bDoClimbingCheck)
{
	PF_FUNC(ClimbAll);

	//! Ensure we don't run this check if climbing already.
	if(bDoClimbingCheck && m_Ped.GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))
	{
		return;
	}

	if(m_Ped.GetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb))
	{
		ResetFlags();

		m_bBlockJump = false;

		if(m_Ped.GetPedResetFlag(CPED_RESET_FLAG_SearchingForAutoVaultClimb))
		{
			m_flags.SetFlag(DF_AutoVault);
		}

		if(pPedParams && pPedParams->bOnlyDoJumpVault)
		{
			m_flags.SetFlag(DF_JumpVaultTestOnly);
		}

		if(pPedParams && pPedParams->bForceSynchronousTest)
		{
			m_flags.SetFlag(DF_ForceSynchronous);
		}

		m_handHoldDetected.Reset();

#if __BANK
		if(ms_pDebugDrawStore)
		{
			ms_pDebugDrawStore->ClearAll();
		}
#endif // __BANK

		PF_START(ClimbGetMatrix);

		Matrix34 testMat;
		if(overRideMtx)
		{
			testMat = *overRideMtx;
		}
		else
		{
			GetMatrix(testMat);
		}

		PF_STOP(ClimbGetMatrix);

		ProcessAutoVaultStairDelay();

		eSearchResult eResult = SearchForHandHold(testMat, m_handHoldDetected, pPedParams, pPedResults);

		if(eResult == eSR_ResubmitSynchronously)
		{
			m_flags.SetFlag(DF_ForceSynchronous);
			eResult = SearchForHandHold(testMat, m_handHoldDetected, pPedParams, pPedResults);
		}

		switch(eResult)
		{
		case(eSR_Succeeded):
			{
				if(m_uAutoVaultDetectedStartTimeMs == 0)
					m_uAutoVaultDetectedStartTimeMs = fwTimer::GetTimeInMilliseconds();

				ProcessAutoVaultPhysicalDelay(m_handHoldDetected.GetPhysical());

				if(!m_flags.IsFlagSet(DF_AutoVault) || (!IsDoingAutoVaultCooldownDelay() && !IsDoingAutoVaultPhysicalDelay()))
				{
#if __BANK
					TUNE_GROUP_BOOL(VAULTING, bDisregardVaultTests, false)
					if(!bDisregardVaultTests)
#endif
					{
						m_flags.SetFlag(DF_DetectedHandHold);
						m_uLastTimeDetectedHandhold = fwTimer::GetTimeInMilliseconds();
						SetAutoVaultCoolownDelay(0,0);
						SetAutoVaultPhysicalDelay(NULL, 0);
					}
				}
			}
			break;
		case(eSR_Deferred):
			//! Do nothing. Don;t want to reset m_uAutoVaultDetectedStartTimeMs if we haven't got a result yet.
			break;
		case(eSR_Failed):
			{
				ResetPendingResults();
			}
			break;
		case(eSR_ResubmitSynchronously):
			{
				aiAssertf(0, "Can't handle climb result - shouldn't get here!");
			}
			break;
		}

		//! Reset results if no valid handhold was found.
		if(pPedResults && !m_flags.IsFlagSet(DF_DetectedHandHold))
		{
			pPedResults->Reset();
		}

		// Clear the reset flag
		m_Ped.SetPedResetFlag(CPED_RESET_FLAG_SearchingForClimb, false);
	}

	// Force clear the auto vault reset flag
	m_Ped.SetPedResetFlag(CPED_RESET_FLAG_SearchingForAutoVaultClimb, false);
}

////////////////////////////////////////////////////////////////////////////////

void CClimbDetector::ResetAutoVaultDelayTimers() 
{
	SetAutoVaultPhysicalDelay(NULL, 0);
	m_uAutoVaultDetectedStartTimeMs = 0;
}

////////////////////////////////////////////////////////////////////////////////

void CClimbDetector::ResetFlags()
{
	m_flags.ClearAllFlags();
}

void CClimbDetector::ResetPendingResults()
{
	for(u32 i = 0; i < CLIMB_DETECTOR_MAX_HORIZ_TESTS; i++)
	{
		m_HorizontalTestResults[i].Reset();
	}
	m_eHorizontalTestPhase = eATS_Free;

	m_uAutoVaultDetectedStartTimeMs = 0;
	SetAutoVaultPhysicalDelay(NULL, 0);

	ResetFlags();
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::GetDetectedHandHold(CClimbHandHoldDetected& handHold) const
{
	if(m_flags.IsFlagSet(DF_DetectedHandHold))
	{
		handHold = m_handHoldDetected;
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

u32 CClimbDetector::GetLastDetectedHandHoldTime() const
{
	return m_uLastTimeDetectedHandhold;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::IsDetectedHandholdFromAutoVault() const
{
	if(m_flags.IsFlagSet(DF_DetectedHandHold))
	{
		return m_flags.IsFlagSet(DF_AutoVault);
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::UpdateAsyncStatus()
{
	TUNE_GROUP_BOOL(VAULTING, s_bDoHorizontalLineTests, false);
	TUNE_GROUP_BOOL(VAULTING, s_bDoAutoVaultAsyncTests, true);

#if __BANK
	//! Deal with setting this through RAG. Don't need to worry about doing this in FINAL.
	if( (m_bDoHorizontalLineTests != s_bDoHorizontalLineTests) )
	{
		for(u32 i = 0; i < CLIMB_DETECTOR_MAX_HORIZ_TESTS; i++)
		{
			m_HorizontalTestResults[i].Reset();
			m_eHorizontalTestPhase = eATS_Free;
		}
	}
#endif

	if(GetPlayerPed() && m_flags.IsFlagSet(DF_AutoVault))
	{
		//! Note: Lazy initialisation of probe results. Don't want to intialise these for AI that don't use async probes. 
		if(!m_bDoAsyncTests)
		{
			for(u32 i = 0; i < CLIMB_DETECTOR_MAX_HORIZ_TESTS; ++i)
			{
				if(m_HorizontalTestResults[i].pProbeResult == NULL) 
				{
					m_HorizontalTestResults[i].pProbeResult = rage_new WorldProbe::CShapeTestResults(CLIMB_DETECTOR_MAX_INTERSECTIONS);
				}
			}
		}

		m_bDoHorizontalLineTests = s_bDoHorizontalLineTests;
		m_bDoAsyncTests = s_bDoAutoVaultAsyncTests;
	}
	else
	{
		m_bDoHorizontalLineTests = false;
		m_bDoAsyncTests = false;
	}

	return m_bDoAsyncTests;
}

////////////////////////////////////////////////////////////////////////////////

CClimbDetector::eSearchResult CClimbDetector::SearchForHandHold(const Matrix34& testMat, CClimbHandHoldDetected& outHandHold, sPedTestParams *pPedParams, sPedTestResults *pPedResults)
{
	PF_START(ClimbSearch);

	//! Need a capsule for tests.
	if(!m_Ped.GetCapsuleInfo())
	{
		return eSR_Failed;
	}

	// Allow quadrupeds ridden by player.
	if(IsQuadrupedTest())
	{
		if(!GetPlayerPed())
			return eSR_Failed;
	}
	else if(!m_Ped.GetCapsuleInfo()->GetBipedCapsuleInfo())
	{
		return eSR_Failed;
	}

	UpdateAsyncStatus();
	bool bAsync = !m_flags.IsFlagSet(DF_ForceSynchronous) && m_bDoAsyncTests;

	float fMaxClimbHeight = GetMaxClimbHeight(pPedParams);

	static dev_float HEIGHT_TEST_OFFSET = 0.25f;
	Vector3 vP1(testMat.d);
	vP1.z -= m_Ped.GetCapsuleInfo()->GetGroundToRootOffset();
	vP1.z += (fMaxClimbHeight + HEIGHT_TEST_OFFSET);

	// Perform several horizontal tests, up to the determined ceiling height
	// We will look for intersections that resemble vertical edges

	static dev_float HORIZONTAL_PROBE_RADIUS = 0.215f;
	static dev_float START_HEIGHT_MODIFIER = -0.75f;
	static dev_float HORIZONTAL_SEPARATION = HORIZONTAL_PROBE_RADIUS * 2.0f;
	static dev_float HORIZONTAL_INTERSECTION_NORMAL_MAX_Z = 0.9f;

	Vector3 vStart(testMat.d);
	vStart.z += START_HEIGHT_MODIFIER;

	// Round up or down to avoid floating point error
	f32 fNumTests = (vP1.z - vStart.z) / HORIZONTAL_SEPARATION;
	f32 fNumTestsCeil = ceilf(fNumTests);
	f32 fNumTestsFloor = floorf(fNumTests);
	u32 uNumTests = static_cast<u32>((fNumTests - fNumTestsFloor) < (fNumTestsCeil - fNumTests) ? fNumTestsFloor : fNumTestsCeil);
	uNumTests = Min(uNumTests, CLIMB_DETECTOR_MAX_HORIZ_TESTS);

	// Perform the tests
	WorldProbe::CShapeTestHitPoint *intersections[CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS];
	bool bFirstIntersections[CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS] = { 0 };

	//! Hack. In non async mode, we have to allocate intersections on the stack. To keep both code paths viable, we have
	//! to copy into intersection buffer.
	//! In async mode, we don't own the intersection results, so we let the results buffer allocate space for us.
	WorldProbe::CShapeTestHitPoint stackIntersections[CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS];
	if(!bAsync)
	{
		for(int i = 0; i < CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS; ++i)
		{
			intersections[i] = &stackIntersections[i];
		}
	}

	s32 iNumIntersections = 0;

	float minIntesectionDistances[CLIMB_DETECTOR_MAX_HORIZ_TESTS];

	const WorldProbe::CShapeTestHitPoint *pFirstIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS] = { NULL };

	bool bHorizontalTestsReady = true;

	Vector3 vIntersectionTestNormalAVG(Vector3::ZeroType);
	u8 iNumIntersectionTestNormals = 0;
	bool bDoIntersectionNormalTest = true;

	//! In non-climb tests, don't carry on searching. Just take 1st intersection we come across.
	u32 uMaxHorizontalResults = m_flags.IsFlagSet(DF_JumpVaultTestOnly) ? 1 : CLIMB_DETECTOR_MAX_INTERSECTIONS;

	for(u32 i = 0; i < uNumTests && iNumIntersections < CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS; i++)
	{
#if __BANK
		static Color32 debugSearchColours[CLIMB_DETECTOR_MAX_HORIZ_TESTS] =
		{
			Color_PowderBlue,
			Color_DodgerBlue,
			Color_SteelBlue,
			Color_SlateBlue,
			Color_SkyBlue1,
			Color_SkyBlue2,
		};

		Color32 debugHorizontalColor;
		u32 nCurrentTime = fwTimer::GetTimeInMilliseconds();
		bool bInCooldown = nCurrentTime < m_uAutoVaultCooldownEndTimeMs;
		if(bInCooldown)
		{
			if(nCurrentTime < (m_uAutoVaultDetectedStartTimeMs + m_uAutoVaultCooldownDelayMs))
				debugHorizontalColor = Color_red;
			else
				debugHorizontalColor = Color_yellow;
		}
		else
		{
			debugHorizontalColor = debugSearchColours[i];
		}

		SetDebugEnabled(ms_bDebugDrawSearch, debugHorizontalColor);
#endif // __BANK

		float fHorizontalTestDist;
		if(IsQuadrupedTest())
		{
			fHorizontalTestDist = ms_fOnHorseHorizontalTestDistances[i];
		}
		else if(m_flags.IsFlagSet(DF_AutoVault))
		{
			fHorizontalTestDist = ms_fAutoVaultHorizontalTestDistances[i];
		}
		else
		{
			if(pPedParams && pPedParams->fHorizontalTestDistanceOverride > 0.0f)
			{
				fHorizontalTestDist = pPedParams->fHorizontalTestDistanceOverride;
			}
			else
			{
				fHorizontalTestDist = ms_DefaultHorizontalTestDistance;
			}
		}
		
		if(fHorizontalTestDist > 0.0f)
		{
			Vector3 vTestStart(vStart);
			vTestStart.z += i * HORIZONTAL_SEPARATION + HORIZONTAL_PROBE_RADIUS;

			Vector3 vTestEnd(vTestStart);
			vTestEnd += testMat.b * fHorizontalTestDist;

			if(GetPlayerPed() && m_flags.IsFlagSet(DF_AutoVault))
			{
				bool bReady = false;
				if(m_bDoHorizontalLineTests)
				{
					bReady = TestLineAsync(vTestStart, vTestEnd, m_HorizontalTestResults[i], uMaxHorizontalResults, bAsync);
				}
				else
				{
					bReady = TestCapsuleAsync(vTestStart, vTestEnd, HORIZONTAL_PROBE_RADIUS, m_HorizontalTestResults[i], uMaxHorizontalResults, bAsync);
				}

				if(bReady)
				{
					//! Determine min intersection distances
					u32 nNumHits = m_HorizontalTestResults[i].pProbeResult->GetNumHits();
					if(nNumHits > 0)
					{
						bFirstIntersections[iNumIntersections] = true;

						//! Need to copy intersections into buffer.
						int hits = 0;
						for(hits = 0; hits < nNumHits; ++hits)
						{
							intersections[iNumIntersections + hits] = &((*m_HorizontalTestResults[i].pProbeResult)[hits]);

							if(!CanFindFurtherIntersections(*intersections[iNumIntersections + hits]))
							{
								//! If any intersection is non-climbable - ignore further intersections behind it.
								hits++;
								break;
							}
						}

						pFirstIntersections[i] = &((*m_HorizontalTestResults[i].pProbeResult)[0]);
						iNumIntersections += hits;
					}
				}
				else
				{
					bHorizontalTestsReady = false;
				}
			}
			else
			{
				// Capsule test
				s32 iMaxIntersections = Min(CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS - iNumIntersections, CLIMB_DETECTOR_MAX_INTERSECTIONS);
				s32 iTestIntersections = TestCapsule(vTestStart, vTestEnd,  HORIZONTAL_PROBE_RADIUS, intersections[iNumIntersections], iMaxIntersections);

				//! Get minimum hit distances for each test.
				if(iTestIntersections > 0)
				{
					bFirstIntersections[iNumIntersections] = true;

					int hits = 0;
					for(hits = 0; hits < iTestIntersections; ++hits)
					{
						if(!CanFindFurtherIntersections(*intersections[iNumIntersections + hits]))
						{
							//! If any intersection is non-climbable - ignore further intersections behind it.
							hits++;
							break;
						}
					}

					pFirstIntersections[i] = intersections[iNumIntersections];
					iNumIntersections += hits;
				}
			}

			if(pFirstIntersections[i])
			{
				Vector3 vDistance = RCC_VECTOR3(pFirstIntersections[i]->GetPosition()) - vTestStart;
				minIntesectionDistances[i] = vDistance.XYMag();

				if(bDoIntersectionNormalTest)
				{
					Vector3 intersectionNormal = RCC_VECTOR3(pFirstIntersections[i]->GetIntersectedPolyNormal());
					if(intersectionNormal.z < HORIZONTAL_INTERSECTION_NORMAL_MAX_Z)
					{
						vIntersectionTestNormalAVG += intersectionNormal;
						++iNumIntersectionTestNormals;
					}
				}
			}
			else
			{
				minIntesectionDistances[i] = fHorizontalTestDist;
				bDoIntersectionNormalTest = false;
			}
		}
	}
	
	if(bAsync && (m_eHorizontalTestPhase == eATS_Free) )
	{
		m_eHorizontalTestPhase = eATS_InProgress;
	}

	PF_STOP(ClimbSearch);

	if(bHorizontalTestsReady)
	{
		m_eHorizontalTestPhase = eATS_Free;
	}
	else
	{
		//! If all horizontal tests haven't finished, bail.
		return eSR_Deferred;
	}

	if(iNumIntersectionTestNormals > 0)
	{
		vIntersectionTestNormalAVG /= iNumIntersectionTestNormals;
		vIntersectionTestNormalAVG.Normalize();
	}

	//! If we are trying to do an async test on a vehicle, and we get a hit, then resubmit test synchronously to verify.
	if(m_flags.IsFlagSet(DF_AutoVault) && bAsync && iNumIntersections > 0 && m_Ped.GetGroundPhysical())
	{
		return eSR_ResubmitSynchronously;
	}

	DoBlockJumpTest(testMat, pFirstIntersections);

	PF_START(ClimbEdgeDetection);

	atFixedArray<sEdge, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS> edges;
	for(s32 i = 0; i < iNumIntersections; i++)
	{
		if(!intersections[i])
		{
			continue;
		}

#if __BANK
		SetDebugEnabled(ms_bDebugDrawIntersections);
		if(GetDebugEnabled())
		{
			if(intersections[i]->IsAHit())
			{
				char buff[8];
				buff[0] = '\0';
				safecatf(buff, 8, "%d", i);
				GetDebugDrawStore().AddText(intersections[i]->GetPosition(), 0, 0, buff, Color_white, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);
			}
		}
#endif
		
		//! Do not do any further tests if hit an intersection that disallows it.
		if(DoesIntersectionInvalidateClimb(*intersections[i]))
		{
			CLIMB_DEBUGF("ClimbDetector: Intersection invalidates climb = %d", i);
			return eSR_Failed;
		}

		if(intersections[i]->GetHitPolyNormal().z > HORIZONTAL_INTERSECTION_NORMAL_MAX_Z)
		{
			CLIMB_DEBUGF("ClimbDetector: Don't count large horizontal hits = %d", i);
			continue;
		}

		if(!GetIsValidIntersection(*intersections[i]))
		{
			CLIMB_DEBUGF("ClimbDetector: GetIsValidIntersection failed = %d", i);
			continue;
		}

		if(IsIntersectionOnSlope(testMat, *intersections[i], vIntersectionTestNormalAVG))
		{
			CLIMB_DEBUGF("ClimbDetector: IsIntersectionOnSlope failed = %d", i);
			continue;
		}

		// Calculate a score based on distance
		static dev_float DIST_SCORE_MIN = 0.0f;
		static dev_float DIST_SCORE_MAX = 1.0f;
		static dev_float DIST_RANGE_MIN = square(0.0f);
		static dev_float DIST_RANGE_MAX = square(5.0f);

		// Intersection pos
		Vector3 vPos(RCC_VECTOR3(intersections[i]->GetPosition()));

		// Determine the distance to the intersection - this will allow us to group intersections together
		// as well as providing us with an sEdge weight
		Vector3 vDist(vPos);
		vDist -= testMat.d;

		f32 fEdgeHeight = vPos.z - GetPedGroundPosition(testMat).z;

		// Get the squared distance
		f32 fXYDistSq = vDist.XYMag2();
		f32 fDistScore = Clamp(fXYDistSq, DIST_RANGE_MIN, DIST_RANGE_MAX);
		fDistScore = (fDistScore - DIST_RANGE_MIN) / (DIST_RANGE_MAX - DIST_RANGE_MIN);
		fDistScore = fDistScore * (DIST_SCORE_MAX - DIST_SCORE_MIN) + DIST_SCORE_MIN;

		// Calculate a score based on heading
		static dev_float HEADING_SCORE_MIN = 0.0f;
		static dev_float HEADING_SCORE_MAX = 1.0f;

		// Get the intersection heading (reversed, so we can compare with the matrix and do a dot)
		Vector3 vNormal(-RCC_VECTOR3(intersections[i]->GetIntersectedPolyNormal()));
		f32 fHeading = fwAngle::LimitRadianAngle(Atan2f(vNormal.x, -vNormal.y));
		vNormal.z = 0.0f;
		vNormal.NormalizeFast();
		f32 fHeadingScore = 0.0;
		if(vNormal.Mag2() > 0.0f)
		{
			fHeadingScore = testMat.b.Dot(vNormal);
			fHeadingScore = (fHeadingScore - 1.f) * -0.5f;
			fHeadingScore = fHeadingScore * (HEADING_SCORE_MAX - HEADING_SCORE_MIN) + HEADING_SCORE_MIN;
		}

		static dev_float DIST_FACTOR = 5.0f;
		static dev_float HEADING_FACTOR = 1.0f;
		static dev_float HEIGHT_FACTOR = 0.0f;
		static dev_float SECONDARY_HIT_FACTOR = 1.25f;

		static dev_float HEIGHT_SCORE_RANGE_MIN = 0.0f;
		static dev_float HEIGHT_SCORE_RANGE_MAX = 2.8f;
		f32 fHeightScore = Clamp(fEdgeHeight, HEIGHT_SCORE_RANGE_MIN, HEIGHT_SCORE_RANGE_MAX);
		fHeightScore = 1.0f - (fEdgeHeight - HEIGHT_SCORE_RANGE_MIN) / (HEIGHT_SCORE_RANGE_MAX - HEIGHT_SCORE_RANGE_MIN);

		f32 fScore = (fDistScore * DIST_FACTOR) + (fHeadingScore * HEADING_FACTOR) + (fHeightScore*HEIGHT_FACTOR);

		if(!bFirstIntersections[i])
		{
			fScore *= SECONDARY_HIT_FACTOR;
		}

		// Not added yet
		bool bAdded = false;

		// Go through the current edges to determine if this intersection belongs in an edge group
		for(s32 j = 0; j < edges.GetCount(); j++)
		{
			f32 fXYDistSqDiff = (vPos - edges[j].vMidPoint).XYMag2();
			static dev_float DISTANCE_TOLERANCE = 0.09f;
			if(Abs(fXYDistSqDiff) < square(DISTANCE_TOLERANCE))
			{
				f32 fHeadingDiff = fwAngle::LimitRadianAngle(fHeading - edges[j].fHeading);
				static dev_float HEADING_TOLERANCE = DtoR * 45.f;
				if(Abs(fHeadingDiff) < HEADING_TOLERANCE)
				{
					static dev_float HEIGHT_TOLERANCE = HORIZONTAL_SEPARATION * 2.0f;
					if((vPos.z >= intersections[edges[j].iStart]->GetPosition().GetZf()-HEIGHT_TOLERANCE) && (vPos.z <= intersections[edges[j].iEnd]->GetPosition().GetZf()+HEIGHT_TOLERANCE))
					{
						if(intersections[edges[j].iStart]->GetPosition().GetZf() > intersections[i]->GetPosition().GetZf())
						{
							edges[j].iStart = i;
						}

						if(intersections[edges[j].iEnd]->GetPosition().GetZf() < intersections[i]->GetPosition().GetZf())
						{
							edges[j].iEnd = i;
						}

						// The midpoint becomes the average of the old/new distance squared
						edges[j].vMidPoint += vPos;
						edges[j].vMidPoint *= 0.5f;

						// The heading becomes the average of the old/new heading
						edges[j].fHeading += fHeading;
						edges[j].fHeading *= 0.5f;
						edges[j].fHeading  = fwAngle::LimitRadianAngle(edges[j].fHeading);

						edges[j].vNormals += intersections[i]->GetHitPolyNormal();
						edges[j].nNumNormals++;

						//! Penalise normals that are quite severe.
						/*if(intersections[i]->GetHitPolyNormal().z <= -0.9f)
						{
							fScore += 0.25f;
						}*/

						edges[j].fScore = Min(edges[j].fScore, fScore);

						// Set the score to the best score
						edges[j].fScore = Min(edges[j].fScore, fScore);
						bAdded = true;
						break;
					}
				}
			}
		}

		// If not added, add a new entry
		if(!bAdded)
		{
			edges.Push(sEdge(i, i, vPos, fHeading, fScore, intersections[i]->GetHitPolyNormal()));
		}
	}

	PF_STOP(ClimbEdgeDetection);

	if(edges.GetCount() > 0)
	{
		PF_START(ClimbEdgeSort);

		// Sort the edges
		qsort(&edges[0], edges.GetCount(), sizeof(sEdge), (s32 (/*__cdecl*/ *)(const void*, const void*))sEdge::PairSort);

		PF_START(ClimbEdgeSort);

#if __BANK
		// Render the edges
		SetDebugEnabled(ms_bDebugDrawSearch);
		if(GetDebugEnabled())
		{
			for(s32 i = 0; i < edges.GetCount(); i++)
			{
				Vec3V vN = intersections[edges[i].iEnd]->GetIntersectedPolyNormal();
				Vec3V v1 = intersections[edges[i].iStart]->GetPosition() + vN * ScalarV(0.05f);
				Vec3V v2 = intersections[edges[i].iEnd]->GetPosition()   + vN * ScalarV(0.05f);

				if(edges[i].iStart != edges[i].iEnd)
				{
					GetDebugDrawStore().AddLine(v1, v2, Color_pink, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);
				}

				char buff[8];
				buff[0] = '\0';
				safecatf(buff, 8, "%d", i);
				Vec3V vMidPoint = (v1 + v2) * ScalarV(V_HALF);
				GetDebugDrawStore().AddText(vMidPoint, 0, 0, buff, Color_white, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);
			}
		}
		SetDebugEnabled(false);
#endif // __BANK

#if __BANK
		TUNE_GROUP_INT(VAULTING, nDebugEdge, -1, -1, 100, 1);
#endif

		// Go through the edges and try to detect valid hand holds
		for(s32 i = 0; i < edges.GetCount(); i++)
		{
#if __BANK
			if(nDebugEdge >= 0 && (i != nDebugEdge))
			{
				continue;
			}
#endif

			if(!GetIsEdgeValid(edges[i], intersections[edges[i].iStart], intersections[edges[i].iEnd]))
			{
				CLIMB_DEBUGF("ClimbDetector: Failed valid edge check! edge = %d", i);
				continue;
			}

			// Use average edge normal for a better, more accurate result.
			taskAssert(edges[i].nNumNormals > 0);
			Vector3 vAvgEdgeNormal = edges[i].vNormals / (float)edges[i].nNumNormals;
			vAvgEdgeNormal.NormalizeFast();

			// Reset before doing any tests.
			if(pPedResults)
				pPedResults->Reset();

			// Attempt to detect a valid ledge point
			f32 fStartDist = HORIZONTAL_SEPARATION + HORIZONTAL_PROBE_RADIUS;
			f32 fEndDist = HORIZONTAL_SEPARATION * 2.0f;

			WorldProbe::CShapeTestHitPoint handHoldIntersection[eHT_NumTests];
			if(!ComputeHandHoldPoint(testMat, *intersections[edges[i].iEnd], fStartDist, fEndDist, vAvgEdgeNormal, pPedParams, handHoldIntersection))
			{
				CLIMB_DEBUGF("ClimbDetector: Failed to compute handhold! edge = %d", i);
				continue;
			}

			// Construct the hand hold out of the result
			Vector3 vPoint(RCC_VECTOR3(handHoldIntersection[eHT_Main].GetPosition()));

			Vector3 vFlatSurfaceNormal = vAvgEdgeNormal;
			vFlatSurfaceNormal.z = 0.0f;
			if(vFlatSurfaceNormal.Mag2() > 0.0f)
				vFlatSurfaceNormal.NormalizeFast();

			outHandHold.SetHandHold(CClimbHandHold(vPoint, vFlatSurfaceNormal));

			//! Find highest/lowest handhold point.
			float fHighestZ, fLowestZ;
			fHighestZ = fLowestZ = handHoldIntersection[0].GetPosition().GetZf();
			for(int nHandholds = 1; nHandholds < eHT_NumTests; ++nHandholds)
			{
				if(handHoldIntersection[nHandholds].GetPosition().GetZf() > fHighestZ)
					fHighestZ = handHoldIntersection[nHandholds].GetPosition().GetZf();

				if(handHoldIntersection[nHandholds].GetPosition().GetZf() < fLowestZ)
					fLowestZ = handHoldIntersection[nHandholds].GetPosition().GetZf();
			}

			outHandHold.SetHighestZ(fHighestZ);
			outHandHold.SetLowestZ(fLowestZ);

			if(!SetClimbPhysical(outHandHold, handHoldIntersection[eHT_Main]))
			{
				CLIMB_DEBUGF("ClimbDetector: Failed to set climb physical! edge = %d", i);
				continue;
			}

			if(!SetClimbHeight(testMat, outHandHold, pPedParams))
			{
				CLIMB_DEBUGF("ClimbDetector: Failed SetClimbHeight! edge = %d", i);
				continue;
			}

			//! Determine if ped can use this handhold before going any further.
			if(pPedParams && pPedResults)
			{
				if(!DoPedHandholdTest(testMat, outHandHold, pPedParams, pPedResults))
				{
					CLIMB_DEBUGF("ClimbDetector: Failed Ped Handhold Check! edge = %d", i);
					continue;
				}
			}

			if(!bFirstIntersections[edges[i].iStart])
			{
				outHandHold.SetFrontSurfaceMaterial(intersections[edges[i].iStart]->GetMaterialId(),g_CollisionAudioEntity.GetMaterialFromShapeTestSafe(intersections[edges[i].iStart]));
			}
			else
			{
				SelectBestFrontSurfaceMaterial(outHandHold, pFirstIntersections, intersections[edges[i].iStart]);
			}

			// Set any climbing materials
			outHandHold.SetTopSurfaceMaterial(g_CollisionAudioEntity.GetMaterialFromShapeTestSafe(&handHoldIntersection[eHT_Main]));

			//! DMKH. Hack! Use Intersection tests (to save carrying out another test) to determine if we are going under an arch. 
			//! We want to disregard this climb if so.
			if(IsGoingUnderArch(testMat, outHandHold, minIntesectionDistances, pFirstIntersections))
			{
				CLIMB_DEBUGF("ClimbDetector: Failed IsGoingUnderArch() check! edge = %d", i);
				continue;
			}

			if(!CalculateClimbAngle(testMat, outHandHold, minIntesectionDistances, pFirstIntersections))
			{
				CLIMB_DEBUGF("ClimbDetector: Failed CalculateClimbAngle() - angle not valid! edge = %d", i);
				continue;
			}

			// Construct test direction (the normal which will influence the direction we will do our validation checks).
			bool bUseTestOrientation = pPedParams ? outHandHold.GetHeight() < pPedParams->fUseTestOrientationHeight : false;

			bool bClimbingOnVehicle = outHandHold.GetPhysical() && outHandHold.GetPhysical()->GetIsTypeVehicle();
			
			//! Try and keep test alignment for vehicles (as surfaces are quite angular). This ensures bonnet slides 
			//! maintain ped heading better. 
			if(!bUseTestOrientation && bClimbingOnVehicle)
			{
				static dev_float s_CloseNormal = 0.95f;
				float fDot = DotProduct(vFlatSurfaceNormal, -testMat.b);
				if(fDot > s_CloseNormal)
				{
					bUseTestOrientation = true;
				}
			}

			Vector3 vTestDirection = vFlatSurfaceNormal;
			Vector3 vTestDirectionFromPedHeading = vTestDirection;
			bool bRunningJump = pPedResults && pPedResults->GetCanAutoJumpVault();
			bool bUsePedOrientation = false;

			//! Need to create a copy of handhold as we may need to test validation again.
			CClimbHandHoldDetected tempHandhold = outHandHold;
			if(m_flags.IsFlagSet(DF_JumpVaultTestOnly) || bRunningJump || bUseTestOrientation )
			{
				AdjustHandholdPoint(testMat, tempHandhold, handHoldIntersection[eHT_Adjacent1], handHoldIntersection[eHT_Adjacent2]);

				vTestDirectionFromPedHeading = -testMat.b;
				vTestDirectionFromPedHeading.z = 0.0f;
				if(vTestDirectionFromPedHeading.Mag2() > 0.0f)
				{
					vTestDirectionFromPedHeading.NormalizeFast();
				}
				bUsePedOrientation = true;
			}

			bool bDirectionValid = DoValidationTestInDirection(testMat, i,
				bUsePedOrientation ? vTestDirectionFromPedHeading : vTestDirection,
				bUsePedOrientation ? tempHandhold : outHandHold, 
				pPedParams, pPedResults, minIntesectionDistances, pFirstIntersections);
			
			if(bUsePedOrientation)
			{
				if(bClimbingOnVehicle)
				{
					//! Redo test if we didn't manage to validate.
					if(!bDirectionValid)
					{
						bUsePedOrientation = false;
						bDirectionValid = DoValidationTestInDirection(testMat, i, vTestDirection, outHandHold, pPedParams, pPedResults, minIntesectionDistances, pFirstIntersections);
					}
					else
					{
						//! Redo test if we were successful, but didn't find a slide. We want to ensure we slide where possible.
						bool bCanSlideUsingPedOrientation = CTaskVault::IsSlideSupported(tempHandhold, GetPlayerPed(), bRunningJump, m_flags.IsFlagSet(DF_AutoVault));
						if(!bCanSlideUsingPedOrientation)
						{
							bool bValidationTestForSlide = DoValidationTestInDirection(testMat, i, vTestDirection, outHandHold, pPedParams, pPedResults, minIntesectionDistances, pFirstIntersections);
							if(bValidationTestForSlide)
							{
								bool bCanSlideUsingHitNormal = CTaskVault::IsSlideSupported(outHandHold, GetPlayerPed(), bRunningJump, m_flags.IsFlagSet(DF_AutoVault));
								if(bCanSlideUsingHitNormal)
								{
									bUsePedOrientation = false;
								}
							}
						}
					}
				}

				if(bUsePedOrientation)
				{
					outHandHold = tempHandhold;
				}
			}

			if(bDirectionValid)
			{
				if(bUsePedOrientation)
				{
					outHandHold.SetAlignDirection(vTestDirectionFromPedHeading);
				}
				else
				{
					m_flags.SetFlag(VF_OrientToHandhold);
				}

				//! Ensure all offsets are relative to physical.
				outHandHold.CalcPhysicalHandHoldOffset();

				if(NetworkInterface::IsGameInProgress() && !m_Ped.IsNetworkClone())
				{	//Succeeded so save the test direction and bUseTestOrientation for sending to remote 			
					Vector3 vCloneTestheading = testMat.b;
					vCloneTestheading.z = 0.0f;
					if(vCloneTestheading.Mag2() > 0.0f)
						vCloneTestheading.NormalizeFast( );

					float fTestHeading = rage::Atan2f(-vCloneTestheading.x, vCloneTestheading.y);
					outHandHold.SetCloneTestHeading(fTestHeading);
					outHandHold.SetCloneUseTestheading(bUseTestOrientation);
					outHandHold.SetCloneIntersection(handHoldIntersection[eHT_Main].GetHitPosition());
				}

				return eSR_Succeeded;
			}
			else
			{
				CLIMB_DEBUGF("ClimbDetector: Failed Validation Check! edge = %d", i);
			}
		}
	}

	return eSR_Failed;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::DoValidationTestInDirection(const Matrix34& testMat, 
												 int ASSERT_ONLY(iEdge), 
												 const Vector3 &vTestDirection,
												 CClimbHandHoldDetected& outHandHold, 
												 sPedTestParams *pPedParams, 
												 sPedTestResults *pPedResults, 
												 const float (&minIntesectionDistances)[CLIMB_DETECTOR_MAX_HORIZ_TESTS],
												 const WorldProbe::CShapeTestHitPoint *pIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS])
{
	BANK_ONLY(SetDebugEnabled(ms_bDebugDrawCompute, Color_yellow));
	
	// Do slope test on handheld. If we can walk to it, we shouldn't vault it.
	if(IsHandholdOnSlope(testMat, -vTestDirection, outHandHold))
	{
		CLIMB_DEBUGF("ClimbDetector: Failed IsHandholdOnSlope() check! edge = %d", iEdge);
		return false;
	}

	BANK_ONLY(SetDebugEnabled(false));

	// Validate it - perform volume queries
	if(ValidateHandHoldAndDetermineVolume(testMat, vTestDirection, outHandHold, pPedParams, pPedResults, minIntesectionDistances, pIntersections))
	{
		//! Identity if this is a step up. 
		//! If so, prevent if we have been told to stop this.
		if(pPedParams && pPedResults && m_flags.IsFlagSet(DF_AutoVault) && !pPedParams->bCanDoStandingAutoStepUp)
		{
			//! Note: This value is used in the MoVE network. Ideally we'd pass it though, but don't want to modify/patch MoVE networks atm
			static dev_float s_fDistanceToHandholdForStandStepUp = 0.85f;
			Vector3 vDiff = outHandHold.GetHandholdPosition() - VEC3V_TO_VECTOR3(GetPlayerPed()->GetTransform().GetPosition());
			bool bStepUpDistance = vDiff.XYMag() <= s_fDistanceToHandholdForStandStepUp;
			if((outHandHold.GetHeight() <= CTaskVault::ms_fStepUpHeight) && 
				((!pPedResults->GetCanJumpVault() && !pPedResults->GetCanAutoJumpVault()) || bStepUpDistance))
			{
				return false;
			}
		}
		
		return true;
	}

	return false;
}

CClimbDetector::eSearchResult CClimbDetector::GetCloneVaultHandHold(const Matrix34& testMat, sPedTestParams *pPedParams)
{
	WorldProbe::CShapeTestHitPoint handHoldIntersection[eHT_NumTests];

	Assertf(m_Ped.IsNetworkClone(),"Only expect clones to use this");

	if(!pPedParams)
	{
		return eSR_Failed;
	}

	Assertf(m_Ped.IsNetworkClone(),"Only expect clones to use this");

	if(!GetCloneVaultHandHoldPoint(pPedParams,handHoldIntersection))
	{
		CLIMB_DEBUGF("%s ClimbDetector: GetCloneVaultHandHoldPoint failed \n",m_Ped.GetDebugName());
		return eSR_Failed;
	}

	m_handHoldDetected.Reset();

	// Construct the hand hold out of the result
	Vector3 vPoint(RCC_VECTOR3(handHoldIntersection[eHT_Main].GetPosition()));

	Vector3 vFlatSurfaceNormal = pPedParams->vCloneHandHoldNormal;
	vFlatSurfaceNormal.z = 0.0f;
	if(vFlatSurfaceNormal.Mag2() > 0.0f)
		vFlatSurfaceNormal.NormalizeFast();

	m_handHoldDetected.SetHandHold(CClimbHandHold(vPoint, vFlatSurfaceNormal));

	//! Find highest/lowest handhold point.
	float fHighestZ, fLowestZ;
	fHighestZ = fLowestZ = handHoldIntersection[0].GetPosition().GetZf();
	for(int nHandholds = 1; nHandholds < eHT_NumTests; ++nHandholds)
	{
		if(handHoldIntersection[nHandholds].GetPosition().GetZf() > fHighestZ)
			fHighestZ = handHoldIntersection[nHandholds].GetPosition().GetZf();

		if(handHoldIntersection[nHandholds].GetPosition().GetZf() < fLowestZ)
			fLowestZ = handHoldIntersection[nHandholds].GetPosition().GetZf();
	}

	m_handHoldDetected.SetHighestZ(fHighestZ);
	m_handHoldDetected.SetLowestZ(fLowestZ);

	if(!SetClimbPhysical(m_handHoldDetected, handHoldIntersection[eHT_Main]))
	{
		CLIMB_DEBUGF("ClimbDetector: Failed to set climb physical! edge \n");
		return eSR_Failed;
	}

	if(!SetClimbHeight(testMat, m_handHoldDetected, pPedParams))
	{
		CLIMB_DEBUGF("ClimbDetector: Failed SetClimbHeight! edge \n");
		return eSR_Failed;
	}

	// Construct test direction (the normal which will influence the direction we will do our validation checks).
	bool bUseTestOrientation = pPedParams ? m_handHoldDetected.GetHeight() < pPedParams->fUseTestOrientationHeight : false;

	bUseTestOrientation =  pPedParams->bCloneUseTestDirection;

	Vector3 vTestDirection = vFlatSurfaceNormal;
	if( bUseTestOrientation ) 
	{
		vTestDirection = -testMat.b;
		vTestDirection.z = 0.0f;
		if(vTestDirection.Mag2() > 0.0f)
			vTestDirection.NormalizeFast();
		m_handHoldDetected.SetAlignDirection(vTestDirection);

		//After calling CClimbHandHoldDetected::SetAlignDirection ensure all offsets are relative to physical.
		m_handHoldDetected.CalcPhysicalHandHoldOffset();

		m_flags.ClearFlag(VF_OrientToHandhold);
	}

	// Set any climbing materials
	m_handHoldDetected.SetTopSurfaceMaterial(g_CollisionAudioEntity.GetMaterialFromShapeTestSafe(&handHoldIntersection[eHT_Main]));
	m_handHoldDetected.SetFrontSurfaceMaterial(handHoldIntersection[eHT_Main].GetMaterialId(),g_CollisionAudioEntity.GetMaterialFromShapeTestSafe(&handHoldIntersection[eHT_Main]));

	float dummyMinIntesectionDistances[CLIMB_DETECTOR_MAX_HORIZ_TESTS]; //Not used just need to pass to ValidateHandHoldAndDetermineVolume for clones
	const WorldProbe::CShapeTestHitPoint *pDummyFirstIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS] = { NULL }; //Not used just need to pass ValidateHandHoldAndDetermineVolume for clones

	// Validate it - perform volume queries
	if(ValidateHandHoldAndDetermineVolume(testMat, vTestDirection, m_handHoldDetected, pPedParams, NULL, dummyMinIntesectionDistances, pDummyFirstIntersections ))
	{
		m_flags.SetFlag(DF_DetectedHandHold);
		return eSR_Succeeded;
	}

	CLIMB_DEBUGF("%s ClimbDetector: GetCloneVaultHandHold failed \n",m_Ped.GetDebugName());

	return eSR_Failed;
}


bool CClimbDetector::DoPedHandholdTest(const Matrix34& testMat, CClimbHandHoldDetected& handHold, sPedTestParams *pPedParams, sPedTestResults *pPedResults)
{	
	PF_FUNC(ClimbPedTest);

	aiAssert(pPedParams);
	aiAssert(pPedResults);

	bool bIsVehicle = handHold.GetPhysical() && handHold.GetPhysical()->GetIsTypeVehicle();

	Vector3 vecPlayerForward(testMat.b);
	float fDot = DotProduct(-vecPlayerForward, handHold.GetHandHold().GetNormal());
	if(fDot >= pPedParams->fParallelDot)
	{
		Vector3 vToHandHold(handHold.GetHandHold().GetPoint());
		vToHandHold -= testMat.d;
		vToHandHold.z = 0.0f;

		if(vToHandHold.Mag2() > 0.0f)
		{
			// Get the distance to the hand hold
			float fDistXY = vToHandHold.XYMag();

			bool bDistanceOk = true;
			
			//! Don't to step up if we are closer than cutoff distance.
			if(m_flags.IsFlagSet(DF_AutoVault) && (handHold.GetHeight() <= CTaskVault::ms_fStepUpHeight) && (fDistXY < pPedParams->fStepUpDistanceCutoff) )
			{
				bDistanceOk = false;
			}

			if(bDistanceOk)
			{
				// Normalise it for the dot product
				vToHandHold.NormalizeFast();

				CControl* pControl = GetPlayerPed() ? GetPlayerPed()->GetControlFromPlayer() : NULL;

				fDot = vToHandHold.Dot(vecPlayerForward);
				if(fDot >= pPedParams->fForwardDot)
				{
					float fStandingDist;
					float fStickVaultDist;
					if(handHold.GetHeight() <= CTaskVault::ms_fStepUpHeight)
					{
						fStandingDist = pPedParams->fVaultDistStepUp;
						fStickVaultDist = pPedParams->fVaultDistStepUp;
					}
					else
					{
						fStandingDist = bIsVehicle ? pPedParams->fVaultDistStandingVehicle : pPedParams->fVaultDistStanding;
						fStickVaultDist = pPedParams->fMaxVaultDistance;
					}

					//! If we are within stand vault distance, go for it.
					if( (fDistXY < fStandingDist) && pPedParams->bDoStandingVault)
					{
						pPedResults->SetCanVault(true);
					}

					//! Do stick intent checks.
					if(pControl)
					{
						Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);
						if( (vStickInput.Mag2() > 0.0f) || !pPedParams->bShowStickIntent)
						{
							float fCamOrient = camInterface::GetHeading();
							vStickInput.RotateZ(fCamOrient);
							vStickInput.NormalizeFast();

							fDot = vToHandHold.Dot(vStickInput);
							if(fDot >= pPedParams->fStickDot)
							{
								if(!m_flags.IsFlagSet(DF_AutoVault))
								{
									pPedResults->SetGoingToVaultSoon(true);
								}

								if (fDistXY < fStickVaultDist)
								{
									pPedResults->SetCanVault(true);
								}

								if(pPedParams->bDoJumpVault &&
									(pPedParams->fCurrentMBR >= pPedParams->fJumpVaultMBRThreshold) && 
									(!bIsVehicle || IsQuadrupedTest()))
								{
									static const dev_float s_LineLength = 20.0f;

									//! Test highest point on handhold is not too high.
									Vector3	vPedRoot = testMat.d; 
									vPedRoot.z = GetPedGroundPosition(testMat).z;
									Vector3 vHighestHandHoldPoint(handHold.GetHandHold().GetPoint());
									Vector3 vToHandHoldFromRoot = vHighestHandHoldPoint - vPedRoot;

									bool bTestHeightFromClimbBaseSuccessful = handHold.GetHeight() <= pPedParams->fJumpVaultHeightThresholdMax;  
									if(!IsQuadrupedTest() && m_flags.IsFlagSet(DF_AutoVault) && !pPedResults->GetCanVault())
									{
										Vector3 vSlopeDirection = m_Ped.GetGroundNormal();
										vSlopeDirection.Cross(testMat.a);

#if __BANK
										if(GetDebugEnabled())
										{
											Vector3 vEnd = vPedRoot + (vSlopeDirection * s_LineLength);
											GetDebugDrawStore().AddLine(RCC_VEC3V(vPedRoot), RCC_VEC3V(vEnd), ms_debugDrawColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);
										}
	#endif
										Vector3 vClosest;
										fwGeom::fwLine line(vPedRoot, vPedRoot + (vSlopeDirection * s_LineLength) );
										line.FindClosestPointOnLine(vHighestHandHoldPoint, vClosest);
										Vector3 vDistanceToLine = vHighestHandHoldPoint - vClosest;
										float fHighestHeight = vDistanceToLine.z;
								
										bTestHeightFromClimbBaseSuccessful = fHighestHeight <= pPedParams->fJumpVaultHeightAllowanceOnSlope;
									}

									if(bTestHeightFromClimbBaseSuccessful && vToHandHoldFromRoot.z <= pPedParams->fJumpVaultHeightThresholdMax)
									{
										//! Need to test height of point relative to ground, as opposed to ped. The line intersection will
										//! give us the height of the handhold.
										
										Vector3 vClosest;
										Vector3 vLowestHandHoldPoint = handHold.GetHandHold().GetPoint();
										vLowestHandHoldPoint.z = handHold.GetLowestZ();

										fwGeom::fwLine line(vPedRoot, vPedRoot + (testMat.b * s_LineLength) );
										line.FindClosestPointOnLine(vLowestHandHoldPoint, vClosest);
										Vector3 vDistanceToLine = vLowestHandHoldPoint - vClosest;
										float fLowestHeight = vDistanceToLine.z;

										if(fLowestHeight >= pPedParams->fJumpVaultHeightThresholdMin)
										{
											pPedResults->SetCanJumpVault(true);

											float fSpeed = Max(pPedParams->fJumpVaultMBRThreshold, pPedParams->fCurrentMBR);
											fSpeed = (pPedParams->fCurrentMBR - pPedParams->fJumpVaultMBRThreshold) / (MOVEBLENDRATIO_RUN - pPedParams->fJumpVaultMBRThreshold);
											float fAutoVaultDistance = pPedParams->fAutoJumpVaultDistMin + ((pPedParams->fAutoJumpVaultDistMax-pPedParams->fAutoJumpVaultDistMin)*fSpeed);

											if((fDistXY < fAutoVaultDistance) && (fDistXY > pPedParams->fAutoJumpVaultDistCutoff) )
											{
												pPedResults->SetCanAutoJumpVault(true);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	//! Ensure we never set vault flags if all we are interested in is jump vault.
	if(m_flags.IsFlagSet(DF_JumpVaultTestOnly))
	{
		pPedResults->SetCanVault(false);
		pPedResults->SetGoingToVaultSoon(false);
	}

	return pPedResults->IsValid();
}

////////////////////////////////////////////////////////////////////////////////

float CClimbDetector::GetMaxClimbHeight(sPedTestParams *pPedParams) const
{
	//! Use override climb height if set.
	TUNE_GROUP_FLOAT(VAULTING, fVaultMaxHeight, 3.005f, 0.0f, 5.0f, 0.001f); 
	float fMaxHeight = (pPedParams && pPedParams->fOverrideMaxClimbHeight > 0.0f) ? pPedParams->fOverrideMaxClimbHeight : fVaultMaxHeight;
	return fMaxHeight;
}

////////////////////////////////////////////////////////////////////////////////

float CClimbDetector::GetMinClimbHeight(sPedTestParams *pPedParams, bool bClimbingPed) const
{
	//! Use override climb height if set.
	TUNE_GROUP_FLOAT(VAULTING, fVaultMinHeight, 0.325f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultMinHeightLowerBound, 0.2f, 0.0f, 1.0f, 0.01f);

	float fMinHeight = fVaultMinHeight;
	if (pPedParams != NULL)
	{
		if (bClimbingPed)
		{
			if (pPedParams->fOverrideMinClimbPedHeight > 0.0f)
			{
				fMinHeight = pPedParams->fOverrideMinClimbPedHeight;
			}
		}
		else
		{
			if (pPedParams->fOverrideMinClimbHeight > 0.0f)
			{
				fMinHeight = pPedParams->fOverrideMinClimbHeight;
			}
		}
	}

	//! Adjust based on ground offset. E.g. if capsule is compressed, lower threshold by compression height.
	const CBaseCapsuleInfo* pCapsuleInfo = m_Ped.GetCapsuleInfo();
	if(m_Ped.GetGroundOffsetForPhysics() > -pCapsuleInfo->GetGroundToRootOffset())
	{
		float fDiff = Abs(-pCapsuleInfo->GetGroundToRootOffset() - m_Ped.GetGroundOffsetForPhysics());
		fMinHeight -= fDiff; 
	}

	//! Cap adjustment at a sensible lower bound.
	return Max(fMinHeight, fVaultMinHeightLowerBound);
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::SetClimbHeight(const Matrix34& testMat, CClimbHandHoldDetected& inOutHandHold, sPedTestParams *pPedParams)
{
	//
	// Determine the height
	//

	const Vector3& vHandHoldPoint(inOutHandHold.GetHandHold().GetPoint());

	//! Use override climb height if set.
	f32 fHeight = vHandHoldPoint.z - GetPedGroundPosition(testMat).z;
	float fMaxHeight = GetMaxClimbHeight(pPedParams);
	if( fHeight > fMaxHeight )
	{
		// Too high
		CLIMB_DEBUGF("CClimbDetector::SetClimbHeight: Failed fHeight > fMaxHeight");
		return false;
	}

	float fMinHeight = GetMinClimbHeight(pPedParams, inOutHandHold.GetPhysical() != NULL && inOutHandHold.GetPhysical()->GetIsTypePed());
	if( fHeight < fMinHeight)
	{
		// Too low
		CLIMB_DEBUGF("CClimbDetector::SetClimbHeight: Failed fHeight < fMinHeight");
		return false;
	}

	//! This is a small allowance for climbing things in 2 directions. E.g. If a vaultable fence is just cimbable on one side, but not another, we
	//! allow a small additional threshold to climb this. This can hopefully prevent this inconsistency (and potentially fix cases where we can
	//! climb into an area, but can't climb back out).
	if( fHeight > (fMaxHeight-GetMaxHeightDropAllowance()) && pPedParams && pPedParams->bDo2SidedHeightTest)
	{
		m_flags.SetFlag(VF_DoMaxHeightDropCheck);
	}
	else
	{
		m_flags.ClearFlag(VF_DoMaxHeightDropCheck);
	}

	// Store the height
	inOutHandHold.SetHeight(fHeight);

	return true;
}

////////////////////////////////////////////////////////////////////////////////

float CClimbDetector::GetMaxHeightDropAllowance() const
{
	TUNE_GROUP_FLOAT(VAULTING, fMaxHeightDropAllowance, 0.05f, 0.0f, 5.0f, 0.01f); 
	return fMaxHeightDropAllowance;
}

////////////////////////////////////////////////////////////////////////////////

bool LineIntersection2D(const Vector3 &line1_p1, const Vector3 &line1_p2, const Vector3 &line2_p1, const Vector3 &line2_p2, float &x, float &y)
{
	float a1 = line2_p2.y - line2_p1.y;
	float b1 = line2_p1.x - line2_p2.x;
	float c1 = (a1*line2_p1.x) + (b1*line2_p1.y);

	float a2 = line1_p2.y - line1_p1.y;
	float b2 = line1_p1.x - line1_p2.x;
	float c2 = (a2*line1_p1.x) + (b2*line1_p1.y);

	float det = (a1*b2) - (a2*b1);
	if(det != 0)
	{
		x = ( (b2*c1) - (b1*c2) )/det;
		y = ( (a1*c2) - (a2*c1) )/det;

		return true;
	}

	return false;
}


void CClimbDetector::AdjustHandholdPoint(const Matrix34& testMat, CClimbHandHoldDetected& inOutHandHold, const WorldProbe::CShapeTestHitPoint& adjacent1, const WorldProbe::CShapeTestHitPoint& adjacent2)
{
	Vector3 vPlayerForward = VEC3V_TO_VECTOR3(m_Ped.GetTransform().GetB());

	Vector3 player1 = testMat.d;
	Vector3 player2 = testMat.d + (vPlayerForward * 10.0f);
	Vector3 adjacentHit1 = adjacent1.GetHitPosition();
	Vector3 adjacentHit2 = adjacent2.GetHitPosition();

	//! Do a line-line intersection test to see if out projected heading interests our handhold. If it does, get intersection point,
	//! then adjust heading to accommodate.
	//! Note: This is a cheap approximation to doing a new set of intersection tests. The results _should_ be reliable enough to use. And
	//! we perform validation tests on this anyway.
	float x = 0.0f, y = 0.0f;
	if(LineIntersection2D(player1, player2, adjacentHit1, adjacentHit2, x, y))
	{
		Vector3 vHandHold = inOutHandHold.GetHandHold().GetPoint();
		Vector3 vAdjustedHandHold(x, y, vHandHold.z);

		//! Now find z. Just do 2D line test downwards (so ignore y).
		player1 = vAdjustedHandHold;
		player2 = vAdjustedHandHold;
		player1.z += 10.0f;
		player2.z -= 10.0f;

		player1.y = player1.z;
		player2.y = player2.z;
	
		adjacentHit1.y = adjacentHit1.z;
		adjacentHit2.y = adjacentHit2.z;

		if(LineIntersection2D(player1, player2, adjacentHit1, adjacentHit2, x, y))
		{
			vAdjustedHandHold.z = y;
		}

		if(vAdjustedHandHold.IsClose(vHandHold, 0.5f))
		{
			inOutHandHold.SetPoint(vAdjustedHandHold);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::SetClimbPhysical(CClimbHandHoldDetected& inOutHandHold, WorldProbe::CShapeTestHitPoint& mainHandhold)
{
	// Set the physical entity - used to attach us to dynamic objects
	CEntity* pEntity = CPhysics::GetEntityFromInst(mainHandhold.GetInstance());
	if(pEntity && pEntity->GetIsPhysical())
	{
		//! No auto-vaulting on vehicles we aren't standing on.
		if(m_flags.IsFlagSet(DF_AutoVault) && static_cast<CPhysical*>(pEntity)->GetIsTypeVehicle())
		{
			if(m_Ped.GetGroundPhysical() != pEntity)
			{
				return false;
			}
		}

		u16 iHandHoldComponent = mainHandhold.GetComponent();

		//! if we get this stage, and it is an unclimbable vehicle door, never attach directly to it, use vehicle instead.
		if(IsIntersectionEntityAnUnClimbableDoor(pEntity, mainHandhold, true))
		{
			iHandHoldComponent = 0;
		}

		// Do not use wheels as hand hold component, B* 1757504
		if(static_cast<CPhysical*>(pEntity)->GetIsTypeVehicle())
		{
			if(CCarEnterExit::GetCollidedWheelFromCollisionComponent(static_cast<CVehicle*>(pEntity), mainHandhold.GetComponent()))
			{
				iHandHoldComponent = 0;
			}
		}

		inOutHandHold.SetPhysical(static_cast<CPhysical*>(pEntity), iHandHoldComponent);
		inOutHandHold.CalcPhysicalHandHoldOffset();
	}
	else
	{
		inOutHandHold.SetPhysical(NULL, 0);
	}	

	return true;
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CClimbDetector::Debug()
{
	if(ms_pDebugDrawStore)
	{
		ms_pDebugDrawStore->Render();
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::GetSweptSphereIntersection(const WorldProbe::CShapeTestHitPoint& intersectionSphere, 
												const WorldProbe::CShapeTestHitPoint& intersectionSweptSphere, 
												WorldProbe::CShapeTestHitPoint& outIntersection) const
{
	if(GetIsValidIntersection(intersectionSphere))
	{
		outIntersection = intersectionSphere;
		return true;
	}
	else if(GetIsValidIntersection(intersectionSweptSphere))
	{
		outIntersection = intersectionSweptSphere;
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::ComputeHandHoldPoint(const Matrix34& testMat, 
										  const WorldProbe::CShapeTestHitPoint& intersection, 
										  const f32 fStartDist, 
										  const f32 fEndDist, 
										  const Vector3 &vAvgIntersectionNormal,
										  sPedTestParams *pPedParams, 
										  WorldProbe::CShapeTestHitPoint (&outIntersection)[eHT_NumTests]) const
{
	PF_FUNC(ClimbCompute);

	// Return value
	bool bValidHandHoldPoint = false;

	// Compute the right vector
	Vector3 vRight;
	//vRight.Cross(RCC_VECTOR3(intersection.GetIntersectedPolyNormal()), ZAXIS);
	vRight.Cross(vAvgIntersectionNormal, ZAXIS);
	vRight.z = 0.0f;

	Vector3 vUpDirection = ZAXIS;

	//! If a vehicle, angle tests to normal.
	/*if(intersection.GetHitEntity() && intersection.GetHitEntity()->GetIsTypeVehicle())
	{
		Vector3 vTempUp;
		vTempUp.Cross(-vAvgIntersectionNormal, vRight);
		if(vTempUp.Dot(testMat.b) > 0.0f)
		{
			vUpDirection = vTempUp; 
		}
	}*/

	float fTestRadius = 0.0f;

	if(!vRight.IsZero())
	{
		vRight.NormalizeFast();

		// Perform a test at this point
		static dev_float RADIUS = 0.1f;
		static dev_float QUADRUPED_RADIUS = 0.25f;
		static dev_float HEIGHT_TOLERANCE = 0.2f;
		static dev_float QUADRUPED_HEIGHT_TOLERANCE = 0.65f;
		static dev_float QUADRUPED_WIDTH_TOLERANCE = 1.25;
		static dev_float HANDHOLD_MIN_DOT = 0.1f;

		// Determine the test starting point
		Vector3 vTestStart(RCC_VECTOR3(intersection.GetPosition()));
		vTestStart += vUpDirection * fStartDist;

		// Get the test end
		Vector3 vTestEnd = vTestStart - (vUpDirection * fEndDist);

		float fHeightTolerance;
		if(IsQuadrupedTest())
		{
			fTestRadius = QUADRUPED_RADIUS;
			fHeightTolerance = QUADRUPED_HEIGHT_TOLERANCE;
			vTestStart.z += (fTestRadius*2.0f);
			vTestEnd.z -= (fTestRadius*2.0f);
		}
		else
		{
			fTestRadius = RADIUS;
			fHeightTolerance = HEIGHT_TOLERANCE;
		}

		aiAssertf(rage::FPIsFinite(vRight.x) && rage::FPIsFinite(vRight.y) && rage::FPIsFinite(vRight.z), "Invalid right vector: (%f:%f:%f)", vRight.x, vRight.y, vRight.z);
		aiAssertf(rage::FPIsFinite(vTestStart.x) && rage::FPIsFinite(vTestStart.y) && rage::FPIsFinite(vTestStart.z), "Invalid start vector: (%f:%f:%f)", vTestStart.x, vTestStart.y, vTestStart.z);
		aiAssertf(rage::FPIsFinite(vTestEnd.x) && rage::FPIsFinite(vTestEnd.y) && rage::FPIsFinite(vTestEnd.z), "Invalid end vector: (%f:%f:%f)", vTestEnd.x, vTestEnd.y, vTestEnd.z);

		bool bLeftValid = false;
		bool bRightValid = false;

		WorldProbe::CShapeTestHitPoint intersectionMid;
		WorldProbe::CShapeTestHitPoint intersectionLeft;
		WorldProbe::CShapeTestHitPoint intersectionRight;

		BANK_ONLY(SetDebugEnabled(ms_bDebugDrawCompute, Color_yellow));

		TUNE_GROUP_BOOL(VAULTING, bDoHandHoldComputeSphereTests, true);
		TUNE_GROUP_BOOL(VAULTING, bComputeHandHoldPointBatch, false);

		if(bComputeHandHoldPointBatch)
		{
			static const u32 iNumTests = 3;
			phShapeTest<phShapeBatch> batchTester;
			batchTester.GetShape().AllocateSpheres(iNumTests);
			batchTester.GetShape().AllocateSweptSpheres(iNumTests);
			batchTester.SetExcludeInstance(m_Ped.GetCurrentPhysicsInst());

			Matrix34 cullMtx;
			cullMtx.Set(vRight, -RCC_VECTOR3(intersection.GetIntersectedPolyNormal()), ZAXIS);
			cullMtx.Normalize();
			cullMtx.d = vTestEnd;
			cullMtx.d.z += ((vTestStart.z - vTestEnd.z) * 0.5f);
			Vector3 vecHalfWidth(fTestRadius*iNumTests, fTestRadius, (vTestStart.z - vTestEnd.z) * 0.5f);
			batchTester.GetShape().SetCullBox(cullMtx, vecHalfWidth);

			WorldProbe::CShapeTestHitPoint intersections[iNumTests * 2];

			Vector3 vOffset[iNumTests];
			vOffset[0] = Vector3(Vector3::ZeroType);
			vOffset[1] = (vRight * fTestRadius * -2.0f);
			vOffset[2] = (vRight * fTestRadius *  2.0f);

#if __BANK
			if(GetDebugEnabled())
			{
				GetDebugDrawStore().AddOBB(-RCC_VEC3V(vecHalfWidth), RCC_VEC3V(vecHalfWidth), RCC_MAT34V(cullMtx), ms_debugDrawColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);
			}
#endif

			for(u32 i = 0; i < iNumTests; i++)
			{
				Vector3 vStart = vTestStart + vOffset[i];
				Vector3 vEnd = vTestEnd + vOffset[i];

				if(bDoHandHoldComputeSphereTests)
					batchTester.InitSphere(vStart, fTestRadius, &intersections[i*2], 1);

				phSegment segment(vStart, vEnd);
				batchTester.InitSweptSphere(segment, fTestRadius, &intersections[((i*2)+1)], 1);
#if __BANK
				if(GetDebugEnabled())
				{
					GetDebugDrawStore().AddLine(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), ms_debugDrawColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);
					GetDebugDrawStore().AddSphere(RCC_VEC3V(vEnd), fTestRadius, ms_debugDrawColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
					if(bDoHandHoldComputeSphereTests)
						GetDebugDrawStore().AddSphere(RCC_VEC3V(vStart), fTestRadius, ms_debugDrawColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
				}
#endif
			}
			batchTester.TestInLevel(m_Ped.GetCurrentPhysicsInst(), CLIMB_DETECTOR_DEFAULT_TEST_FLAGS, TYPE_FLAGS_ALL, phLevelBase::STATE_FLAGS_ALL, TYPE_FLAGS_NONE, CPhysics::GetLevel());

			//! Filter results.

			bool bIntersectionMid = GetSweptSphereIntersection(intersections[0],intersections[1], intersectionMid);
			if(bIntersectionMid)
			{
				bool bIntersectionLeft = GetSweptSphereIntersection(intersections[2],intersections[3], intersectionLeft);
				bool bIntersectionRight = GetSweptSphereIntersection(intersections[4],intersections[5], intersectionRight);

				if(bIntersectionLeft && (Abs(intersectionLeft.GetPosition().GetZf() - intersectionMid.GetPosition().GetZf()) < fHeightTolerance))
				{
					bLeftValid = true;
				}
				if(bIntersectionRight && (Abs(intersectionRight.GetPosition().GetZf() - intersectionMid.GetPosition().GetZf()) < fHeightTolerance))
				{
					bRightValid = true;
				}
#if __BANK
				if(GetDebugEnabled())
				{
					AddDebugIntersections(&intersectionMid, 1, ms_debugDrawColour);
					AddDebugIntersections(&intersectionLeft, 1, ms_debugDrawColour);
					AddDebugIntersections(&intersectionRight, 1, ms_debugDrawColour);
				}
#endif
			}
		}
		else
		{
			WorldProbe::CShapeTestHitPoint testIntersection[CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS];
			if(TestCapsule(vTestStart, vTestEnd, fTestRadius, testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, bDoHandHoldComputeSphereTests))
			{
				if(FindFirstValidIntersection(testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, intersectionMid) && (((intersectionMid.GetHitPolyNormal().z + intersectionMid.GetHitNormal().z) * 0.5f) > HANDHOLD_MIN_DOT) )
				{
					// Perform a test either side of the mid test
					// Left test
					if(TestOffsetCapsule(vTestStart, vTestEnd, fTestRadius, testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, vRight, fTestRadius * -2.0f, bDoHandHoldComputeSphereTests))
					{
						if (FindFirstValidIntersection(testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, intersectionLeft))
						{
							bLeftValid = Abs(intersectionLeft.GetPosition().GetZf() - intersectionMid.GetPosition().GetZf()) < fHeightTolerance;
						}
					}
					else if(m_flags.IsFlagSet(DF_JumpVaultTestOnly))
					{
						Vector3 vEnd = RCC_VECTOR3(intersectionMid.GetPosition()) + (testMat.b * 2.0f);
						if(TestOffsetCapsule(testMat.d, vEnd, fTestRadius, testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, testMat.a, fTestRadius * 2.0f, bDoHandHoldComputeSphereTests))
						{
							if (FindFirstValidIntersection(testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, intersectionLeft))
							{
								bLeftValid = Mag(Subtract(intersectionMid.GetPosition(), intersectionLeft.GetPosition())).Getf() < QUADRUPED_WIDTH_TOLERANCE;
							}
						}
					}

					// Right test
					if(TestOffsetCapsule(vTestStart, vTestEnd, fTestRadius, testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, vRight, fTestRadius * 2.0f, bDoHandHoldComputeSphereTests))
					{
						if (FindFirstValidIntersection(testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, intersectionRight))
						{
							bRightValid = Abs(intersectionRight.GetPosition().GetZf() - intersectionMid.GetPosition().GetZf()) < fHeightTolerance;
						}
					}
					else if(m_flags.IsFlagSet(DF_JumpVaultTestOnly))
					{
						Vector3 vEnd = RCC_VECTOR3(intersectionMid.GetPosition()) + (testMat.b * 2.0f);
						if(TestOffsetCapsule(testMat.d, vEnd, fTestRadius, testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, testMat.a, fTestRadius * -2.0f, bDoHandHoldComputeSphereTests))
						{
							if (FindFirstValidIntersection(testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, intersectionRight))
							{
								bRightValid = Mag(Subtract(intersectionMid.GetPosition(), intersectionRight.GetPosition())).Getf() < QUADRUPED_WIDTH_TOLERANCE;
							}
						}
					}
				}
			}
		}

		const CEntity *pEntity = intersection.GetHitEntity();
		f32 fHandholdHeight = intersectionMid.GetHitPosition().z - GetPedGroundPosition(testMat).z;
		bool bUseTestOrientation = pPedParams ? (fHandholdHeight < pPedParams->fUseTestOrientationHeight && (pEntity == NULL || !pEntity->GetIsTypePed())) : false;

		// Are both valid?
		if(bLeftValid && bRightValid)
		{
			outIntersection[eHT_Main] = intersectionMid;
			outIntersection[eHT_Adjacent1] = intersectionLeft;
			outIntersection[eHT_Adjacent2] = intersectionRight;

			static dev_float s_fMinWidthScale = 0.6f;
			static dev_float s_fAdditionalTestOffset = 3.0f;
			float fMaxWidth = fTestRadius * 6.0f;

			Vector3 vHandholdDist = outIntersection[eHT_Adjacent1].GetHitPosition() - outIntersection[eHT_Adjacent2].GetHitPosition();
			float fHandHoldDistSq = vHandholdDist.XYMag2();
			float fSmallHandholdDistSq = (fMaxWidth * s_fMinWidthScale) * (fMaxWidth * s_fMinWidthScale);

			//! Detected a very small handhold. This requires further testing to determine if it meets minimum
			//! width.
			if( (fHandHoldDistSq < fSmallHandholdDistSq) && bUseTestOrientation && !m_flags.IsFlagSet(DF_JumpVaultTestOnly))
			{
				Vector3 vHandholdToAdj1 = outIntersection[eHT_Adjacent1].GetHitPosition() -  outIntersection[eHT_Main].GetHitPosition();
				float fDistToAdj1Sq = vHandholdToAdj1.XYMag2();

				Vector3 vHandholdToAdj2 = outIntersection[eHT_Adjacent2].GetHitPosition() -  outIntersection[eHT_Main].GetHitPosition();
				float fDistToAdj2Sq = vHandholdToAdj2.XYMag2();

				//! Invalidate closest adjacent intersection. 
				f32 fOffset;
				if(fDistToAdj1Sq < fDistToAdj2Sq)
				{
					//! Do longer test left.
					fOffset = fTestRadius * -s_fAdditionalTestOffset;
				}
				else
				{
					//! Do longer test right.
					fOffset = fTestRadius * s_fAdditionalTestOffset;
				}

				WorldProbe::CShapeTestHitPoint testIntersection[CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS];
				if(TestOffsetCapsule(vTestStart, vTestEnd, fTestRadius, testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, vRight, fOffset, bDoHandHoldComputeSphereTests))
				{
					if(FindFirstValidIntersection(testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, testIntersection[0]) && (Abs(testIntersection[0].GetPosition().GetZf() - intersectionMid.GetPosition().GetZf()) < fHeightTolerance) )
					{
						bValidHandHoldPoint = true;
					}
				}
			}
			else
			{
				bValidHandHoldPoint = true;
			}
		}

		bool bVehicle = false;
		bool bSmallPushubleObject = false;
		if(pEntity)
		{
			bVehicle = pEntity->GetIsTypeVehicle();

			if(pEntity->GetIsPhysical())
			{
				bSmallPushubleObject = IsClimbingSmallOrPushableObject(static_cast<const CPhysical*>(pEntity), intersection.GetHitInst());
			}
		}

		bool bDoAdditionalAdjancyTest = !bUseTestOrientation && 
				!m_flags.IsFlagSet(DF_JumpVaultTestOnly) && 
				!m_flags.IsFlagSet(DF_AutoVault) && 
				!bVehicle &&
				!bSmallPushubleObject;

		// Are any valid? 
		// Note 1 : when looking for a jump point, we don't want a handhold point, we are looking for a ledge that is
		// wide enough, which it isn't if both points fail.
		// Note 2 : Don't do this for auto-vault - makes it too easy to climb on edges.
		
		if( !bValidHandHoldPoint && (bLeftValid || bRightValid) && bDoAdditionalAdjancyTest)
		{
			WorldProbe::CShapeTestHitPoint* pIntersection;
			f32 fOffset;
			if(bLeftValid)
			{
				pIntersection = &intersectionLeft;
				fOffset = fTestRadius * -4.0f;
			}
			else
			{
				pIntersection = &intersectionRight;
				fOffset = fTestRadius * 4.0f;
			}

			outIntersection[eHT_Adjacent1] = intersectionMid;

			// Perform an additional test further along the direction that is valid,
			// if this is valid, then set the outIntersection to be the new mid point
			WorldProbe::CShapeTestHitPoint testIntersection[CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS];
			if(TestOffsetCapsule(vTestStart, vTestEnd, fTestRadius, testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, vRight, fOffset, bDoHandHoldComputeSphereTests))
			{
				if(FindFirstValidIntersection(testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, testIntersection[0]) && (Abs(testIntersection[0].GetPosition().GetZf() - intersectionMid.GetPosition().GetZf()) < fHeightTolerance) )
				{
					outIntersection[eHT_Main] = *pIntersection;
					outIntersection[eHT_Adjacent2] = intersection;
					bValidHandHoldPoint = true;
				}
			}
		}
	}

	BANK_ONLY(SetDebugEnabled(false));
	return bValidHandHoldPoint;
}

bool CClimbDetector::GetCloneVaultHandHoldPoint(sPedTestParams *pPedParams, WorldProbe::CShapeTestHitPoint (&outIntersection)[eHT_NumTests]) const
{
	bool bSucceeded = false;
	BANK_ONLY(SetDebugEnabled(ms_bDebugDrawValidate, Color_DarkGreen));

	Assertf(m_Ped.IsNetworkClone(),"Only expect clones to use this");

	if(pPedParams)
	{
		static float CLONE_TEST_RADIUS = 0.11f;
		static float CLONE_HORIZONTAL_PROBE_RADIUS = 0.215f;
		static float CLONE_HORIZONTAL_SEPARATION = CLONE_HORIZONTAL_PROBE_RADIUS * 2.0f;

		WorldProbe::CShapeTestHitPoint intersectionMid;
		WorldProbe::CShapeTestHitPoint intersectionLeft;
		WorldProbe::CShapeTestHitPoint intersectionRight;

		Vector3 vCloneDetectPoint	= pPedParams->vCloneHandHoldPoint;
		Vector3 vCloneDetectNormal	= pPedParams->vCloneHandHoldNormal;


		// Compute the right vector
		Vector3 vRight;
		//vRight.Cross(RCC_VECTOR3(intersection.GetIntersectedPolyNormal()), ZAXIS);
		vRight.Cross(vCloneDetectNormal, ZAXIS);
		vRight.z = 0.0f;

		Vector3 vUpDirection = ZAXIS;

		float fTestRadius = CLONE_TEST_RADIUS;

		bool bLeftValid = false;
		bool bRightValid = false;

		Vector3 vTestStart = vCloneDetectPoint;
//		vTestStart += vUpDirection * CLONE_START_DIST;

		// Attempt to detect a valid ledge point
		f32 fStartDist = CLONE_HORIZONTAL_SEPARATION + CLONE_HORIZONTAL_PROBE_RADIUS;
		f32 fEndDist = CLONE_HORIZONTAL_SEPARATION * 2.0f;

		// Determine the test starting point
//		Vector3 vTestStart(RCC_VECTOR3(intersection.GetPosition()));
		vTestStart += vUpDirection * fStartDist;

		// Get the test end
		Vector3 vTestEnd = vTestStart - (vUpDirection * fEndDist);

		WorldProbe::CShapeTestHitPoint testIntersection[CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS];
		if(TestCapsule(vTestStart, vTestEnd, fTestRadius, testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, true))
		{
			if(FindFirstValidIntersection(testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, intersectionMid))
			{
				// Perform a test either side of the mid test
				// Left test
				if(TestOffsetCapsule(vTestStart, vTestEnd, fTestRadius, testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, vRight, fTestRadius * -2.0f, true))
				{
					bLeftValid = FindFirstValidIntersection(testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, intersectionLeft);
				}

				// Right test
				if(TestOffsetCapsule(vTestStart, vTestEnd, fTestRadius, testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, vRight, fTestRadius * 2.0f, true))
				{
					bRightValid = FindFirstValidIntersection(testIntersection, CLIMB_DETECTOR_MAX_EDGE_INTERSECTIONS, intersectionRight);
				}

				outIntersection[eHT_Main] = intersectionMid;
				bSucceeded = true;
			}
		}
		CLIMB_DEBUGF(" CLONE HAND HOLD bLeftValid %s bRightValid %s \n",bLeftValid ?"YES":"NO",bRightValid ?"YES":"NO" );

		// Are both valid?
		if(bLeftValid && bRightValid) 
		{
			outIntersection[eHT_Adjacent1] = intersectionLeft;
			outIntersection[eHT_Adjacent2] = intersectionRight;
		}
	}

	BANK_ONLY(SetDebugEnabled(false));
	return bSucceeded;
}


////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::ValidateHandHoldAndDetermineVolume(const Matrix34& testMat, 
														const Vector3& vTestDirection, 
														CClimbHandHoldDetected& inOutHandHold, 
														sPedTestParams *pPedParams, 
														sPedTestResults *pPedResults,
														const float (&minIntesectionDistances)[CLIMB_DETECTOR_MAX_HORIZ_TESTS],
														const WorldProbe::CShapeTestHitPoint *pIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS])
{

	bool bClonePed = m_Ped.IsNetworkClone();

	PF_FUNC(ClimbValidate);

	BANK_ONLY(SetDebugEnabled(ms_bDebugDrawValidate));

	// Cache hand hold values
	const Vector3& vHandHoldPoint(inOutHandHold.GetHandHold().GetPoint());

	//
	// Firstly, determine if the space between the ped and hand hold is clear
	//

	const CBaseCapsuleInfo* pCapsuleInfo = m_Ped.GetCapsuleInfo();
	aiAssert(pCapsuleInfo);

	static dev_float	DIST_FROM_HANDHOLD_TO_TEST_FOR_NEAR_INTERSECTIONS = 0.75f;
	static dev_float	INTERSECTION_DIST_FROM_FROM_HANDHOLD_TO_REJECT = 0.5f;

	//! Test for intersections between head height and handhold. If we have any, check that they are within a certain tolerance. If too
	//! far away, it indicates we have hit something that we shouldn't climb through.
	float fCapsuleTop = testMat.d.z + pCapsuleInfo->GetMaxSolidHeight();
	if(!bClonePed && fCapsuleTop > vHandHoldPoint.z)
	{
		Vector3 vPedToHandHold(vHandHoldPoint);
		vPedToHandHold -= testMat.d;
		float fDistToHandholdXY = vPedToHandHold.XYMag();

		if(fDistToHandholdXY > DIST_FROM_HANDHOLD_TO_TEST_FOR_NEAR_INTERSECTIONS)
		{
			for(int i = 0; i < CLIMB_DETECTOR_MAX_HORIZ_TESTS; ++i)
			{
				if(pIntersections[i])
				{
					Vector3 vHitPoint = pIntersections[i]->GetHitPosition();
					if( (vHitPoint.z > testMat.d.z) && (vHitPoint.z < fCapsuleTop) )
					{
						float fIntersectionDist = minIntesectionDistances[i];

						vHitPoint -= vHandHoldPoint;
						float fHitDistToHandhold = vHitPoint.XYMag();
						if( (fIntersectionDist < fDistToHandholdXY) && abs(fHitDistToHandhold) > INTERSECTION_DIST_FROM_FROM_HANDHOLD_TO_REJECT)
						{
							// Something in the way
							CLIMB_DEBUGF("CClimbDetector::ValidateHandHoldAndDetermineVolume: Failed abs(fHitDistToHandhold - fDistToHandholdXY) > INTERSECTION_DIST_FROM_FROM_HANDHOLD_TO_REJECT");
							return false;
						}
					}
				}
			}
		}
	}


	//
	// Determine if there are any obstructions between us and the hand hold
	//

	WorldProbe::CShapeTestHitPoint intersection;

	const float			PED_TO_HANDHOLD_RADIUS = Max(pCapsuleInfo->GetHalfWidth()*0.5f, 0.05f);
	static dev_float	HAND_HOLD_Z_OFFSET = 0.2f;
	static dev_float	HAND_HOLD_OFFSET = 0.335f; 

	Vector3 vPedToHandHoldCheckEnd(vHandHoldPoint);
	vPedToHandHoldCheckEnd.z += HAND_HOLD_Z_OFFSET;
	vPedToHandHoldCheckEnd += (vTestDirection * HAND_HOLD_OFFSET);

	Vector3 vPedToHandholdCheckStart = testMat.d;

	BANK_ONLY(SetDebugEnabled(ms_bDebugDrawValidate, Color_cyan));
	if(!bClonePed && TestCapsule(vPedToHandholdCheckStart, vPedToHandHoldCheckEnd, PED_TO_HANDHOLD_RADIUS, &intersection, 1))
	{
		// Something in the way
		CLIMB_DEBUGF("CClimbDetector::ValidateHandHoldAndDetermineVolume: Failed TestCapsule(vPedToHandholdCheckStart, vPedToHandHoldCheckEnd, PED_TO_HANDHOLD_RADIUS, &intersection, 1)");
		return false;
	}

	//
	// Determine the depth of hand hold
	//

	static dev_float CLEAR_RADIUS						= 0.225f;
	static dev_float CLEAR_OFFSET						= 0.15f;
	static dev_float QUADRUPED_PHYSICAL_CLEAR_OFFSET	= 0.5f;
	static dev_float CLEAR_BACK_FROM_HANDHOLD			= 0.25f;
	static dev_float CLEAR_FORWARD						= 3.0f;
	static dev_float CLEAR_FORWARD_QUADRUPED			= 5.0f;
	static dev_u32	 CLEAR_TESTS						= 2;
	static dev_u32	 MIN_DEPTH_TESTS_AUTO_VAULT			= 4;

	float fClearanceForward = IsQuadrupedTest() ? CLEAR_FORWARD_QUADRUPED : CLEAR_FORWARD;
	float fClearOffset = CLEAR_OFFSET;
	
	//! Lift test for quadruped if we hit something physical (i.e. a car)
	if(IsQuadrupedTest() && inOutHandHold.GetPhysical())
	{
		fClearOffset += QUADRUPED_PHYSICAL_CLEAR_OFFSET;
	}

	static dev_float	DEPTH_RADIUS = 0.275f;
	static dev_float	DEPTH_LENGTH = 5.0f;
	static const u32	DEPTH_TESTS = 5;

	float fDepthLengthForPed = m_Ped.IsLocalPlayer() ? CPlayerInfo::GetFallHeightForPed(m_Ped) : DEPTH_LENGTH;

	//! if auto-vaulting, we need at least 3 tests to determine invalid drop heights.
	u32 minDepthTests = Min(m_flags.IsFlagSet(DF_AutoVault) ? MIN_DEPTH_TESTS_AUTO_VAULT : 1, DEPTH_TESTS);

	u32 cloneNumDepthTests = 0;

	if(bClonePed && pPedParams && pPedParams->uCloneMinNumDepthTests >0 )
	{
		cloneNumDepthTests = pPedParams->uCloneMinNumDepthTests;
	}

	Vector3 vDepthCheckStart(vHandHoldPoint);

	// Make the height equal to the height of the "clear" tests
	vDepthCheckStart.z += fClearOffset + CLEAR_TESTS * CLEAR_RADIUS * 2.0f - (DEPTH_RADIUS - CLEAR_RADIUS) - CLEAR_RADIUS;

	// Store the previous test height
	f32 fPreviousDepthTestHeight = vHandHoldPoint.z;

	Vector3 vClearanceTestNormalAVG(Vector3::ZeroType);
	Vector3 vHighestIntersection(0.0f, 0.0f, -9999.0f);
	s32 nHighestIntersectionDepth = 0;

	WorldProbe::CShapeTestHitPoint intersectionDepths[DEPTH_TESTS];
	s32 iLastDepthTest = 0;
	s32 iForcedLastDepth = -1;
	u8 iNumIntersectionNormals = 0;
	float fMaxDrop = 0.0f;
	bool bHitMaxDrop = false;

	for(iLastDepthTest = 0; iLastDepthTest < DEPTH_TESTS; iLastDepthTest++)
	{
		Vector3 vTestStart(vDepthCheckStart);
		vTestStart += static_cast<f32>(iLastDepthTest) * vTestDirection * -DEPTH_RADIUS * 2.0f;

		Vector3 vTestEnd(vTestStart);
		vTestEnd.z -= fDepthLengthForPed;

		BANK_ONLY(SetDebugEnabled(ms_bDebugDrawValidate, Color_green));
		if(TestCapsule(vTestStart, vTestEnd, DEPTH_RADIUS, &intersectionDepths[iLastDepthTest], 1))
		{
			Vector3 intersectionNormal = RCC_VECTOR3(intersectionDepths[iLastDepthTest].GetIntersectedPolyNormal());
			static dev_float s_fNormalTheshold = 0.0f; //90 degrees
			if(intersectionNormal.Dot(ZAXIS) >= s_fNormalTheshold)
			{
				vClearanceTestNormalAVG += intersectionNormal;
				++iNumIntersectionNormals;
			}

			if(intersectionDepths[iLastDepthTest].GetHitPosition().z > vHighestIntersection.z)
			{
				vHighestIntersection = intersectionDepths[iLastDepthTest].GetHitPosition();
				nHighestIntersectionDepth = iLastDepthTest;
			}

			static dev_float TOLERANCE = 0.2f;
			if(Abs(intersectionDepths[iLastDepthTest].GetPosition().GetZf() - fPreviousDepthTestHeight) > TOLERANCE)
			{
				if(bClonePed && cloneNumDepthTests>0 && iLastDepthTest < cloneNumDepthTests)
				{  //B* 1635499 Clone got past this stage so set this detected intersection height to be the same as last and allow to continue
					Vector3 vlastDepth(RCC_VECTOR3(intersectionDepths[iLastDepthTest].GetPosition()));
					vlastDepth.z = fPreviousDepthTestHeight;
					intersectionDepths[iLastDepthTest].SetPosition(VECTOR3_TO_VEC3V(vlastDepth));
				}
				else
				{
					//! Cache highest drop height.
					float fDropHeight = fDropHeight = Max(vHandHoldPoint.z - intersectionDepths[iLastDepthTest].GetPosition().GetZf(), 0.0f);
					if(fDropHeight > fMaxDrop)
					{
						fMaxDrop = fDropHeight;
					}

					if(iForcedLastDepth == -1)
					{
						iForcedLastDepth = iLastDepthTest;
					}

					// Height difference change is too great. Bail if we have hit our min test threshold.
					if((iLastDepthTest+1) >= minDepthTests)
					{
						break;
					}
				}
			}
		}
		else
		{
			// Hit nothing - a large drop in height. Bail out. We'll record this as our drop height.
			bHitMaxDrop = true;
			break;
		}

		fPreviousDepthTestHeight = intersectionDepths[iLastDepthTest].GetPosition().GetZf();
	}

	bool bCantAutoVaultIntoDrop = false;
	if(bHitMaxDrop && (iLastDepthTest+1) <= minDepthTests)
	{
		bCantAutoVaultIntoDrop = true;
	}

	//! Rewind back to actual drop point.
	if(iForcedLastDepth >= 0)
	{
		iLastDepthTest = iForcedLastDepth;
	}

	if(iLastDepthTest != DEPTH_TESTS)
	{
		float fDropHeight;
		if(intersectionDepths[iLastDepthTest].IsAHit())
		{
			fDropHeight = Max(vHandHoldPoint.z - intersectionDepths[iLastDepthTest].GetPosition().GetZf(), 0.0f);
		}
		else
		{
			fDropHeight = fDepthLengthForPed;
			inOutHandHold.SetMaxDrop(true);
		}

		inOutHandHold.SetDrop(fDropHeight);
	}
	else
	{
		//could not find drop - just set to 0.0f.
		inOutHandHold.SetDrop(0.0f);
	}

	//! If the drop is outside of climb tolerance for height, then disallow.
	if(!bClonePed && m_flags.IsFlagSet(VF_DoMaxHeightDropCheck))
	{
		//! Drop is also outside climb height, so ignore.
		if( inOutHandHold.GetDrop() > (GetMaxClimbHeight(pPedParams)-GetMaxHeightDropAllowance()) )
		{
			CLIMB_DEBUGF("CClimbDetector::ValidateHandHoldAndDetermineVolume: Failed inOutHandHold.GetDrop() > (GetMaxClimbHeight()-GetMaxHeightDropAllowance())");
			return false;
		}

		//! Other side has a lower drop. Allow climb to make it more likely climbs can be 2-sided. 
		float fDiff = inOutHandHold.GetHeight() - inOutHandHold.GetDrop();
		if( (fDiff < 0.0f) || (inOutHandHold.GetDrop() < GetMinClimbHeight(pPedParams, inOutHandHold.GetPhysical() != NULL && inOutHandHold.GetPhysical()->GetIsTypePed())) )
		{
			CLIMB_DEBUGF("CClimbDetector::ValidateHandHoldAndDetermineVolume: Failed fDiff < 0.0f");
			return false;
		}
	}

	if(!bClonePed && iLastDepthTest>=0)
	{   //Let the clone know how many tests were needed
		inOutHandHold.SetCloneMinNumDepthTests((u8)iLastDepthTest );
	}
	// Store the depth
	iLastDepthTest--;
	if(iLastDepthTest < 0)
	{
		inOutHandHold.SetDepth(0.0f);
		inOutHandHold.SetGroundPositionAtDrop(inOutHandHold.GetHandHold().GetPoint());
		inOutHandHold.SetMaxGroundPosition(inOutHandHold.GetHandHold().GetPoint());
	}
	else if(iLastDepthTest < DEPTH_TESTS)
	{
		Vector3 vDepth(RCC_VECTOR3(intersectionDepths[iLastDepthTest].GetPosition()));
		vDepth -= vHandHoldPoint;
		inOutHandHold.SetDepth(vDepth.XYMag());

		Vector3 vDropPosition = intersectionDepths[iLastDepthTest].GetHitPosition();

		//! If our last 2 depth points are close together, this may indicate a lip. In this case, choose highest z.
		if(iLastDepthTest >= 1)
		{
			Vector3 vLastDepth = intersectionDepths[iLastDepthTest].GetHitPosition();
			Vector3 vPreviousDepth = intersectionDepths[iLastDepthTest-1].GetHitPosition();
			Vector3 vDistance = vLastDepth - vPreviousDepth;
			
			static dev_float s_fCloseDropSq = 0.1f*0.1f;
			if(vDistance.XYMag2() < s_fCloseDropSq )
			{
				vDropPosition.z = Max(vPreviousDepth.z, vLastDepth.z);
			}
		}

		inOutHandHold.SetGroundPositionAtDrop(vDropPosition);

		inOutHandHold.SetMaxGroundPosition(vHighestIntersection);
	}
	else
	{
		inOutHandHold.SetDepth(fClearanceForward);
	}

	//! Test jump vault depth. If we aren't able to do a normal climb, return.
	if(pPedResults && pPedParams)
	{
		//! Range can be tested inside or outside.
		bool bOutsideRange = ((inOutHandHold.GetDepth() < pPedParams->fJumpVaultDepthThresholdMin) || (inOutHandHold.GetDepth() > pPedParams->fJumpVaultDepthThresholdMax));
		bool bFailed = false;
		if(pPedParams->bDepthThresholdTestInsideRange)
		{
			if(bOutsideRange)
			{
				bFailed = true;
			}
		}
		else if(!bOutsideRange)
		{
			bFailed = true;
		}

		if(bFailed)
		{
			pPedResults->SetCanJumpVault(false);
			pPedResults->SetCanAutoJumpVault(false);

			if(!pPedResults->GetCanVault() && !pPedResults->GetGoingToVaultSoon())
			{
				CLIMB_DEBUGF("CClimbDetector::ValidateHandHoldAndDetermineVolume: Failed Depth Check");
				return false;
			}
		}
	}

	// DMKH. Average out depth normals, so that we can angle our clearance test to work on slopes.
	Vector3 vClearanceDirection = -vTestDirection;
	Vector3 vClimbGroundNormal = ZAXIS;
	float fClimbAngle = 0.0f;
	if(iNumIntersectionNormals > 0)
	{
		vClearanceTestNormalAVG /= iNumIntersectionNormals;
		vClearanceTestNormalAVG.Normalize();

		vClimbGroundNormal = vClearanceTestNormalAVG;

		TUNE_GROUP_FLOAT(VAULTING, fVaultSlopeMaxAngle, 42.5f, 0.0f, 90.0f, 0.1f); 
		TUNE_GROUP_FLOAT(HORSE_JUMPING, fQuadrupedSlopeMaxAngle, 20.0f, 0.0f, 90.0f, 0.1f); 
		
		Vector3 vSlopeNormal;
		Vector3 vSlope;
		bool bValid = false;
		if(nHighestIntersectionDepth > 0)
		{
			vSlope = intersectionDepths[nHighestIntersectionDepth].GetHitPosition() - vHandHoldPoint;
			vSlope.NormalizeFast();
			
			Vector3 vTempRight;
			vTempRight.Cross(vSlope, ZAXIS);
			vSlopeNormal.Cross(vTempRight, vSlope);
			vSlopeNormal.NormalizeFast();
			bValid = true;
		}
		else
		{
			vSlopeNormal = vClearanceTestNormalAVG;

			if(vClearanceTestNormalAVG.Dot(YAXIS) < (1.0f - CLIMB_EPSILON))
			{
				Matrix34 m;
				m.LookDown(vSlopeNormal, YAXIS);
				vSlope = vSlopeNormal;
				m.Transform3x3(-vTestDirection, vSlope);
				bValid = true;
			}
		}

		if(bValid)
		{
			float fAngle = vSlopeNormal.Angle(ZAXIS) * RtoD;
			bool bSteepSlope = IsQuadrupedTest() ? fAngle >= fQuadrupedSlopeMaxAngle : fAngle >= fVaultSlopeMaxAngle;
				
			if(!bSteepSlope)
			{
				//! Only orient up slopes. If we orient down, we can penetrate the handhold geometry (and, logically, it doesn't make much sense either).
				//! Note: This also indirectly tests for non-zero magnitude normals.
				if(vSlope.GetZ() > 0.0f)
				{
					vClearanceDirection = vSlope;
					fClimbAngle = fAngle;
				}
			}
		}
	}

	inOutHandHold.SetHandHoldGroundAngle(fClimbAngle);
	inOutHandHold.SetGroundNormalAVG(vClimbGroundNormal);

	Vector3 vHandHoldForClearance(vHandHoldPoint);

	//
	// Determine if the space above the hand hold is clear
	//

	Vector3 vClearCheckStart(vHandHoldForClearance);
	vClearCheckStart.z += fClearOffset + CLEAR_RADIUS;

	float fDepthSmallest = fClearanceForward;
	bool bHitDepth = false;
	if(!bClonePed)
	{
		for(s32 i = 0; i < CLEAR_TESTS; i++)
		{
			Vector3 vTestStart(vClearCheckStart);

			if(i==0)
			{
				vTestStart += (-vClearanceDirection * CLEAR_BACK_FROM_HANDHOLD);
			}
			vTestStart.z += i * CLEAR_RADIUS * 2.0f;

			Vector3 vTestEnd(vTestStart);
			vTestEnd += vClearanceDirection * fClearanceForward;

			BANK_ONLY(SetDebugEnabled(ms_bDebugDrawValidate, Color_magenta));
			if(TestCapsule(vTestStart, vTestEnd, CLEAR_RADIUS, &intersection, 1))
			{
				Vector3 vStartToHandheld = vHandHoldForClearance - vTestStart;
				Vector3 vStartToIntersection = RCC_VECTOR3(intersection.GetPosition()) - vTestStart;
				float distStartToHandheld = vStartToHandheld.XYMag();
				float distStartToIntersection = vStartToIntersection.XYMag();

				float fFailedDepth = distStartToIntersection - distStartToHandheld;	

				if(!m_flags.IsFlagSet(DF_JumpVaultTestOnly) && (distStartToIntersection > distStartToHandheld) )
				{
					// Hit an obstacle after hand hold position. Check distance is less than some stand threshold.
					TUNE_GROUP_FLOAT(VAULTING, fVaultClearanceMinDepth, 0.35f, 0.01f, 2.0f, 0.001f); 
					if(fFailedDepth <= fVaultClearanceMinDepth)
					{
						CLIMB_DEBUGF("CClimbDetector::ValidateHandHoldAndDetermineVolume: Failed fFailedDepth <= fVaultClearanceMinDepth");
						return false;
					}
				}
				else
				{
					// Intersection happens before handhold point. Just bail.
					CLIMB_DEBUGF("CClimbDetector::ValidateHandHoldAndDetermineVolume: Failed m_flags.IsFlagSet(DF_JumpVaultTestOnly) && (distStartToIntersection > distStartToHandheld)");
					return false;
				}

				if (fFailedDepth < fDepthSmallest)
				{
					fDepthSmallest = fFailedDepth;
					bHitDepth = true;
				}
			}
		}
	}
	inOutHandHold.SetHorizontalClearance(fDepthSmallest);
	inOutHandHold.SetMaxHorizontalClearance(!bHitDepth);

	//! Test jump vault depth. If we aren't able to do a normal climb, return.
	if(pPedResults && pPedParams)
	{
		//! Range can be tested inside or outside.
		bool bHorizontalClearanceFailed = (inOutHandHold.GetHorizontalClearance() < pPedParams->fJumpVaultMinHorizontalClearance);
		if(bHorizontalClearanceFailed)
		{
			pPedResults->SetCanJumpVault(false);
			pPedResults->SetCanAutoJumpVault(false);

			if(!pPedResults->IsValid())
			{
				CLIMB_DEBUGF("CClimbDetector::ValidateHandHoldAndDetermineVolume: Failed horizontal clearance check");
				return false;
			}
		}
	}

	if(m_flags.IsFlagSet(DF_AutoVault))
	{
		static dev_float MAX_AUTO_VAULT_DEPTH = 0.75f;
		static dev_float MAX_AUTO_VAULT_DROP_HEIGHT = 1.5f;
		static dev_float MAX_AUTO_VAULT_DROP_HEIGHT_WATER = 2.0f;

		//! It's ok to auto-vault onto ledges greater than MAX_AUTO_VAULT_DEPTH deep.
		if(m_flags.IsFlagSet(DF_JumpVaultTestOnly) || 
			((inOutHandHold.GetDepth() <= MAX_AUTO_VAULT_DEPTH) && inOutHandHold.GetHorizontalClearance() > MAX_AUTO_VAULT_DEPTH) )
		{
			//! Use max drop height to prevent auto-vault.
			float fDropZPos = vDepthCheckStart.z - fMaxDrop;

			if( bCantAutoVaultIntoDrop || ((GetPedGroundPosition(testMat).z - fDropZPos) > MAX_AUTO_VAULT_DROP_HEIGHT) )
			{
				float fWaterHeight = 0.0f;
				if(CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(inOutHandHold.GetGroundPositionAtDrop(), &fWaterHeight, true, MAX_AUTO_VAULT_DROP_HEIGHT_WATER)) 
				{
					if( (testMat.d.z - fWaterHeight) > MAX_AUTO_VAULT_DROP_HEIGHT_WATER)
					{
						CLIMB_DEBUGF("CClimbDetector::ValidateHandHoldAndDetermineVolume: Failed water test");
						return false;	
					}
				}
				else
				{
					CLIMB_DEBUGF("CClimbDetector::ValidateHandHoldAndDetermineVolume: Failed bCantAutoVaultIntoDrop");
					return false;	
				}
			}
		}
	}

	//
	// Determine the height above the hand hold ledge
	//

	static dev_float	CLEARANCE_RADIUS = 0.175f;
	static const s32	CLEARANCE_TESTS = 1;

	float fClearanceAdjustment = pPedParams ? pPedParams->fZClearanceAdjustment : 0.0f;

	Vector3 vClearanceCheckStart(vHandHoldForClearance);

	// Make the height equal to the height of the "clear" tests

	vClearanceCheckStart.z += fClearOffset + CLEAR_RADIUS;

	f32 fLowestClearance = FLT_MAX;

	WorldProbe::CShapeTestHitPoint intersectionClearances[CLEARANCE_TESTS];
	for(s32 i = 0; i < CLEARANCE_TESTS; i++)
	{
		Vector3 vTestStart(vClearanceCheckStart);
		vTestStart += (-vTestDirection * CLEARANCE_RADIUS);
		vTestStart += (-vTestDirection * static_cast<float>(i) * (CLEARANCE_RADIUS * 2.0f) );

		Vector3 vTestEnd(vTestStart);
		vTestEnd.z = vHandHoldForClearance.z + pCapsuleInfo->GetGroundToRootOffset() + pCapsuleInfo->GetMaxSolidHeight() - fClearanceAdjustment;

		BANK_ONLY(SetDebugEnabled(ms_bDebugDrawValidate, Color_yellow));
		if(TestCapsule(vTestStart, vTestEnd, CLEARANCE_RADIUS, &intersectionClearances[i], 1))
		{
			//! In jump tests, just bail, we don't have enough vertical clearance.
			if(m_flags.IsFlagSet(DF_JumpVaultTestOnly))
			{
				CLIMB_DEBUGF("CClimbDetector::ValidateHandHoldAndDetermineVolume: Failed m_flags.IsFlagSet(DF_JumpVaultTestOnly)");
				return false;
			}

			if(intersectionClearances[i].GetPosition().GetZf() < fLowestClearance)
			{
				fLowestClearance = intersectionClearances[i].GetPosition().GetZf();
			}
		}
	}

	fLowestClearance -= vHandHoldForClearance.z;

	inOutHandHold.SetVerticalClearance(fLowestClearance);

	//! Need to ask vault task if we can support this handhold. If we don't have the animation variation necessary, it'll return false. Ideally,
	//! it'd handle everything the climb detector throws at it, but that's not always the case right now.
	bool bJumpVault = pPedResults ? pPedResults->GetCanAutoJumpVault() : false;
	bool bDisableAutoVaultDelay = false;
	bool bAutoVault = m_flags.IsFlagSet(DF_AutoVault);
	if(!CTaskVault::IsHandholdSupported(inOutHandHold, GetPlayerPed(), bJumpVault, bAutoVault, false, bAutoVault ? &bDisableAutoVaultDelay : NULL))
	{
		CLIMB_DEBUGF("CClimbDetector::ValidateHandHoldAndDetermineVolume: Failed CTaskVault::IsHandholdSupported(inOutHandHold, bJumpVault)");
		return false;
	}

	m_flags.ChangeFlag(VF_DisableAutoVaultDelay, bDisableAutoVaultDelay);

	return true;
}

////////////////////////////////////////////////////////////////////////////////

s32 CClimbDetector::TestCapsule(const Vector3& vP0, const Vector3& vP1, const f32 fRadius, WorldProbe::CShapeTestHitPoint* pIntersections, const u32 uNumIntersections, const bool bDoInitialSphereTest) const
{
	PF_FUNC(ClimbCapsuleTest);

	u32 iMaxIntersections = MAX_SHAPE_TEST_INTERSECTIONS;
	iMaxIntersections = Min(uNumIntersections, iMaxIntersections);

	u32 nExcludeFlags = ArchetypeFlags::GTA_PICKUP_TYPE;

	static const s32 iNumExceptions = 2;
	const CEntity* ppExceptions[iNumExceptions] = { NULL };
	ppExceptions[0] = &m_Ped;
	ppExceptions[1] = GetPlayerPed();

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	WorldProbe::CShapeTestResults probeResults(pIntersections, (u8)iMaxIntersections);
	capsuleDesc.SetResultsStructure(&probeResults);
	capsuleDesc.SetCapsule(vP0, vP1, fRadius);
	capsuleDesc.SetIsDirected(true);
	capsuleDesc.SetDoInitialSphereCheck(bDoInitialSphereTest);
	capsuleDesc.SetIncludeFlags(CLIMB_DETECTOR_DEFAULT_TEST_FLAGS);
	//capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
	capsuleDesc.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);
	
	if(!NetworkSetExcludedSpectatorPlayerEntities(capsuleDesc))
	{
		capsuleDesc.SetExcludeEntities(ppExceptions,iNumExceptions);
	}

	capsuleDesc.SetExcludeTypeFlags(nExcludeFlags);
	capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);

	WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST);

#if __BANK
	for(s32 i = 0; i < probeResults.GetNumHits(); i++)
	{
		aiAssertf(IsEqualAll(pIntersections[i].GetPosition(), pIntersections[i].GetPosition()), "NaN detected from collision at (%.2f,%.2f,%.2f)", pIntersections[i].GetPosition().GetXf(), pIntersections[i].GetPosition().GetYf(), pIntersections[i].GetPosition().GetZf());
		aiAssertf(IsEqualAll(pIntersections[i].GetNormal(), pIntersections[i].GetNormal()), "NaN detected from collision at (%.2f,%.2f,%.2f)", pIntersections[i].GetPosition().GetXf(), pIntersections[i].GetPosition().GetYf(), pIntersections[i].GetPosition().GetZf());
		aiAssertf(IsEqualAll(pIntersections[i].GetIntersectedPolyNormal(), pIntersections[i].GetIntersectedPolyNormal()), "NaN detected from collision at (%.2f,%.2f,%.2f)", pIntersections[i].GetPosition().GetXf(), pIntersections[i].GetPosition().GetYf(), pIntersections[i].GetPosition().GetZf());
	}

	if(GetDebugEnabled())
	{
		GetDebugDrawStore().AddLine(RCC_VEC3V(vP0), RCC_VEC3V(vP1), ms_debugDrawColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);
		if(bDoInitialSphereTest)
		{
			GetDebugDrawStore().AddSphere(RCC_VEC3V(vP0), fRadius, ms_debugDrawColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
		}
		GetDebugDrawStore().AddSphere(RCC_VEC3V(vP1), fRadius, ms_debugDrawColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
		AddDebugIntersections(pIntersections, probeResults.GetNumHits(), ms_debugDrawColour);
	}
#endif // __BANK

	return probeResults.GetNumHits();
}

////////////////////////////////////////////////////////////////////////////////

bool  CClimbDetector::NetworkSetExcludedSpectatorPlayerEntities(WorldProbe::CShapeTestCapsuleDesc & capsuleDesc ) const
{
	if(NetworkInterface::IsGameInProgress() && m_Ped.IsPlayer())
	{
		const CPed * pScannerPed = &m_Ped;

		//Check if this machine is spectating and only exclude other peds if this ped is the one we are spectating
		if(NetworkInterface::IsInSpectatorMode())
		{
			if(!m_Ped.IsNetworkClone())
			{	//we can't be spectating this ped as it is locally controlled so leave it to process normally
				return false;
			}

			if(NetworkInterface::IsSpectatingPed(&m_Ped))
			{
				//If we are spectating this ped then since it is a clone, which has no intelligence regarding nearby peds, we use
				//our own ped object which is in the same position to get intelligence about nearby peds.
				pScannerPed = NetworkInterface::GetLocalPlayer() ? NetworkInterface::GetLocalPlayer()->GetPlayerPed() : 0;
			}
			else
			{		
				return false;
			}
		}

		Assert(pScannerPed);

		bool bCloneIgnoreAllNearbyPeds = pScannerPed->IsNetworkClone();
		static const float TEST_NEARBY_PED_RANGE = 3.0f;

		const int MINIMUM_NUM_EXCEPTIONS = 2; //start with enough for this ped and the one from GetPlayerPed (can be the mount or the same) for ppExceptions elements [0] and [1] below as in SP
		const int MAXIMUM_NUM_EXCEPTIONS = MAX_NUM_PHYSICAL_PLAYERS + 1; 

		CEntityScannerIterator entityScannerIterator = pScannerPed->GetPedIntelligence()->GetNearbyPeds();
		CPed * pTestSpectatorPed = (CPed*)entityScannerIterator.GetFirst();

		const CEntity* ppExceptions[MAXIMUM_NUM_EXCEPTIONS];
		ppExceptions[0] = pScannerPed;
		ppExceptions[1] = GetPlayerPed();

		s32 iNumExceptions = MINIMUM_NUM_EXCEPTIONS; 
		//Add up the number of nearby spectator players we need to exclude
		while(pTestSpectatorPed && iNumExceptions < MAXIMUM_NUM_EXCEPTIONS )
		{
			bool bIsSpectator = pTestSpectatorPed->IsPlayer()?NetworkInterface::IsPlayerPedSpectatingPlayerPed(pTestSpectatorPed, &m_Ped):false;
			bool bCloneIgnorePedInTestRange = false;

			if(bCloneIgnoreAllNearbyPeds)
			{
				Vector3 scannerPedPosition = VEC3V_TO_VECTOR3(pScannerPed->GetTransform().GetPosition());
				Vector3 testPedPosition = VEC3V_TO_VECTOR3(pTestSpectatorPed->GetTransform().GetPosition());

				if(!pTestSpectatorPed->GetIsInVehicle() && testPedPosition.IsClose(scannerPedPosition,TEST_NEARBY_PED_RANGE))
				{
					bCloneIgnorePedInTestRange = true;
				}
			}

			if(bIsSpectator || bCloneIgnorePedInTestRange)
			{
				ppExceptions[iNumExceptions++] = pTestSpectatorPed;
			}

			pTestSpectatorPed = (CPed*) entityScannerIterator.GetNext();
		}

		capsuleDesc.SetExcludeEntities(ppExceptions,iNumExceptions);
		return true;
	}

	return false;
}


////////////////////////////////////////////////////////////////////////////////

s32 CClimbDetector::TestOffsetCapsule(const Vector3& vP0, const Vector3& vP1, const f32 fRadius, WorldProbe::CShapeTestHitPoint* pIntersections, const u32 uNumIntersections, const Vector3& vDir, const f32 fOffset, const bool bDoInitialSphereTest) const
{
	Vector3 vNewP0(vP0);
	vNewP0 += vDir * fOffset;

	Vector3 vNewP1(vP1);
	vNewP1 += vDir * fOffset;

	return TestCapsule(vNewP0, vNewP1, fRadius, pIntersections, uNumIntersections, bDoInitialSphereTest);
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::TestCapsuleAsync(const Vector3& vP0, const Vector3& vP1, const f32 fRadius, sShapeTestResult &result, u32 nMaxResults, bool bAsync) const
{
	PF_FUNC(ClimbCapsuleTest);

	//! Do test if we are not currently waiting for the result of one.
	if(!result.pProbeResult->GetWaitingOnResults() && (m_eHorizontalTestPhase != eATS_InProgress))
	{	
		static const s32 iNumExceptions = 2;
		const CEntity* ppExceptions[iNumExceptions] = { NULL };
		ppExceptions[0] = &m_Ped;
		ppExceptions[1] = GetPlayerPed();

		result.Reset();

		s32 iMaxIntersections = Min(nMaxResults, CLIMB_DETECTOR_MAX_INTERSECTIONS);

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		capsuleDesc.SetResultsStructure(result.pProbeResult);
		capsuleDesc.SetMaxNumResultsToUse(iMaxIntersections);
		capsuleDesc.SetCapsule(vP0, vP1, fRadius);
		capsuleDesc.SetIsDirected(true);
		//! Initial spheres not supported in async mode.
		if(!bAsync)
		{
			capsuleDesc.SetDoInitialSphereCheck(true);
		}
		capsuleDesc.SetIncludeFlags(CLIMB_DETECTOR_DEFAULT_TEST_FLAGS);
		//capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
		capsuleDesc.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);
		capsuleDesc.SetExcludeEntities(ppExceptions,iNumExceptions);
		capsuleDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
		capsuleDesc.SetContext(WorldProbe::LOS_GeneralAI);

		WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, bAsync ? WorldProbe::PERFORM_ASYNCHRONOUS_TEST : WorldProbe::PERFORM_SYNCHRONOUS_TEST);
	}

	bool bReady = result.pProbeResult->GetResultsReady();

	if (bReady)
	{
#if __BANK
		for(s32 i = 0; i < result.pProbeResult->GetNumHits(); ++i)
		{
#if __ASSERT
			WorldProbe::CShapeTestHitPoint &testResult = (*result.pProbeResult)[i];
			aiAssertf(IsEqualAll(testResult.GetPosition(), testResult.GetPosition()), "NaN detected from collision at (%.2f,%.2f,%.2f). Start (%.2f,%.2f,%.2f) End (%.2f,%.2f,%.2f)", testResult.GetPosition().GetXf(), testResult.GetPosition().GetYf(),testResult.GetPosition().GetZf(),vP0.x, vP0.y, vP0.z,vP1.x, vP1.y, vP1.z);
			aiAssertf(IsEqualAll(testResult.GetNormal(), testResult.GetNormal()), "NaN detected from collision at (%.2f,%.2f,%.2f)", testResult.GetPosition().GetXf(), testResult.GetPosition().GetYf(), testResult.GetPosition().GetZf());
			aiAssertf(IsEqualAll(testResult.GetIntersectedPolyNormal(), testResult.GetIntersectedPolyNormal()), "NaN detected from collision at (%.2f,%.2f,%.2f)", testResult.GetPosition().GetXf(), testResult.GetPosition().GetYf(), testResult.GetPosition().GetZf());
#endif
		}

		if(GetDebugEnabled())
		{
			GetDebugDrawStore().AddLine(RCC_VEC3V(vP0), RCC_VEC3V(vP1), ms_debugDrawColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);
			GetDebugDrawStore().AddSphere(RCC_VEC3V(vP0), fRadius, ms_debugDrawColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
			GetDebugDrawStore().AddSphere(RCC_VEC3V(vP1), fRadius, ms_debugDrawColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
			AddDebugIntersections(&((*result.pProbeResult)[0]), result.pProbeResult->GetNumHits(), ms_debugDrawColour);
		}
#endif // __BANK
	}
	else
	{
#if __BANK
		result.iFrameDelay++;
#endif
	}

	return bReady;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::TestLineAsync(const Vector3& vP0, const Vector3& vP1, sShapeTestResult &result, u32 nMaxResults, bool bAsync) const
{
	PF_FUNC(ClimbLineTest);

	//! Do test if we are not currently waiting for the result of one.
	if(!result.pProbeResult->GetWaitingOnResults() && (m_eHorizontalTestPhase != eATS_InProgress))
	{	
		static const s32 iNumExceptions = 2;
		const CEntity* ppExceptions[iNumExceptions] = { NULL };
		ppExceptions[0] = &m_Ped;
		ppExceptions[1] = GetPlayerPed();

		result.Reset();

		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(result.pProbeResult);
		probeDesc.SetMaxNumResultsToUse(nMaxResults);
		probeDesc.SetStartAndEnd(vP0, vP1);
		probeDesc.SetIncludeFlags(CLIMB_DETECTOR_DEFAULT_TEST_FLAGS);
		//probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
		probeDesc.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);
		probeDesc.SetExcludeEntities(ppExceptions,iNumExceptions);
		probeDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
		probeDesc.SetContext(WorldProbe::LOS_GeneralAI);

		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, bAsync ? WorldProbe::PERFORM_ASYNCHRONOUS_TEST : WorldProbe::PERFORM_SYNCHRONOUS_TEST);
	}

	bool bReady = result.pProbeResult->GetResultsReady();

	if (bReady)
	{
#if __BANK
		for(s32 i = 0; i < result.pProbeResult->GetNumHits(); ++i)
		{
#if __ASSERT
			WorldProbe::CShapeTestHitPoint &testResult = (*result.pProbeResult)[i];
			aiAssertf(IsEqualAll(testResult.GetPosition(), testResult.GetPosition()), "NaN detected from collision at (%.2f,%.2f,%.2f)", testResult.GetPosition().GetXf(), testResult.GetPosition().GetYf(),testResult.GetPosition().GetZf());
			aiAssertf(IsEqualAll(testResult.GetNormal(), testResult.GetNormal()), "NaN detected from collision at (%.2f,%.2f,%.2f)", testResult.GetPosition().GetXf(), testResult.GetPosition().GetYf(), testResult.GetPosition().GetZf());
			aiAssertf(IsEqualAll(testResult.GetIntersectedPolyNormal(), testResult.GetIntersectedPolyNormal()), "NaN detected from collision at (%.2f,%.2f,%.2f)", testResult.GetPosition().GetXf(), testResult.GetPosition().GetYf(), testResult.GetPosition().GetZf());
#endif
		}

		if(GetDebugEnabled())
		{
			GetDebugDrawStore().AddLine(RCC_VEC3V(vP0), RCC_VEC3V(vP1), ms_debugDrawColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);
			AddDebugIntersections(&((*result.pProbeResult)[0]), result.pProbeResult->GetNumHits(), ms_debugDrawColour);
		}
#endif // __BANK
	}
	else
	{
#if __BANK
		result.iFrameDelay++;
#endif
	}

	return bReady;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::DoesIntersectionInvalidateClimb(const WorldProbe::CShapeTestHitPoint& intersection) const
{
	const phInst* pHitInst = intersection.GetHitInst();
	if(pHitInst)
	{
		const CEntity* pEntity = intersection.GetHitEntity();

		if(pEntity)
		{
			//! we hit a door in any of our 1st intersection tests.
			if(IsIntersectionEntityAnUnClimbableDoor(pEntity, intersection, false, true))
			{
				return true;
			}

			//! HACK. monster truck tyres. The collision for these is dodgy, which results in us climbing through them.
			if(pEntity->GetIsTypeVehicle())
			{
				const CVehicle *pVehicle = static_cast<const CVehicle*>(pEntity);

				if(pVehicle->pHandling && pVehicle->pHandling->hFlags & HF_EXT_WHEEL_BOUNDS_COL) // Prevent peds climbing on large exposed wheels( i.e monster truck)
				{
					for(int i = 0; i < pVehicle->GetNumWheels(); i++)
					{
						for(int k = 0; k < MAX_WHEEL_BOUNDS_PER_WHEEL; k++)
						{
							if(pVehicle->GetWheel(i)->GetFragChild(k) == intersection.GetComponent())
							{
								return true;
							}
						}
					}
				}
			}

			// HACK: Don't allow peds to climb onto the humpback whale
			if(pEntity->GetIsTypePed())
			{
				const CPed* pPed = static_cast<const CPed*>(pEntity);
				if (pPed->GetPedModelInfo()->GetHashKey() == ATSTRINGHASH("A_C_HumpBack", 0x471be4b2))
				{
					return true;
				}
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::IsIntersectionEntityAnUnClimbableDoor(const CEntity* pEntity, const WorldProbe::CShapeTestHitPoint& intersection, bool bTryingToAttachToDoor, bool bInvalidatingClimb) const
{
	//! Disable auto-vaulting over (some) doors.
	if(pEntity)
	{
		if(pEntity->GetIsTypeVehicle())
		{
			//! Disable auto-vaulting if we hit a car door. Also, don't vault moving doors as we'll just ragdoll.
			const CVehicle *pVehicle = static_cast<const CVehicle*>(pEntity);
			const CCarDoor* pDoor = CCarEnterExit::GetCollidedDoorFromCollisionComponent(pVehicle, intersection.GetComponent());
			if(pDoor)
			{
				if (pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_GULL_WING_DOORS))
				{
					if(bTryingToAttachToDoor)
					{
						return true;
					}

					if(!bInvalidatingClimb && !pDoor->GetIsClosed())
					{
						return true;
					}
				}

				if((m_flags.IsFlagSet(DF_AutoVault) || pDoor->GetCurrentSpeed() > 0.1f))
				{
					return true;
				}
		
				s32 nSeatIndexForDoor = pVehicle->GetSeatIndexFromDoor(pDoor);
				if(CTaskVehicleFSM::GetPedInOrUsingSeat(*pVehicle, nSeatIndexForDoor))
				{
					//! Don't allow climbing on door when a ped is in seat as they can open/close the door.
					//! By default, we only check this when door is open (we still want to climb when a ped is in a seat and has door shut)
					//! but we need to force this when we want to ensure that we never attach to the door in this case.
					if(!pDoor->GetIsClosed() || bTryingToAttachToDoor)
					{
						return true;
					}
				}
			}
		}
		else if(pEntity->GetIsTypeObject())
		{
			const CObject *pObject = static_cast<const CObject*>(pEntity);
			if(pObject->IsADoor())
			{
				//! Disable on hinged doors.
				const CDoor *pDoor = static_cast<const CDoor*>(pObject);
				if(pDoor)
				{
					//! Don't climb interior doors.
					if(m_Ped.GetIsInInterior())
					{
						return true;
					}

					//! Don't climb portal doors. Funny results ensue.
					fwSceneGraphNode* sceneNode = pDoor->GetOwnerEntityContainer()->GetOwnerSceneNode();
					if ( sceneNode && sceneNode->IsTypePortal() )
					{
						return true;
					}

					switch(pDoor->GetDoorType())
					{
					case(CDoor::DOOR_TYPE_STD):
					case(CDoor::DOOR_TYPE_STD_SC):
						{
							if(m_flags.IsFlagSet(DF_AutoVault))
							{
								//! Don't autovault over unlocked doors.
								bool bIsLocked = pDoor->IsBaseFlagSet( fwEntity::IS_FIXED ) || pDoor->IsBaseFlagSet( fwEntity::IS_FIXED_BY_NETWORK );
								if(!bIsLocked)
									return true;
							}
						}
						break;
					case(CDoor::DOOR_TYPE_GARAGE):
					case(CDoor::DOOR_TYPE_GARAGE_SC):
						//! Never climb garage doors.
						return true;
					default:
						break;
					}
				}
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::GetIsValidIntersection(const WorldProbe::CShapeTestHitPoint& intersection) const
{
	if(IsIntersectionClimbable(intersection))
	{
		const phInst* pHitInst = intersection.GetHitInst();
		if(pHitInst)
		{
			const CEntity* pEntity = intersection.GetHitEntity();

			if(pEntity)
			{
				//! Disable against peds - we just want to know if they are in the way.
				if(pEntity->GetIsTypePed())
				{
					bool bLargePed = false;
					if(pHitInst->GetClassType() == PH_INST_FRAG_PED && static_cast<const fragInstNMGta*>(pHitInst)->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH)
					{
						bLargePed = static_cast<const fragInstNMGta*>(pHitInst)->GetTypePhysics()->GetChild(intersection.GetHitComponent())->GetUndamagedMass() >= CPed::ms_fLargeComponentMass;
					}

					const CPed* pPed = static_cast<const CPed*>(pEntity);
					// If we hit a large component of a ragdolled corpse then consider vaulting!
					if(pHitInst->GetClassType() != PH_INST_FRAG_PED || pPed->GetDeathState() != DeathState_Dead || !pPed->ShouldBeDead() || !bLargePed)
					{
						return false;
					}
				}

				if(IsIntersectionEntityAnUnClimbableDoor(pEntity, intersection, false))
				{
					return false;
				}

				if(pEntity->GetIsPhysical())
				{
					const CPhysical *pPhysical = static_cast<const CPhysical*>(pEntity);
					if(pPhysical->GetClimbingDisabled())
					{
						return false;
					}

					//! Don't vault onto quickly spinning objects. Note: We only care about x, y axis. Objects spinning about z are fine as
					//! the attachment code is ok with this.
					static dev_float s_fAngularVelThreshold = 1.5f;
					float fXYAngVelSq = pPhysical->GetAngVelocity().XYMag2();
					if(fXYAngVelSq > (s_fAngularVelThreshold * s_fAngularVelThreshold))
					{
						return false;
					}
				}

				// Disable climbing on anything that a ped is attached to while running a scenario (chairs, benches).
				// Don't do this for vehicles, though.
				if (!pEntity->GetIsTypeVehicle())
				{
					const fwEntity* pChild = pEntity->GetChildAttachment();
					while (pChild)
					{
						if (pChild->GetType() == ENTITY_TYPE_PED)
						{
							const CPed* pPed = static_cast<const CPed*>(pChild);
							const CScenarioPoint* pScenarioPoint = CPed::GetScenarioPoint(*pPed);
							if (pScenarioPoint && pScenarioPoint->GetEntity() == pEntity)
							{
								return false;
							}
						}
						pChild = pChild->GetSiblingAttachment();
					}
				}

				if(m_flags.IsFlagSet(DF_AutoVault))
				{
					if(pEntity->GetIsPhysical())
					{
						const CPhysical *pPhysical = static_cast<const CPhysical*>(pEntity);
						if(pPhysical->GetAutoVaultDisabled())
						{
							return false;
						}

						//! Disable auto-vaulting over un-rootable props that the player ped can move.
						//! If object is fixed, that's ok - just treat it like static geometry.
						TUNE_GROUP_BOOL(VAULTING,bDisableAutoVaultOnUnFixedObjects,true);
						if(!m_flags.IsFlagSet(DF_JumpVaultTestOnly) && 
							bDisableAutoVaultOnUnFixedObjects &&
							IsClimbingSmallOrPushableObject(pPhysical, pHitInst))
						{
							return false;
						}
					}
				}

				if(pEntity->GetIsTypeVehicle() && !m_flags.IsFlagSet(DF_JumpVaultTestOnly))
				{
					const CVehicle *pVehicle = static_cast<const CVehicle*>(pEntity);
					
					if(pVehicle->InheritsFromTrain())
					{
						//! Don't climb onto oncoming trains!
						Vector3 vPlayerToIntersection = intersection.GetHitPosition() - VEC3V_TO_VECTOR3(m_Ped.GetTransform().GetPosition());
						vPlayerToIntersection.NormalizeFast();
						Vector3 vTrainVelocity = pVehicle->GetVelocity();
						Vector3 vPedVelocity = m_Ped.GetVelocity();
						Vector3 vRelativeVelocity = vPedVelocity - vTrainVelocity;
						vTrainVelocity.NormalizeFast();
						float fDot = DotProduct(vPlayerToIntersection, vTrainVelocity);
						
						static dev_float s_fInvalidDot = -0.5f;
						static dev_float s_fRelativeVelocityDifference = 5.0f;

						if(fDot < s_fInvalidDot && vRelativeVelocity.XYMag2() > (s_fRelativeVelocityDifference * s_fRelativeVelocityDifference) )
						{
							return false;
						}
					}
					else if(pVehicle->InheritsFromHeli())
					{
						//! Don't climb onto heli's that have spinning rotors.
						const CHeli *pHeliVehicle = static_cast<const CHeli*>(pVehicle);
						if(pHeliVehicle->GetMainRotorSpeed() > 0.1f)
						{
							return false;
						}
					}
					else
					{
						//! Don't allow vaulting on bikes.
						if(pVehicle->InheritsFromBike() || pVehicle->InheritsFromQuadBike() || pVehicle->InheritsFromAmphibiousQuadBike() || pVehicle->GetIsJetSki())
						{
							return false;
						}

						//! If object is a vehicle, disallow any vaulting above a certain speed.
						//! Note: It's ok to climb on vehicle if we are standing on it.
						static dev_float s_fVehicleVelocityThreshold = 5.0f;
						if(pVehicle != m_Ped.GetGroundPhysical() &&
							(pVehicle->GetVelocity().Mag2() > (s_fVehicleVelocityThreshold * s_fVehicleVelocityThreshold)))
						{
							return false;
						}
					}
				}

				// do not allow the player to entities that the player ped does not collide with in MP (eg ghost vehicles in passive mode)
				if (NetworkInterface::IsGameInProgress() && GetPlayerPed())
				{
					if (NetworkInterface::AreInteractionsDisabledInMP(*GetPlayerPed(), *pEntity))
					{
						return false;
					}
				}
			}

			//! Don't vault onto ledges under water.
			if(m_Ped.m_Buoyancy.GetStatus() != NOT_IN_WATER)
			{
				static dev_float s_fUnderwaterVaultThreshold = -0.2f; 
				static dev_float s_fUnderwaterVaultThresholdBoat = -1.0f; 

				float fUnderWaterThreshold = s_fUnderwaterVaultThreshold;
				if(pEntity && pEntity->GetIsTypeVehicle())
				{
					const CVehicle *pVehicle = static_cast<const CVehicle*>(pEntity);
					if(pVehicle->InheritsFromBoat())
					{
						fUnderWaterThreshold = s_fUnderwaterVaultThresholdBoat;
					}
				}

				if(intersection.GetHitPosition().z - m_Ped.m_Buoyancy.GetAbsWaterLevel() < fUnderWaterThreshold)
				{
					return false;
				}
			}

			if(!pEntity || !pEntity->IsEntityAParentAttachment(&m_Ped))
			{
				return true;
			}
		}
	}

	return false;
}

bool CClimbDetector::FindFirstValidIntersection(const WorldProbe::CShapeTestHitPoint* pIntersection, u32 uNumIntersection, WorldProbe::CShapeTestHitPoint& validIntersection) const
{
	for (int i = 0; i < uNumIntersection; i++)
	{
		if (!pIntersection[i].IsAHit())
		{
			break;
		}

		if (GetIsValidIntersection(pIntersection[i]))
		{
			validIntersection = pIntersection[i];
			return true;
		}

		if (!CanFindFurtherIntersections(pIntersection[i]))
		{
			break;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::IsClimbingSmallOrPushableObject(const CPhysical *pPhysical, const phInst* pHitInst) const
{
	if(!pPhysical || !pHitInst)
		return false;

	if(!pPhysical->GetIsAnyFixedFlagSet() && !pPhysical->GetIsTypeVehicle())
	{
		//! consider pushable at a small speed.
		static dev_float s_fDynamicObjectVelocityThresholdSq = 1.0f;
		if(pPhysical->GetVelocity().Mag2() > (s_fDynamicObjectVelocityThresholdSq))
		{
			return true;
		}

		//! Disable auto-vaulting onto objects that don't weigh much. Note: Non frag objects have a ridiculous mass
		//! setup atm, hence the crazy high numbers here.
		static dev_float s_sMassCanPush = 800.0f; 
		static dev_float s_sFragMassCanPush = 100.0f;
		static dev_float s_sMaxBoundsXZSize = 2.0f;

		fragInst * pFragInst = pPhysical->GetFragInst();
		float fMassToCheck = s_sMassCanPush;
		if(pFragInst)
		{
			bool bUprooted = pPhysical->GetIsTypeObject() ? static_cast<const CObject*>(pPhysical)->m_nObjectFlags.bHasBeenUprooted : false;
			if(!bUprooted)
				fMassToCheck = s_sFragMassCanPush;
		}

		float fMassOfCollision = pHitInst->GetArchetype()->GetMass();
		CLIMB_DEBUGF("Mass of collision instance: %f", fMassOfCollision);
		if (fMassOfCollision < fMassToCheck)
		{
			Vector3 vSize = VEC3V_TO_VECTOR3(pHitInst->GetArchetype()->GetBound()->GetBoundingBoxSize());
			float fBoundXZSize = vSize.x * vSize.z;
			CLIMB_DEBUGF("Size of XZ bounds: %f", fBoundXZSize);
			if (fBoundXZSize < s_sMaxBoundsXZSize) 
			{
				return true;								
			}	
		} 
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::GetIsEdgeValid(const CClimbDetector::sEdge& edge, const WorldProbe::CShapeTestHitPoint* pIntersectionStart, const WorldProbe::CShapeTestHitPoint* pIntersectionEnd) const
{
	if(IsQuadrupedTest())
	{
		//! If we have hit a 1 intersection edge, then test that it's not ~vertical. This would indicate that it's not possible to climb.
		if(edge.iStart == edge.iEnd)
		{
			TUNE_GROUP_FLOAT(HORSE_JUMPING, fHorseHorizontalIntersectionDot, 0.85f, -1.0f, 1.0f, 0.01f);
			Vector3 vHitNormal = RCC_VECTOR3(pIntersectionEnd->GetIntersectedPolyNormal());
			float fDot = vHitNormal.Dot(ZAXIS);
			if(fDot > fHorseHorizontalIntersectionDot)
			{
				CLIMB_DEBUGF("CClimbDetector::GetIsEdgeValid: Failed fDot > fHorseHorizontalIntersectionDot");
				return false;
			}
		}
	}

	bool bStairEdge = (PGTAMATERIALMGR->GetPolyFlagStairs(pIntersectionEnd->GetMaterialId()) && PGTAMATERIALMGR->GetPolyFlagStairs(pIntersectionStart->GetMaterialId()));

	// Detect stair edge. If so, it must be a certain length, otherwise we discard it.
	if(m_flags.IsFlagSet(DF_OnStairs) || bStairEdge)
	{
		static dev_float s_fSTAIR_EDGE_DISCARD_ALL_HEIGHT = 0.225f;
		static dev_float s_fSTAIR_EDGE_DISCARD_ALL_HEIGHT_STAIR_EDGE = 0.3f;
		static dev_float s_fMAX_STAIR_EDGE = 0.4f;
		static dev_float s_fSTAIR_INTERSECTION_TOLERANCE = 0.1f;

		float fStartEdgeHeight = pIntersectionStart->GetPosition().GetZf();
		float fEndEdgeHeight = pIntersectionEnd->GetPosition().GetZf();

		Vector3 vHitDiff = pIntersectionStart->GetHitPosition() - pIntersectionEnd->GetHitPosition();
		vHitDiff.z = 0.0f;

		float fEdgeHeight = abs(fEndEdgeHeight - fStartEdgeHeight);

		float fDiscardAllHeight = bStairEdge ? s_fSTAIR_EDGE_DISCARD_ALL_HEIGHT_STAIR_EDGE : s_fSTAIR_EDGE_DISCARD_ALL_HEIGHT;

		if( fEdgeHeight < fDiscardAllHeight)
		{
			CLIMB_DEBUGF("CClimbDetector::GetIsEdgeValid: Failed fEdgeHeight < s_fSTAIR_EDGE_DISCARD_ALL_HEIGHT");
			return false;
		}
		else if( (fEdgeHeight < s_fMAX_STAIR_EDGE ) && (vHitDiff.Mag() > s_fSTAIR_INTERSECTION_TOLERANCE) )
		{
			CLIMB_DEBUGF("CClimbDetector::GetIsEdgeValid: Failed (fEdgeHeight < s_fMAX_STAIR_EDGE ) && (vHitDiff.Mag() > s_fSTAIR_INTERSECTION_TOLERANCE)");
			return false;
		}

		if(m_flags.IsFlagSet(DF_AutoVault))
		{
			const Vector3& vNormal = m_Ped.GetSlopeNormal();
			float fSlopeHeading = rage::Atan2f(-vNormal.x, vNormal.y);
			if(abs(fSlopeHeading)>CLIMB_EPSILON)
			{
				float fHeadingToSlope = m_Ped.GetCurrentHeading() - fSlopeHeading;
				fHeadingToSlope = fwAngle::LimitRadianAngleSafe(fHeadingToSlope);

				// Are we going upwards on stairs?
				if (fHeadingToSlope >= -QUARTER_PI && fHeadingToSlope <= QUARTER_PI)
				{
					//! Disable auto-vaulting whilst running down stairs
					CLIMB_DEBUGF("CClimbDetector::GetIsEdgeValid: Failed fHeadingToSlope >= -QUARTER_PI && fHeadingToSlope <= QUARTER_PI");
					return false;
				}
			}
		}
	}

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::IsIntersectionOnSlope(const Matrix34& testMat, const WorldProbe::CShapeTestHitPoint& intersection, const Vector3 &vSlopeNormalAVG) const
{
	//! This function determines if ped is on the same plane as the handheld. I.e. there is a slope in the direction of the handhold.

	if(vSlopeNormalAVG.Mag2() < CLIMB_EPSILON)
		return false;

	Vector3 vPoint(RCC_VECTOR3(intersection.GetPosition()));

	TUNE_GROUP_FLOAT(VAULTING, fVaultSlopeCheckHeight, 1.0f, 0.0f, 3.0f, 0.1f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultSlopeDotDiff, 0.3f, -1.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultSlopeDist, 2.0f, 0.0f, 5.0f, 0.01f);
	TUNE_GROUP_FLOAT(VAULTING, fVaultSlopeMinZ, 0.65f, 0.0f, 1.0f, 0.01f);

	Vector3 vIntersectionToGround = vPoint - GetPedGroundPosition(testMat);

	bool bVaultSlopeDist = false;
	Vector3 vIntersectionNormal;
	if((vIntersectionToGround.XYMag2() >= (fVaultSlopeDist*fVaultSlopeDist)))
	{
		vIntersectionNormal = intersection.GetHitPolyNormal();
		bVaultSlopeDist = true;
	}
	else
	{
		vIntersectionNormal = vSlopeNormalAVG;
	} 

	f32 fHeight = vPoint.z - GetPedGroundPosition(testMat).z;
	if( ((fHeight >= 0.0f) && (fHeight < fVaultSlopeCheckHeight)) || bVaultSlopeDist ) 
	{
		Vector3 vSlopeNormal = m_Ped.GetSlopeNormal();
		Vector3 vGroundNormal = m_Ped.GetGroundNormal();

		Vector3 vIntersectionToGround = vPoint - GetPedGroundPosition(testMat);
		vIntersectionToGround.Normalize();
		float fIntersectionDot = vIntersectionNormal.Dot(vIntersectionToGround);

		//! Need to determine if the slope normal is valid. i.e. not horizontal vertical
		if(vSlopeNormal.z > fVaultSlopeMinZ)
		{
			float fSlopeDot = vSlopeNormal.Dot(vIntersectionToGround);
			if( (fabs(fSlopeDot - fIntersectionDot) < fVaultSlopeDotDiff) )
			{
				return true;
			}
		}

		//! Need to determine if the ground normal is valid. i.e. not horizontal vertical
		if(vGroundNormal.z > fVaultSlopeMinZ)
		{
			float fGroundDot = vGroundNormal.Dot(vIntersectionToGround);
			if( (fabs(fGroundDot - fIntersectionDot) < fVaultSlopeDotDiff) )
			{
				return true;
			}
		}
	}

	return false;
}

bool CClimbDetector::IsHandholdOnSlope(const Matrix34& testMat, const Vector3& vTestDirection, const CClimbHandHoldDetected& inHandHold) const
{
	TUNE_GROUP_FLOAT(VAULTING, fVaultSlopeDistToLine, 0.135f, 0.0f, 1.0f, 0.01f);

	//! The handhold slope test basically performs a test using current slope dir we are standing on. We project a line along this slope
	//! and test how close it is too the handhold. If it's close, we reject the handhold as it indicates that we can walk up to it.

	bool bSlopeNormalTest = DoSlopeTest(testMat, vTestDirection, inHandHold, m_Ped.GetSlopeNormal(), fVaultSlopeDistToLine);
	if(bSlopeNormalTest)
		return true;

	Vector3 vGroundNormal = m_Ped.GetGroundPhysical() ? VEC3V_TO_VECTOR3(m_Ped.GetMaxGroundNormal()) : m_Ped.GetGroundNormal();
	
	bool bGroundNormalTest = DoSlopeTest(testMat, vTestDirection, inHandHold, vGroundNormal, fVaultSlopeDistToLine);
	if(bGroundNormalTest)
		return true;

	return false;
}

bool CClimbDetector::DoSlopeTest(const Matrix34& testMat,
								 const Vector3& vTestDirection, 
								 const CClimbHandHoldDetected& inHandHold, 
								 const Vector3 &vTestNormal, 
								 float fDistToLineThreshold) const
{
	static const float s_fLineDistance = 10.0f;

	TUNE_GROUP_FLOAT(VAULTING, fVaultSlopeMinZ, 0.55f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(VAULTING, fBaseCapsuleOffset, 0.25f, 0.0f, 1.0f, 0.01f);

	Matrix34 m;
	if( (vTestNormal.z > fVaultSlopeMinZ) && (vTestNormal.Dot(YAXIS) < (1.0f - CLIMB_EPSILON)))
	{
		const Vector3& vPoint(inHandHold.GetHandHold().GetPoint());

		Vector3 vSlopeDirection; 
		m.LookDown(vTestNormal, YAXIS);
		m.Transform3x3(vTestDirection, vSlopeDirection);

		if(vSlopeDirection.z > 0.0f)
		{
			Vector3 vBaseCapsulePos = GetPedGroundPosition(testMat);
			vBaseCapsulePos.z += fBaseCapsuleOffset;
			fwGeom::fwLine line(vBaseCapsulePos, vBaseCapsulePos + (vSlopeDirection * s_fLineDistance) );
			Vector3 vClosest;
			line.FindClosestPointOnLine(vPoint, vClosest);
			Vector3 vDistanceToIntersection = vPoint - vClosest;

#if __BANK
			if(GetDebugEnabled())
			{
				GetDebugDrawStore().AddLine( VECTOR3_TO_VEC3V(vPoint), VECTOR3_TO_VEC3V(vClosest), Color_green, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);
				GetDebugDrawStore().AddLine( VECTOR3_TO_VEC3V(vBaseCapsulePos), VECTOR3_TO_VEC3V(vBaseCapsulePos + (vSlopeDirection * s_fLineDistance)), Color_red, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);
			}
#endif
			float fMag = vDistanceToIntersection.Mag();

			if(vDistanceToIntersection.z < 0.0f || (fMag < fDistToLineThreshold))
			{
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::SelectBestFrontSurfaceMaterial(CClimbHandHoldDetected& outHandHold, 
													const WorldProbe::CShapeTestHitPoint *pIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS],
													const WorldProbe::CShapeTestHitPoint *pStartIntersection)
{
	taskAssert(pStartIntersection);

	struct tMaterial
	{
		int nCount;
		phMaterialMgr::Id nId;
		const WorldProbe::CShapeTestHitPoint *pIntersection;
	};

	tMaterial Materials[CLIMB_DETECTOR_MAX_HORIZ_TESTS] = { 0 };
	tMaterial TempMaterial;

	for(int i = 0; i < CLIMB_DETECTOR_MAX_HORIZ_TESTS; ++i)
	{
		if(pIntersections[i])
		{
			static dev_float s_fMaxDistanceFromStart = 0.15f;
			Vector3 vDistanceToStart = pStartIntersection->GetHitPosition() - pIntersections[i]->GetHitPosition();
			float fXYMag = vDistanceToStart.XYMag2();
			if(fXYMag < square(s_fMaxDistanceFromStart))
			{
				//! Add to material count.
				for(int m = 0; m < CLIMB_DETECTOR_MAX_HORIZ_TESTS; ++m)
				{
					if(Materials[m].nCount == 0 || Materials[m].nId == pIntersections[i]->GetMaterialId())
					{
						Materials[m].nCount++;
						Materials[m].nId = pIntersections[i]->GetMaterialId();
						Materials[m].pIntersection = pIntersections[i];	

						if(m > 0 && Materials[m].nCount > Materials[0].nCount)
						{
							//! swap highest to front.
							TempMaterial = Materials[0];
							Materials[0] = Materials[m];
							Materials[m] = TempMaterial;
						}
						break;
					}
				}
			}
		}
	}
	
	//! If we have found multiple materials, use that, else fall back to original 'edge' material.
	if(Materials[0].nCount > 1)
	{
		outHandHold.SetFrontSurfaceMaterial(Materials[0].nId,g_CollisionAudioEntity.GetMaterialFromShapeTestSafe(Materials[0].pIntersection));
	}
	else
	{
		outHandHold.SetFrontSurfaceMaterial(pStartIntersection->GetHitMaterialId(),g_CollisionAudioEntity.GetMaterialFromShapeTestSafe(pStartIntersection));
	}

	return true;
}


////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::IsGoingUnderArch(const Matrix34& testMat, 
									  const CClimbHandHoldDetected& inHandHold, 
									  const float (&minIntesectionDistances)[CLIMB_DETECTOR_MAX_HORIZ_TESTS],
									  const WorldProbe::CShapeTestHitPoint *pIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS]) const
{
	if(m_flags.IsFlagSet(DF_AutoVault) && !m_flags.IsFlagSet(DF_JumpVaultTestOnly))
	{
		f32 fHeight = inHandHold.GetHandHold().GetPoint().GetZ() - GetPedGroundPosition(testMat).z;
		static dev_float s_fOpenSpaceHeight = 1.8f;
		if(fHeight > s_fOpenSpaceHeight)
		{
			static dev_u8 s_StartTest = 1;
			static dev_u8 s_EndTest = 4; 
			static dev_float s_fClearanceDistance = 1.25f;
			bool bIsOpen = true;
			for(u32 i = s_StartTest; i < s_EndTest; i++)
			{
				bool bIntersectionOk = true;
				const CEntity *pEntity = pIntersections[i] ? pIntersections[i]->GetHitEntity() : NULL;
				if(pEntity && pEntity->GetIsTypePed())
				{
					bIntersectionOk = false;
				}

				if(bIntersectionOk && (minIntesectionDistances[i] < s_fClearanceDistance))
				{
					bIsOpen = false;
					break;
				}
			}

			//! If we detected an open space, don't do vault checks.
			if(bIsOpen)
			{
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::CalculateClimbAngle(const Matrix34& testMat,
										 CClimbHandHoldDetected& inOutHandHold, 
										 const float (&minIntesectionDistances)[CLIMB_DETECTOR_MAX_HORIZ_TESTS],
										 const WorldProbe::CShapeTestHitPoint *pIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS])
{
	float fAngle = 0.0f;
	Vector3 vHandhold = inOutHandHold.GetHandHold().GetPoint();
	Vector3 vPedToHandHold(vHandhold);
	vPedToHandHold -= testMat.d;
	vPedToHandHold.z = 0.0f;

	//! Allow a small tolerance value to disregard high angles close to handhold.
	static dev_float s_fDistToHandHoldMin = 0.2f;
	static dev_float s_fDistToHandHoldMax = 0.3f;
	static dev_float s_fIntersectionToHandholdTreshold = 0.105f;

	u32 nNumLargeAngles = 0;
	TUNE_GROUP_FLOAT(VAULTING, fLargeAngle, 37.5f, 0.0f, 90.0, 1.0f);
	TUNE_GROUP_FLOAT(VAULTING, fCutoffAngle, 42.5f, 0.0f, 90.0, 1.0f);
	TUNE_GROUP_FLOAT(VAULTING, fIgnoreHeightFromGroundForAngleTest, 0.4f, 0.0f, 10.0, 0.01f);
	TUNE_GROUP_FLOAT(VAULTING, fDistanceFromHandHoldForAngledClimb, 0.45f, 0.0f, 10.0, 0.01f);

	if(vPedToHandHold.Mag2() > 0.0f)
	{
		float fDistXY = vPedToHandHold.XYMag();
		
		vPedToHandHold.NormalizeFast();

		if(fDistXY > fDistanceFromHandHoldForAngledClimb)
		{
			for(int i = 0; i < CLIMB_DETECTOR_MAX_HORIZ_TESTS; ++i)
			{
				if(pIntersections[i])
				{
					//! Check intersection happens before handhold. And that it's below it. After that, get the worst angle of all
					//! intersections. The intersection must also be a min distance away from handhold to count.
					float fIntersectionDist = minIntesectionDistances[i];

					//! Move point into direction of handhold (makes angle test a little easier).
					Vector3 vTestPoint = testMat.d + (vPedToHandHold * fIntersectionDist);
					vTestPoint.z = pIntersections[i]->GetHitPosition().z;
					Vector3 vIntersectionToHandHold(vHandhold);
					vIntersectionToHandHold -= vTestPoint;

					Vector3 vIntersectionToHandHold2D = vIntersectionToHandHold;
					vIntersectionToHandHold2D.z = 0.0f;
					float fIntersectionToHandholdDist = vIntersectionToHandHold2D.XYMag();

					if( (fIntersectionDist < fDistXY) && (fIntersectionToHandholdDist > s_fIntersectionToHandholdTreshold) )
					{
						//! Ensure it's not too close to handhold as this will give inaccurate angles. Note: Need if we haven't accepted 
						//! a test yet, then lower threshold.
						float fThreshold = i > 0 ? s_fDistToHandHoldMax : s_fDistToHandHoldMin;

						float fHeightDiff = (vHandhold.GetZ() - vTestPoint.z);

						float fHeightFromGround = vTestPoint.z - GetPedGroundPosition(testMat).z;

						if( (fHeightDiff > fThreshold) && (fHeightFromGround > fIgnoreHeightFromGroundForAngleTest) )
						{
							vIntersectionToHandHold.NormalizeFast();
							float fIntersectionAngle = abs(vIntersectionToHandHold.FastAngle(ZAXIS) * RtoD);
							if(fIntersectionAngle > fAngle)
							{
								fAngle = fIntersectionAngle;
							}
							if(fIntersectionAngle > fLargeAngle)
							{
								++nNumLargeAngles;
							}
						}
					}
				}
			}
		}
	}

	//! Due to inconsistencies in calculating the angle of the climb due to intersection positions relative to handhold, we reject the
	//! climb if we find multiple large angles.
	if( (nNumLargeAngles > 1) && (fAngle > fCutoffAngle))
	{
		return false;
	}

	inOutHandHold.SetClimbAngle(fAngle);
	return true;
}

void CClimbDetector::GetMatrix(Matrix34& m) const
{
	m = MAT34V_TO_MATRIX34(m_Ped.GetMatrix());

	if(GetPlayerPed())
	{
		CControl* pControl = GetPlayerPed() ? GetPlayerPed()->GetControlFromPlayer() : NULL;
		if(pControl)
		{
			if(IsQuadrupedTest())
			{
				//! Don't angle horse test upwards. It results in missing climb points when navigating small inclines. However,
				//! we want to angle test downwards to catch fences at the bottom of hills etc.
				if(m.b.z > 0.0f)
				{
					m.MakeRotateZ(m_Ped.GetCurrentHeading());
				}
			}
			else if(m_Ped.GetIsInCover())
			{
				// When in cover, we face either left or right, we need to test into the direction of cover for the climb tests
				CTaskInCover* pCoverTask = static_cast<CTaskInCover*>(m_Ped.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
				if (pCoverTask)
				{
					Vector3 vCoverDirection = pCoverTask->GetCoverDirection();
					float fHeading = rage::Atan2f(-vCoverDirection.x, vCoverDirection.y);
					m.MakeRotateZ(fHeading);
				}
			}
			else
			{
				f32 fHeadingDiff = 0.0f;
				Vector3 vStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm(), 0.0f);
				if(vStickInput.Mag2() > 0.f && !m_flags.IsFlagSet(DF_AutoVault) && !m_Ped.GetIsInWater())
				{
					float fCamOrient = camInterface::GetPlayerControlCamHeading(m_Ped);
					float fStickAngle = rage::Atan2f(-vStickInput.x, vStickInput.y);
					fStickAngle = fStickAngle + fCamOrient;
					fStickAngle = fwAngle::LimitRadianAngle(fStickAngle);
					fHeadingDiff  = fwAngle::LimitRadianAngle(fStickAngle - m_Ped.GetCurrentHeading());

					TUNE_GROUP_FLOAT(VAULTING, MAX_HEADING_DIFF, 30.0f, 0.0f, 90.0f, 1.0f);
					TUNE_GROUP_FLOAT(VAULTING, MIN_HEADING_DIFF, 15.0f, 0.0f, 90.0f, 1.0f);

#if FPS_MODE_SUPPORTED
					const bool bInFpsMode = m_Ped.IsFirstPersonShooterModeEnabledForPlayer(false, false, true);
					TUNE_GROUP_FLOAT(VAULTING, MAX_HEADING_DIFF_FPS, 100.0f, 0.0f, 180.0f, 1.0f);
#endif // FPS_MODE_SUPPORTED

					float fMaxHeading = (FPS_MODE_SUPPORTED_ONLY(bInFpsMode ? MAX_HEADING_DIFF_FPS :) MAX_HEADING_DIFF) * DtoR;
					float fMinHeading = MIN_HEADING_DIFF * DtoR;
					fHeadingDiff = Clamp(fHeadingDiff, -fMaxHeading, fMaxHeading);

					if(Abs(fHeadingDiff) <= fMinHeading)
					{
						fHeadingDiff = 0.0f;
					}
				}

				f32 fNewHeading = fwAngle::LimitRadianAngle(m_Ped.GetCurrentHeading() + fHeadingDiff);

				m.MakeRotateZ(fNewHeading);

				Vec3V v = m_Ped.GetTransform().GetPosition();
				m.MakeTranslate(RCC_VECTOR3(v));
			}
		}
	}

#if __BANK
	m.RotateZ(ms_fHeadingModifier);
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

Vector3 CClimbDetector::GetPedGroundPosition(const Matrix34& testMat) const 
{
	Vector3 vGroundPos = testMat.d;

	if(m_Ped.GetIsSwimming() && (m_Ped.m_Buoyancy.GetStatus() != NOT_IN_WATER) )
	{
		vGroundPos.z = m_Ped.m_Buoyancy.GetAbsWaterLevel();
		vGroundPos.z -= m_Ped.GetCapsuleInfo()->GetGroundToRootOffset();
	}
	else
	{
		Vector3 vActualGround = m_Ped.GetGroundPos();
		if(!m_Ped.IsNetworkClone() && vActualGround.z > PED_GROUNDPOS_RESET_Z)
		{
			vGroundPos = vActualGround;
		}
		else
		{
			vGroundPos.z -= m_Ped.GetCapsuleInfo()->GetGroundToRootOffset();
		}
	}

	return vGroundPos;
}

////////////////////////////////////////////////////////////////////////////////

CPed *CClimbDetector::GetPlayerPed() const
{
	if(IsQuadrupedTest())
	{
		CPed* pDriver = m_Ped.GetSeatManager() ? m_Ped.GetSeatManager()->GetDriver() : NULL;
		if (pDriver && pDriver->IsLocalPlayer())
		{
			return pDriver;
		}
	}
	else
	{
		return &m_Ped;
	}
	
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::IsQuadrupedTest() const
{
	return m_Ped.GetCapsuleInfo()->IsQuadruped();
}

////////////////////////////////////////////////////////////////////////////////

void CClimbDetector::ProcessAutoVaultPhysicalDelay(CPhysical *pPhysical) 
{	
	if(IsQuadrupedTest())
		return;

	if(!m_flags.IsFlagSet(DF_AutoVault))
		return;

	if(m_flags.IsFlagSet(VF_DisableAutoVaultDelay))
	{
		//! Reset.
		SetAutoVaultPhysicalDelay(NULL, 0);
		return;
	}
	
	if(pPhysical != m_pLastClimbPhysical && m_Ped.GetGroundPhysical() != pPhysical)
	{
		//! Reset.
		SetAutoVaultPhysicalDelay(NULL, 0);

		if(pPhysical)
		{
			if(pPhysical->GetIsTypeVehicle())
			{
				const CVehicle *pVehicle = static_cast<const CVehicle*>(pPhysical);

				if(!pVehicle->InheritsFromTrain())
				{
					TUNE_GROUP_INT(VAULTING, uAutoVaultVehicleDelay, 500, 0, 5000, 1); 
					SetAutoVaultPhysicalDelay(pPhysical, fwTimer::GetTimeInMilliseconds() + uAutoVaultVehicleDelay);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CClimbDetector::SetAutoVaultPhysicalDelay(CPhysical *pPhysical, u32 uDelayEndMs)
{
	m_pLastClimbPhysical = pPhysical;
	m_uAutoVaultPhysicalDelayEndTime = uDelayEndMs;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::IsDoingAutoVaultPhysicalDelay() const
{
	u32 nCurrentTime = fwTimer::GetTimeInMilliseconds();

	bool bDelaying = nCurrentTime < m_uAutoVaultPhysicalDelayEndTime;
	return bDelaying;
}

////////////////////////////////////////////////////////////////////////////////

void CClimbDetector::ProcessAutoVaultStairDelay()
{
	if(IsQuadrupedTest())
		return;

	if(!m_flags.IsFlagSet(DF_AutoVault))
		return;

	if(m_Ped.GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected) || 
		m_Ped.GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairs))
	{
		m_flags.SetFlag(DF_OnStairs);
		m_uLastOnStairsTime = fwTimer::GetTimeInMilliseconds();
	}

	static dev_u32 s_uOffStairsAutoVaultCooldown = 2000; 
	static dev_u32 s_uOffStairsAutoVaultDelay = 500;
	if(fwTimer::GetTimeInMilliseconds() < (m_uLastOnStairsTime+s_uOffStairsAutoVaultCooldown))
	{
		SetAutoVaultCoolownDelay(s_uOffStairsAutoVaultDelay, (m_uLastOnStairsTime+s_uOffStairsAutoVaultCooldown));
		m_uLastOnStairsTime = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////

void CClimbDetector::SetAutoVaultCoolownDelay(u32 uDelayMS, u32 uDelayEndMs)
{
	m_uAutoVaultCooldownDelayMs = uDelayMS;
	m_uAutoVaultCooldownEndTimeMs = uDelayEndMs;
}

////////////////////////////////////////////////////////////////////////////////

bool CClimbDetector::IsDoingAutoVaultCooldownDelay() const
{
	u32 nCurrentTime = fwTimer::GetTimeInMilliseconds();

	bool bInCooldown = nCurrentTime < m_uAutoVaultCooldownEndTimeMs;
	if(bInCooldown)
	{
		if(nCurrentTime < (m_uAutoVaultDetectedStartTimeMs + m_uAutoVaultCooldownDelayMs))
			return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CClimbDetector::DoBlockJumpTest(const Matrix34& testMat, const WorldProbe::CShapeTestHitPoint *pIntersections[CLIMB_DETECTOR_MAX_HORIZ_TESTS])
{
	static dev_float s_fTestHeightMin = 0.5f;
	static dev_float s_fTestHeightMax = 2.0f;
	static dev_float s_fTestDistOffset = 0.3f;
	static dev_float s_fRequiredIntersectionDistAbovePed = 0.2f;

	if(m_Ped.IsLocalPlayer() && !m_flags.IsFlagSet(DF_AutoVault))
	{
		const CBipedCapsuleInfo *pBipedCapsuleInfo = m_Ped.GetCapsuleInfo()->GetBipedCapsuleInfo();
		if(!pBipedCapsuleInfo)
		{
			return;
		}

		float fCloseDistance = (pBipedCapsuleInfo->GetRadiusRunning() * 0.5f) + s_fTestDistOffset;
		float ZMinCutoff = GetPedGroundPosition(testMat).z + s_fTestHeightMin;
		float ZMaxCutoff = GetPedGroundPosition(testMat).z + s_fTestHeightMax;

		float fRequiredIntersectionHeight = testMat.d.z + s_fRequiredIntersectionDistAbovePed; 

		int iNumCloseIntersections=0;
		bool bRequiredHeightHit = false;

		for(int i = 0; i < CLIMB_DETECTOR_MAX_HORIZ_TESTS; ++i)
		{
			if(pIntersections[i] && pIntersections[i]->IsAHit())
			{
				Vector3 vHitPosition = pIntersections[i]->GetHitPosition();

				if(vHitPosition.z > fRequiredIntersectionHeight)
				{
					bRequiredHeightHit = true;
				}

				if(vHitPosition.z > ZMinCutoff && vHitPosition.z < ZMaxCutoff)
				{
					Vector3 vDistance = vHitPosition - testMat.d;
					float fXYDist = vDistance.XYMag();
					if(fXYDist < fCloseDistance)
					{
						iNumCloseIntersections++;
					}
				}
			}
		}

		if(iNumCloseIntersections >= 2 && bRequiredHeightHit)
		{
			m_bBlockJump = true;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

#if __BANK
bool CClimbDetector::GetDebugEnabled() const
{
	if(ms_bDebugDraw)
	{
		return true;
	}

	return false;
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CClimbDetector::SetDebugEnabled(const bool bEnabled, const Color32& colour) const
{
	ms_bDebugDraw = bEnabled;
	ms_debugDrawColour = colour;
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////

#if __BANK
void CClimbDetector::AddDebugIntersections(const WorldProbe::CShapeTestHitPoint* pIntersections, const u32 uNumIntersections, const Color32 debugColour) const
{
	if(pIntersections)
	{
		for(u32 i = 0; i < uNumIntersections; i++)
		{
			if(pIntersections[i].IsAHit())
			{
				Color32 rotatedColour(debugColour.GetBlue(), debugColour.GetRed(), debugColour.GetGreen());

				static dev_float RADIUS = 0.0125f;
				GetDebugDrawStore().AddSphere(pIntersections[i].GetPosition(), RADIUS, debugColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);
				GetDebugDrawStore().AddLine(pIntersections[i].GetPosition(), pIntersections[i].GetPosition() + pIntersections[i].GetIntersectedPolyNormal() * ScalarVFromF32(0.1f), rotatedColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME);

				if(!GetIsValidIntersection(pIntersections[i]))
				{
					// Render something extra if not valid
					GetDebugDrawStore().AddSphere(pIntersections[i].GetPosition(), RADIUS * 2.0f, rotatedColour, CLIMB_DETECTOR_DEBUG_EXPIRY_TIME, 0, false);
				}
			}
		}
	}
}
#endif // __BANK

////////////////////////////////////////////////////////////////////////////////
