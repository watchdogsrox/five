// Title	:	TaskSeekCover.cpp
// Author	:	Phil Hooker
// Started	:	31/05/05

// This selection of class' enables individual peds to seek and hide in cover
// Rage Headers
#include "ai/task/taskchannel.h"
#include "fwanimation/animmanager.h"
#include "fwmaths/angle.h"

//Game headers.
#include "debug/debugscene.h"
#include "Peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Physics/GtaArchetype.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "Task\Combat\Cover\Cover.h"
#include "Task/Combat/Cover/coverfilter.h"
#include "Task\General\TaskBasic.h"
#include "Task/Combat/TaskCombat.h" // For common defines
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task\Combat\Cover\TaskSeekCover.h"
#include "Task/Combat/Cover/TaskCover.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task\General\TaskSecondary.h"
#include "vehicles/vehicle.h"
#include "weapons/Weapon.h"

//Rage headers
#include "Vector/Vector3.h"

AI_OPTIMISATIONS()
AI_COVER_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CCoverFinder
//////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CCoverFinder, CONFIGURED_FROM_FILE, 0.69f, atHashString("CCoverFinder",0x4128232e));

CCoverFinder::CCoverFinder()
: m_coverSearchDistributer(CExpensiveProcess::PPT_CoverFinderDistributer)
{
	m_coverSearchDistributer.RegisterSlot();
}

void CCoverFinder::Update()
{
	Assertf(m_coverSearchDistributer.IsRegistered(), "Cover search expensive process wasn't registered!");

	if(m_iRunningFlags.IsFlagSet(RF_ForceUpdateThisFrame) || m_coverSearchDistributer.ShouldBeProcessedThisFrame())
	{
		m_coverFinderFSM.Update();
	}
	else
	{
		m_coverFinderFSM.ValidateCoverPoints();
	}

	m_iRunningFlags.ClearFlag(RF_ForceUpdateThisFrame);
}

void CCoverFinder::SetupSearchParams(const CPed* pTargetPed, const Vector3& vTargetPos, const Vector3& vCoverSearchStartPos, s32 iFlags, float fMinDistance, 
					   float fMaxDistance, const s32 iSearchType, const s32 iSearchMode, const s32 iScriptedCoverIndex, CCover::CoverPointFilterConstructFn* pFn)
{
	if(iScriptedCoverIndex != INVALID_COVER_POINT_INDEX)
	{
		m_coverFinderFSM.SetCoverPointIndex(iScriptedCoverIndex);
	}

	m_coverFinderFSM.SetCoverStartSearchPos(vCoverSearchStartPos);
	m_coverFinderFSM.SetupSearchParams(pTargetPed, vTargetPos, iFlags, fMinDistance, fMaxDistance, iSearchType, pFn);
	m_coverFinderFSM.SetSearchMode(iSearchMode);
}

// --------------------------------------------------------
// Static functions for handling the pool init and shutdown
// --------------------------------------------------------

void CCoverFinder::Init(unsigned UNUSED_PARAM(initMode))
{
	//Init the pool
	CCoverFinder::InitPool(MEMBUCKET_GAMEPLAY);
}

void CCoverFinder::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
	//Shut down the pool
	Assert(CCoverFinder::GetPool()->GetNoOfUsedSpaces() == 0);
	CCoverFinder::ShutdownPool();
}

//////////////////////////////////////////////////////////////////////////
// CCoverFinderFSM
//////////////////////////////////////////////////////////////////////////

//------------
// Constructor
//------------
CCoverFinderFSM::CCoverFinderFSM()
: m_TargetPos(Vector3(0.0f, 0.0f, 0.0f)),
m_pTargetPed(NULL),
m_flags(0),
m_iCoverSearchMode(COVER_SEARCH_ANY),
m_iCoverSearchType(CCover::CS_PREFER_PROVIDE_COVER),
m_iScriptedCoverPointID(INVALID_COVER_POINT_INDEX),
m_vCoverStartSearchPos(Vector3(0.0f, 0.0f, 0.0f)),
m_pPed(NULL),
m_fTimeInState(0.0f),
m_iNumCoverPoints(0),
m_iCurrentCoverPointIndex(0),
m_iFallbackCoverPointIndex(0),
m_pCoverPointFilterConstructFn(NULL),
m_fMinDistance(0.0f),
m_fMaxDistance(20.0f),
m_bIsActive(false),
m_iLastSearchResult(SEARCH_INACTIVE)
{
	for( s32 i = 0; i < COVER_POINT_HISTORY_SIZE; i++ )
	{
		m_apCoverPointsAlreadyChecked[i] = NULL;
		m_apFoundCoverPoints[i] = NULL;
		m_aFailReasons[i] = FAIL_REASON_NONE;
	}

	m_RouteSearchHelper.ResetSearch();
	m_asyncProbeHelper.ResetProbe();
}

//-------------------------------------------------------------------------
// PURPOSE:	Destructor
// PARAMS:	None
//-------------------------------------------------------------------------
CCoverFinderFSM::~CCoverFinderFSM()
{}

void CCoverFinderFSM::SetupSearchParams(const CPed* pTargetPed, const Vector3& vTargetPos, s32 iFlags, float fMinDistance, float fMaxDistance, 
										const s32 iSearchType, CCover::CoverPointFilterConstructFn* pFn)
{
	m_pTargetPed = pTargetPed;
	m_TargetPos = vTargetPos;
	m_flags = iFlags;
	m_fMinDistance = fMinDistance;
	m_fMaxDistance = fMaxDistance;
	m_iCoverSearchType = iSearchType;
	m_pCoverPointFilterConstructFn = pFn;
}

void CCoverFinderFSM::StartSearch(CPed* pPed)
{
	m_bIsActive = true;
	m_pPed = pPed;
	//@@: location CCOVERFINDERFSM_STARTSEARCH_SET_ACTIVE
	m_iLastSearchResult = SEARCH_ACTIVE;
} 

s32 CCoverFinderFSM::GetLastSearchResult(bool bResetSearchResult)
{
	s32 iLastSearchResult = m_iLastSearchResult;
	if(bResetSearchResult && iLastSearchResult != SEARCH_ACTIVE)
	{
		m_iLastSearchResult = SEARCH_INACTIVE;
	}

	return iLastSearchResult;
}

void CCoverFinderFSM::SetCoverPointIndex(const s32 iCoverPointIndex)
{
	m_iScriptedCoverPointID = iCoverPointIndex;
	
	// Find the position of the cover point and use it to set the search start pos
	CCoverPoint* pCoverPoint = CCover::FindCoverPointWithIndex( iCoverPointIndex );
#if __ASSERT
	if( !pCoverPoint )
	{
		taskAssertf( 0, "Scripted cover point referenced in COVER_FINDER does not exist!" );
	}
#endif

	// Take a note of the cover point position to give us a back up search in case the cover point is removed
	if( pCoverPoint )
	{
		Vector3 vCoverPointPos;
		pCoverPoint->GetCoverPointPosition(vCoverPointPos);
		m_vCoverStartSearchPos = vCoverPointPos;
		m_iCoverSearchMode = COVER_SEARCH_BY_SCRIPTED_ID;
	}
	// The cover point is invalid, take a guess at a valid position
	else
	{
		m_vCoverStartSearchPos.Zero();
		m_iScriptedCoverPointID = INVALID_COVER_POINT_INDEX;
		m_iCoverSearchMode = COVER_SEARCH_ANY;
	}
}

//-------------------------------------------------------------------------
// Sets the minimum and maximum distances allowed to search for cover
//-------------------------------------------------------------------------
void CCoverFinderFSM::SetMinMaxSearchDistances( float fMinDistance, float fMaxDistance )
{
	m_fMinDistance = fMinDistance;
	m_fMaxDistance = fMaxDistance;
}


#define MAX_DISTANCE_FROM_TARGET_TO_COVER (40.0f)
#define MAX_DISTANCE_TO_ACCEPT_LONG_ROUTE (20.0f)


CCoverPoint* CCoverFinderFSM::SearchForCoverPoint(const Vector3& vThreatPos )
{
	if(m_iCurrentCoverPointIndex > 0)
	{
		for(; m_iCurrentCoverPointIndex < COVER_POINT_HISTORY_SIZE; m_iCurrentCoverPointIndex++)
		{
			if(m_apFoundCoverPoints[m_iCurrentCoverPointIndex])
			{
				return m_apFoundCoverPoints[m_iCurrentCoverPointIndex];
			}
		}
	}

	m_iCurrentCoverPointIndex = 0;

	// First release any previous cover points
	m_pPed->ReleaseDesiredCoverPoint();

	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
	// Search starting defaults to the player position unless a position is specified
	Vector3 vSearchStartPos = vPedPosition;
	if( m_iCoverSearchMode == COVER_SEARCH_BY_POS )
	{
		vSearchStartPos = m_vCoverStartSearchPos;
	}

	// If a cover point was specified at the start of the task
	if( m_iCoverSearchMode == COVER_SEARCH_BY_SCRIPTED_ID )
	{
		CCoverPoint* pCoverPoint = CCover::FindCoverPointWithIndex( m_iScriptedCoverPointID );
#if __ASSERT
		if( pCoverPoint == NULL )
		{
			taskAssertf( 0, "Scripted cover point used in task SEEK_COVER removed!" );
		}
#endif
		if( pCoverPoint && ( !pCoverPoint->IsOccupied() || pCoverPoint->m_pPedsUsingThisCover[0] == m_pPed ) )
		{
			return pCoverPoint;
		}
	}
	else
	{
		// If the ped has been assigned a defensive area, any coverpoint found must be within that area
		if( m_pPed->GetPedIntelligence()->GetDefensiveAreaForCoverSearch()->IsActive() )
		{
			Vector3 vDefensiveAreaCentre;
			float fMaxDist;
			m_pPed->GetPedIntelligence()->GetDefensiveAreaForCoverSearch()->GetCentre(vDefensiveAreaCentre);
			m_pPed->GetPedIntelligence()->GetDefensiveAreaForCoverSearch()->GetMaxRadius(fMaxDist);

			fwFlags32 iCoverSearchFlags =  m_flags.IsFlagSet(CTaskCover::CF_RequiresLosToTarget) ? CCover::SF_ForAttack | CCover::SF_CheckForTargetsInVehicle : CCover::SF_CheckForTargetsInVehicle;
			if(CCover::FindCoverPoints(m_pPed, vDefensiveAreaCentre, vThreatPos, iCoverSearchFlags, m_apFoundCoverPoints, COVER_POINT_HISTORY_SIZE, m_apCoverPointsAlreadyChecked, m_iNumCoverPoints, (CCover::eCoverSearchType) m_iCoverSearchType, 0.0f, fMaxDist, CCoverPointFilterDefensiveArea::Construct, (void*) m_pPed, m_pTargetPed))
			{
				return m_apFoundCoverPoints[m_iCurrentCoverPointIndex];
			}
		}
		// IF trying to find a cover point away from the target, search using an arc
		else 
		{
			if( m_flags.IsFlagSet(CTaskCover::CF_SearchInAnArcAwayFromTarget) )
			{
				Vector3 vCoverPos;
				Vector3 vTargetToPed = vPedPosition - vThreatPos;
				const float fDistance = vTargetToPed.Mag();
				vTargetToPed.NormalizeSafe();
				if(CCover::FindCoverPointInArc(m_pPed, &vCoverPos, vSearchStartPos, vTargetToPed, HALF_PI, rage::Max(m_fMinDistance - fDistance,0.0f), m_fMaxDistance, CCover::SF_CheckForTargetsInVehicle, vThreatPos, m_apFoundCoverPoints, COVER_POINT_HISTORY_SIZE, (CCover::eCoverSearchType) m_iCoverSearchType, m_apCoverPointsAlreadyChecked, m_iNumCoverPoints, m_pCoverPointFilterConstructFn, m_pPed, m_pTargetPed ))
				{
					return m_apFoundCoverPoints[m_iCurrentCoverPointIndex];
				}
			}
			else
			{
				fwFlags32 iCoverSearchFlags =  m_flags.IsFlagSet(CTaskCover::CF_RequiresLosToTarget) ? CCover::SF_ForAttack | CCover::SF_CheckForTargetsInVehicle : CCover::SF_CheckForTargetsInVehicle;
				if(CCover::FindCoverPoints(m_pPed, vSearchStartPos, vThreatPos, iCoverSearchFlags, m_apFoundCoverPoints, COVER_POINT_HISTORY_SIZE, m_apCoverPointsAlreadyChecked, m_iNumCoverPoints, (CCover::eCoverSearchType) m_iCoverSearchType, m_fMinDistance, m_fMaxDistance, CCoverPointFilterBase::Construct, (void*) m_pPed, m_pTargetPed))
				{
					return m_apFoundCoverPoints[m_iCurrentCoverPointIndex];
				}
			}
		}
	}

	return NULL;
}

dev_float COVER_DIVE_SHORT_DISTANCE_PLAYER = (4.0f);
dev_float COVER_DIVE_SHORT_DISTANCE_AI = (2.5f);

#define CLIP_PHASE_TO_BEGIN_TURNING (0.75f)

#define CROUCHING_CHANCE 0.33f

//static float DIVE_SHORT_DISTANCE =  0.0f;

void CCoverFinderFSM::CoverRejected(Vector3& DEV_ONLY(vCoverCoors), const char* DEV_ONLY(szReason) )
{
#if __DEV
	if(CPedDebugVisualiserMenu::ms_menuFlags.m_bDisplayCoverSearch &&
		(!CPedDebugVisualiserMenu::IsDisplayingOnlyForFocus() || m_pPed == CDebugScene::FocusEntities_Get(0) ))
	{
		Displayf("Coverpoint rejected, Ped(%p) rejected (%.2f,%.2f,%.2f) becuase %s", m_pPed.Get(), vCoverCoors.x, vCoverCoors.y, vCoverCoors.z, szReason );
	}
#endif
}

dev_float MAX_TIME_TO_SEARCH = 2.0f;

// A couple of checks that get called in a few different states, trying to cut down on code
bool CCoverFinderFSM::ShouldSearchForCover()
{
	// Don't search for longer than a set amount of time
	if( m_fTimeInState > MAX_TIME_TO_SEARCH )
	{
		return true;
	}

	// If our cover point has been removed, find a new cover point
	if( !IsDesiredCoverPointValid() )
	{
		return true;
	}

	// Make sure the path finding handle is valid
	if(m_pPed->GetDesiredCoverPoint()->GetStatus() & CCoverPoint::COVSTATUS_PositionBlocked)
	{
		return true;
	}

	return false;
}

// Check a couple of condition to verify if our desired cover is valid
bool CCoverFinderFSM::IsDesiredCoverPointValid()
{
	CCoverPoint* pDesiredCoverPoint = m_pPed->GetDesiredCoverPoint();

	if(!pDesiredCoverPoint)
	{
		return false;
	}

	if(!pDesiredCoverPoint->CanAccomodateAnotherPed() || pDesiredCoverPoint->IsDangerous() || pDesiredCoverPoint->IsCloseToPlayer(*m_pPed))
	{
		if(m_iCoverSearchMode == COVER_SEARCH_BY_SCRIPTED_ID && pDesiredCoverPoint->m_pPedsUsingThisCover[0] == m_pPed)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	return true;
}

void CCoverFinderFSM::ValidateCoverPoints()
{
	for( s32 i = 0; i < COVER_POINT_HISTORY_SIZE; i++ )
	{
		if(m_apFoundCoverPoints[i] && m_apFoundCoverPoints[i]->GetType() == CCoverPoint::COVTYPE_NONE)
		{
			m_apFoundCoverPoints[i] = NULL;
			m_aFailReasons[i] = FAIL_REASON_NONE;
		}
	}
}

CTaskHelperFSM::FSM_Return CCoverFinderFSM::ProcessPreFSM()
{
	if(!m_pPed)
	{
		// This will set the finder to not be active. The states check for being active and send the helper back to waiting if inactive
		Reset(SEARCH_INACTIVE, false);
	}
	else
	{
		m_pPed->SetPedResetFlag( CPED_RESET_FLAG_SearchingForCover, true );
		m_pPed->SetPedResetFlag( CPED_RESET_FLAG_KeepCoverPoint, true );

		m_fTimeInState += fwTimer::GetTimeStep();
	}

	ValidateCoverPoints();

	return FSM_Continue;
}

CTaskHelperFSM::FSM_Return CCoverFinderFSM::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		
		// Waiting to be activated
		FSM_State(State_Waiting)
			FSM_OnUpdate
				return Waiting_OnUpdate();

		// Search for a nearby cover point that provides cover from the threat direction
		FSM_State(State_SearchForCover)
			FSM_OnEnter
				SearchForCover_OnEnter();
			FSM_OnUpdate
				return SearchForCover_OnUpdate();

		// Probe around the object (if needed) to make sure the cover point is accessible
		FSM_State(State_CheckCoverPointAccessibility)
			FSM_OnEnter
				CheckCoverPointAccessibility_OnEnter();
			FSM_OnUpdate
				return CheckCoverPointAccessibility_OnUpdate();
			FSM_OnExit
				CheckCoverPointAccessibility_OnExit();

		// Check the blocked status from the tactical analysis
		FSM_State(State_CheckCachedCoverPointBlockedStatus)
			FSM_OnUpdate
				return CheckCachedCoverPointBlockedStatus_OnUpdate();

		FSM_State(State_CheckCoverPointBlockedStatus)
			FSM_OnEnter
				CheckCoverPointBlockedStatus_OnEnter();
			FSM_OnUpdate
				return CheckCoverPointBlockedStatus_OnUpdate();

		// Check the LOS from the cover vantage point to see if they can see the target
		FSM_State(State_CheckTacticalAnalysisLos)
			FSM_OnUpdate
				return CheckTacticalAnalysisInfo_OnUpdate(false);

		// Check the LOS from the cover vantage point to see if they can see the target
		FSM_State(State_CheckLosToCoverPoint)
			FSM_OnEnter
				CheckLosToCoverPoint_OnEnter();
			FSM_OnUpdate
				return CheckLosToCoverPoint_OnUpdate();
			FSM_OnExit
				CheckLosToCoverPoint_OnExit();

		// Check that there is a reasonable route from the peds current position to the coverpoint.
		// i.e. make sure a route is available and it isn't too long
		FSM_State(State_CheckRouteToCoverPoint)
			FSM_OnEnter
				CheckRouteToCoverPoint_OnEnter();
			FSM_OnUpdate
				return CheckRouteToCoverPoint_OnUpdate();			
			FSM_OnExit
				CheckRouteToCoverPoint_OnExit();

		// The search for cover failed, this state will make a decision as to how the task should terminate
		FSM_State(State_CoverSearchFailed)
			FSM_OnUpdate
				return CoverSearchFailed_OnUpdate();
		
	FSM_End
}

// This is where we will reset our variables when we go inactive
void CCoverFinderFSM::Reset(s32 iSearchResult, bool bResetState)
{
	for( s32 i = 0; i < COVER_POINT_HISTORY_SIZE; i++ )
	{
		m_apCoverPointsAlreadyChecked[i] = NULL;
		m_apFoundCoverPoints[i] = NULL;
		m_aFailReasons[i] = FAIL_REASON_NONE;
	}

	m_RouteSearchHelper.ResetSearch();
	m_asyncProbeHelper.ResetProbe();

	m_TargetPos = Vector3(0.0f, 0.0f, 0.0f);
	m_pTargetPed = NULL;
	m_flags = 0;
	m_iCoverSearchMode = COVER_SEARCH_ANY;
	m_iCoverSearchType = CCover::CS_PREFER_PROVIDE_COVER;
	m_iScriptedCoverPointID = INVALID_COVER_POINT_INDEX;
	m_vCoverStartSearchPos = Vector3(0.0f, 0.0f, 0.0f);
	m_pPed = NULL;
	m_fTimeInState = 0.0f;
	m_iNumCoverPoints = 0;
	m_iCurrentCoverPointIndex = 0;
	m_iFallbackCoverPointIndex = 0;
	m_pCoverPointFilterConstructFn = NULL;
	m_fMinDistance = 0.0f;
	m_fMaxDistance = 20.0f;
	m_bIsActive = false;
	m_iLastSearchResult = iSearchResult;

	if(bResetState)
	{
		SetState(State_Waiting);
	}
}

CTaskHelperFSM::FSM_Return CCoverFinderFSM::Waiting_OnUpdate()
{
	if(m_bIsActive)
	{
		SetState(State_SearchForCover);
	}

	return FSM_Continue;
}

void CCoverFinderFSM::SearchForCover_OnEnter()
{
	m_fTimeInState = 0.0f;
	m_pPed->ReleaseDesiredCoverPoint();
}

CTaskHelperFSM::FSM_Return CCoverFinderFSM::SearchForCover_OnUpdate()
{
	// If we are no longer active we need to make sure we are in the waiting state
	if( !m_bIsActive )
	{
		Reset(SEARCH_INACTIVE, true);
		return FSM_Continue;
	}

	// Don't search for longer than a set amount of time
	if( m_fTimeInState > MAX_TIME_TO_SEARCH )
	{
		SetState(State_CoverSearchFailed);
		return FSM_Continue;
	}

	// If there is no space left to cache failed cover points, quit
	if( m_iNumCoverPoints >= COVER_POINT_HISTORY_SIZE )
	{
		SetState(State_CoverSearchFailed);
		return FSM_Continue;
	}

	// Try and retrieve a valid cover point
	Vector3 vThreatPosition = GetThreatPosition();
	CCoverPoint* pCurrentCoverPoint = SearchForCoverPoint( vThreatPosition );
	if( pCurrentCoverPoint )
	{
		// Reset the fall back cover point because we found new valid points
		m_iFallbackCoverPointIndex = 0;

		// Set our new desired cover point and increment which cover point index we'll text next
		m_pPed->SetDesiredCoverPoint(pCurrentCoverPoint);
		m_iCurrentCoverPointIndex++;

		// Add this cover point to an exclusion list so we don't check it again
		if( m_iNumCoverPoints < COVER_POINT_HISTORY_SIZE )
		{
			m_apCoverPointsAlreadyChecked[m_iNumCoverPoints++] = m_pPed->GetDesiredCoverPoint();
		}

		if( EvaluateCoverPoint( vThreatPosition ) )
		{
			// If this point is an object, make sure the point is accessible
			if( ( pCurrentCoverPoint->GetType() == CCoverPoint::COVTYPE_OBJECT ||
				pCurrentCoverPoint->GetType() == CCoverPoint::COVTYPE_BIG_OBJECT ||
				pCurrentCoverPoint->GetType() == CCoverPoint::COVTYPE_VEHICLE )
				&& pCurrentCoverPoint->m_pEntity )
			{
				SetState(State_CheckCoverPointAccessibility);
			}
			else
			{
				SetState(State_CheckCachedCoverPointBlockedStatus);
			}

			return FSM_Continue;
		}
		else
		{
			// NULL out this cover point if it failed to pass the evaluation
			aiAssertf(m_iCurrentCoverPointIndex > 0 && m_iCurrentCoverPointIndex <= COVER_POINT_HISTORY_SIZE, "Trying to clear an out of range index (%d) in the cover point array!", m_iCurrentCoverPointIndex);
			m_apFoundCoverPoints[m_iCurrentCoverPointIndex - 1] = NULL;
			m_aFailReasons[m_iCurrentCoverPointIndex - 1] = FAIL_REASON_NONE;

			m_pPed->ReleaseDesiredCoverPoint();
		}
	}
	else
	{
		SetState(State_CoverSearchFailed);
	}

	return FSM_Continue;
}

bool CCoverFinderFSM::BuildAccessibilityCapsuleDesc(WorldProbe::CShapeTestCapsuleDesc &capsuleDesc, bool bIsSynchronous)
{
	Vector3 vThreatPosition = GetThreatPosition();
	Vector3 vCoverPosition;
	if(GetCoverPointPosition(vThreatPosition, vCoverPosition, NULL))
	{
		CEntity* pCoverEntity = m_pPed->GetDesiredCoverPoint()->m_pEntity;
		const Vector3 vCoverEntityPos = VEC3V_TO_VECTOR3(pCoverEntity->GetTransform().GetPosition());
		Vector3 vEnd = vCoverPosition - vCoverEntityPos;
		vEnd.z = 0.0f;
		float fDistance = vEnd.Mag();
		vEnd.Normalize();
		vEnd.Scale(fDistance + 0.2f);
		Vector3 vStart = vCoverEntityPos;
		vStart.z = vCoverPosition.z;
		vEnd += vStart;

		if(bIsSynchronous)
		{
			INC_PEDAI_LOS_COUNTER;
		}
		else
		{
			capsuleDesc.SetIsDirected(true);
			capsuleDesc.SetResultsStructure(&m_CoverAccessibilityResults);
		}

		capsuleDesc.SetCapsule(vStart, vEnd, 0.3f);
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
		capsuleDesc.SetExcludeEntity(pCoverEntity);

		return true;
	}

	return false;
}

void CCoverFinderFSM::CheckCoverPointAccessibility_OnEnter()
{
	m_CoverAccessibilityResults.Reset();

	CCoverPoint* pDesiredCover = m_pPed->GetDesiredCoverPoint();
	if( !taskVerifyf( pDesiredCover, "Cover point expected" ) )
	{
		return;
	}

	CEntity* pCoverEntity = pDesiredCover->m_pEntity;
	if(!pCoverEntity)
	{
		return;
	}

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	if(!BuildAccessibilityCapsuleDesc(capsuleDesc, false))
	{
		return;
	}

	WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

	m_fTimeInState = 0.0f;
}

CTaskHelperFSM::FSM_Return CCoverFinderFSM::CheckCoverPointAccessibility_OnUpdate()
{
	// If we are no longer active we need to make sure we are in the waiting state
	if( !m_bIsActive )
	{
		Reset(SEARCH_INACTIVE, true);
		return FSM_Continue;
	}

	// Check if we should return to search for cover due to some fail case
	if( ShouldSearchForCover() )
	{
		SetState(State_SearchForCover);
		return FSM_Continue;
	}

	bool bShapeTestPassed = false;

	// If the probe failed to start, do a synchronous probe test
	if( m_CoverAccessibilityResults.GetResultsStatus() == WorldProbe::TEST_NOT_SUBMITTED )
	{
		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		if(BuildAccessibilityCapsuleDesc(capsuleDesc, true))
		{
			if( !WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc) )
			{
				bShapeTestPassed = true;
			}
		}
	}
	else if( m_CoverAccessibilityResults.GetResultsReady() )
	{
		// If there is a clear lOS, check the route next
		if( m_CoverAccessibilityResults.GetNumHits() == 0 )
		{
			bShapeTestPassed = true;
		}
	}
	else
	{
		// If we did a synchronous test or our async results are not ready then we return here.
		return FSM_Continue;
	}

	// We have shape test results so change state accordingly
	if(bShapeTestPassed)
	{
		SetState(State_CheckCachedCoverPointBlockedStatus);
	}
	else
	{
		SetState(State_SearchForCover);
	}

	return FSM_Continue;
}

void CCoverFinderFSM::CheckCoverPointAccessibility_OnExit()
{
	m_CoverAccessibilityResults.Reset();
}


void CCoverFinderFSM::CheckCoverPointBlockedStatus_OnEnter()
{
	m_CoverStatusHelper.Reset();

	if( !taskVerifyf(m_pPed->GetDesiredCoverPoint(), "Cover point expected") )
	{
		return;
	}

	m_CoverStatusHelper.Start(m_pPed->GetDesiredCoverPoint());
}

CTaskHelperFSM::FSM_Return CCoverFinderFSM::CheckCoverPointBlockedStatus_OnUpdate()
{
	// If we are no longer active we need to make sure we are in the waiting state
	if(!m_bIsActive)
	{
		Reset(SEARCH_INACTIVE, true);
		return FSM_Continue;
	}

	// Update the cover status helper
	m_CoverStatusHelper.Update();

	// If our helper never started OR If our cover point has been removed, find a new cover point
	if( !m_CoverStatusHelper.IsActive() ||
		!IsDesiredCoverPointValid() )
	{
		SetState(State_SearchForCover);
		return FSM_Continue;
	}

	if(m_CoverStatusHelper.IsFinished())
	{
		// If failed or the cover point isn't clear then search for a new point
		if(m_CoverStatusHelper.GetFailed() || (m_CoverStatusHelper.GetStatus() & CCoverPoint::COVSTATUS_Clear) == 0)
		{
			SetState(State_SearchForCover);
		}
		else
		{
			SetState(m_flags.IsFlagSet(CTaskCover::CF_RequiresLosToTarget) ? State_CheckTacticalAnalysisLos : State_CheckRouteToCoverPoint);
		}
	}

	return FSM_Continue;
}

CTaskHelperFSM::FSM_Return CCoverFinderFSM::CheckCachedCoverPointBlockedStatus_OnUpdate()
{
	// Check if we have a cached cover point status
	CCoverPoint* pCoverPoint = m_pPed->GetDesiredCoverPoint();
	if(pCoverPoint && pCoverPoint->GetStatus())
	{
		if(pCoverPoint->GetStatus() & CCoverPoint::COVSTATUS_PositionBlocked)
		{
			SetState(State_SearchForCover);
			return FSM_Continue;
		}

		if(pCoverPoint->GetStatus() & CCoverPoint::COVSTATUS_Clear)
		{
			SetState(m_flags.IsFlagSet(CTaskCover::CF_RequiresLosToTarget) ? State_CheckTacticalAnalysisLos : State_CheckRouteToCoverPoint);
			return FSM_Continue;
		}
	}

	return CheckTacticalAnalysisInfo_OnUpdate(true);
}

CTaskHelperFSM::FSM_Return CCoverFinderFSM::CheckTacticalAnalysisInfo_OnUpdate(bool bIsBlockedCheck)
{
	// If we are no longer active we need to make sure we are in the waiting state
	if( !m_bIsActive )
	{
		Reset(SEARCH_INACTIVE, true);
		return FSM_Continue;
	}

	// If our cover point has been removed, find a new cover point
	if( !IsDesiredCoverPointValid() )
	{
		SetState(State_SearchForCover);
		return FSM_Continue;
	}

	CTacticalAnalysis* pTacticalAnalysis = CTacticalAnalysis::Find(*m_pTargetPed.Get());
	if(pTacticalAnalysis)
	{
		//Grab the cover points.
		const CTacticalAnalysisCoverPoints& rCoverPoints = pTacticalAnalysis->GetCoverPoints();
		for(int i = 0; i < rCoverPoints.GetNumPoints(); ++i)
		{
			// See if we've tested LOS against our found cover node
			const CTacticalAnalysisCoverPoint& rPoint = rCoverPoints.GetPointForIndex(i);
			if(rPoint.GetCoverPoint() == m_pPed->GetDesiredCoverPoint())
			{
				if(!bIsBlockedCheck)
				{
					if(rPoint.IsLineOfSightBlocked())
					{
						SetState(State_SearchForCover);
						return FSM_Continue;
					}
					
					if(rPoint.IsLineOfSightClear())
					{
						SetState(State_CheckRouteToCoverPoint);
						return FSM_Continue;
					}
				}
				else
				{
					if(rPoint.GetStatus() & CCoverPoint::COVSTATUS_PositionBlocked)
					{
						SetState(State_SearchForCover);
						return FSM_Continue;
					}
					
					if(rPoint.GetStatus() & CCoverPoint::COVSTATUS_Clear)
					{
						SetState(m_flags.IsFlagSet(CTaskCover::CF_RequiresLosToTarget) ? State_CheckTacticalAnalysisLos : State_CheckRouteToCoverPoint);
						return FSM_Continue;
					}
				}

				break;
			}
		}
	}

	SetState(bIsBlockedCheck ? State_CheckCoverPointBlockedStatus : State_CheckLosToCoverPoint);
	return FSM_Continue;
}

void CCoverFinderFSM::CheckLosToCoverPoint_OnEnter()
{
	if( !taskVerifyf( m_pPed->GetDesiredCoverPoint(), "Cover point expected" ) )
	{
		return;
	}

	Vector3 vThreatPosition = GetThreatPosition();
	Vector3 vThreatVantagePosition = GetThreatVantagePosition();
	Vector3 vCoverPosition;
	Vector3 vVantagePosition;
	if(!GetCoverPointPosition(vThreatPosition, vCoverPosition, &vVantagePosition))
	{
		return;
	}

	// Wait for the los result
	WorldProbe::CShapeTestProbeDesc probeData;
	probeData.SetStartAndEnd(vVantagePosition,vThreatVantagePosition);
	probeData.SetContext(WorldProbe::ELosCombatAI);
	probeData.SetExcludeEntity(m_pTargetPed ? m_pTargetPed->GetVehiclePedInside() : NULL);
	probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
	probeData.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU); 
	m_asyncProbeHelper.StartTestLineOfSight(probeData);

	m_fTimeInState = 0.0f;
}


CTaskHelperFSM::FSM_Return CCoverFinderFSM::CheckLosToCoverPoint_OnUpdate()
{
	// If we are no longer active we need to make sure we are in the waiting state
	if( !m_bIsActive )
	{
		Reset(SEARCH_INACTIVE, true);
		return FSM_Continue;
	}

	// Check if we should return to search for cover due to some fail case
	if( ShouldSearchForCover() )
	{
		SetState(State_SearchForCover);
		return FSM_Continue;
	}

	Vector3 vThreatPosition = GetThreatPosition();
	Vector3 vThreatVantagePosition = GetThreatVantagePosition();
	Vector3 vCoverPosition;
	Vector3 vVantagePosition;
	if(!GetCoverPointPosition(vThreatPosition, vCoverPosition, &vVantagePosition))
	{
		SetState(State_SearchForCover);
		return FSM_Continue;
	}

	// If the probe failed to start, do a synchronous probe test
	if( !m_asyncProbeHelper.IsProbeActive() )
	{
		WorldProbe::CShapeTestProbeDesc probeDesc2;
		probeDesc2.SetStartAndEnd(vVantagePosition, vThreatVantagePosition);
		probeDesc2.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE);
		probeDesc2.SetExcludeEntity(m_pTargetPed ? m_pTargetPed->GetVehiclePedInside() : NULL);
		probeDesc2.SetOptions(WorldProbe::LOS_IGNORE_SEE_THRU);
		probeDesc2.SetContext(WorldProbe::LOS_SeekCoverAI);
		if( !WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc2) )
		{
			SetState(State_CheckRouteToCoverPoint);
		}
		else
		{
			SetState(State_SearchForCover);
		}
		return FSM_Continue;
	}
	
	// If we're waiting on a probe search, wait for the result
	ProbeStatus probeStatus;
	if( m_asyncProbeHelper.GetProbeResults(probeStatus) )
	{
		// If there is a clear lOS, check the route next
		if( probeStatus == PS_Clear )
		{
			SetState(State_CheckRouteToCoverPoint);
			return FSM_Continue;
		}
		// No LOS, continue searching for a new cover point
		else
		{
			SetState(State_SearchForCover);
			return FSM_Continue;
		}
	}
	return FSM_Continue;
}


void CCoverFinderFSM::CheckLosToCoverPoint_OnExit()
{
	m_asyncProbeHelper.ResetProbe();
}


void CCoverFinderFSM::CheckRouteToCoverPoint_OnEnter()
{
	if( !taskVerifyf( m_pPed->GetDesiredCoverPoint(), "Coverpoint expected" ) )
	{
		return;
	}
	Vector3 vThreatPosition = GetThreatPosition();
	Vector3 vCoverPosition;
	if( GetCoverPointPosition(vThreatPosition, vCoverPosition, NULL) )
	{
		u64 iPathFlags = m_pPed->GetPedIntelligence()->GetNavCapabilities().BuildPathStyleFlags();
		
		iPathFlags |= (m_flags & CTaskCover::CF_IgnoreNonSignificantObjects) ?
			PATH_FLAG_IGNORE_NON_SIGNIFICANT_OBJECTS : PATH_FLAG_REDUCE_OBJECT_BBOXES;

		if(m_pTargetPed && (m_pPed == CNearestPedInCombatHelper::Find(*m_pTargetPed)))
		{
			iPathFlags |= PATH_FLAG_NEVER_CLIMB_OVER_STUFF;
			iPathFlags |= PATH_FLAG_NEVER_USE_LADDERS;
		}

		iPathFlags |= PATH_FLAG_COVERFINDER;

		TInfluenceSphere InfluenceSphere[1];
		InfluenceSphere[0].Init(vThreatPosition, CTaskCombat::ms_Tunables.m_TargetInfluenceSphereRadius, 1.0f*INFLUENCE_SPHERE_MULTIPLIER, 0.5f*INFLUENCE_SPHERE_MULTIPLIER);

		m_RouteSearchHelper.StartSearch( m_pPed, VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()), vCoverPosition, 0.0f, iPathFlags, 1, InfluenceSphere );
	}

	m_pCoverPointBeingTested = m_pPed->GetDesiredCoverPoint();
	m_fTimeInState = 0.0f;
}

CTaskHelperFSM::FSM_Return CCoverFinderFSM::CheckRouteToCoverPoint_OnUpdate()
{
	// If we are no longer active we need to make sure we are in the waiting state
	if( !m_bIsActive )
	{
		Reset(SEARCH_INACTIVE, true);
		return FSM_Continue;
	}

	// Check if we should return to search for cover due to some fail case
	if( ShouldSearchForCover() )
	{
		SetState(State_SearchForCover);
		return FSM_Continue;
	}

	// If we are already examining a cover point, check the path finding result
	if( m_RouteSearchHelper.IsSearchActive() )
	{
		float fDistance = 0.0f;
		Vector3 vLastPoint(0.0f, 0.0f, 0.0f);
		Vector3 pRoutePoints[MAX_NUM_PATH_POINTS];
		TNavMeshWaypointFlag pWptFlags[MAX_NUM_PATH_POINTS];
		s32 iMaxNumPts = MAX_NUM_PATH_POINTS;
		SearchStatus eSearchStatus;

		if(m_RouteSearchHelper.GetSearchResults( eSearchStatus, fDistance, vLastPoint, pRoutePoints, &iMaxNumPts, pWptFlags ))
		{
			if( eSearchStatus == SS_SearchSuccessful )
			{
				bool bInRange = false;
				// Check if this distance is considered in range depending on the search criteria
				switch( m_iCoverSearchMode )
				{
					// Specific cover points are always considered in range
				case COVER_SEARCH_BY_SCRIPTED_ID:
					bInRange = true;
					break;
					// Any cover point must be within a set distance from the ped
				case COVER_SEARCH_ANY:
					bInRange = (fDistance <= MAX_DISTANCE_TO_ACCEPT_LONG_ROUTE) || (fDistance <= ( VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()).Dist( vLastPoint ) * 3.0f ));
					break;
					// The cover point must be within a range of search point taking into account the distance the ped
					// must first go to reach the point
				case COVER_SEARCH_BY_POS:
					bInRange =  (fDistance <= MAX_DISTANCE_TO_ACCEPT_LONG_ROUTE) || (fDistance <= ( VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()).Dist( vLastPoint ) * 3.0f ));
					break;

				default:
					taskAssertf( 0, "Unknown cover search type requested");
					break;
				}

				// Inside range, so return true signifying a valid cover point has been found
				if( bInRange )
				{
					Vector3 vCoverPosition;
					Vector3 vThreatPosition = GetThreatPosition();
					if(GetCoverPointPosition( vThreatPosition, vCoverPosition, NULL ))
					{
						static const float fXYDistSqr = (1.5f*1.5f);
						const Vector3 vCoverDiff = vLastPoint - vCoverPosition;

						// Check the height between the final point and coverpoint
						if( ABS(vLastPoint.z - vCoverPosition.z) < 1.5f && vCoverDiff.XYMag2() <= fXYDistSqr )
						{
							// When checking the distance we allow the ped to move 1m closer than current
							Vec3V vecThreatPosition = RCC_VEC3V(vThreatPosition);
							Vec3V vPedToThreat = m_pPed->GetTransform().GetPosition() - vecThreatPosition;
							ScalarV scDistToThreatSq = square(Max(ScalarV(V_ZERO), Mag(vPedToThreat) - ScalarV(V_ONE)));
							ScalarV scMinDistSq = ScalarVFromF32(rage::square(CTaskCombat::ms_Tunables.m_TargetMinDistanceToRoute));
							if( IsLessThanAll(scDistToThreatSq, scMinDistSq ) )
							{
								scMinDistSq = scDistToThreatSq;
							}

							bool bFoundNavLink = false;
							bool bFoundDangerousNavLink = false;

							for(int i = 0; i < iMaxNumPts - 1; i++)
							{
								Vec3V vCurrentRoutePoint = RCC_VEC3V(pRoutePoints[i]);
								Vec3V vVec = RCC_VEC3V(pRoutePoints[i+1]) - vCurrentRoutePoint;

								// If we have a route link
								if( !bFoundDangerousNavLink &&
									(pWptFlags[i].IsClimb() || pWptFlags[i].IsDrop() || pWptFlags[i].IsLadderClimb()) )
								{
									bFoundNavLink = true;

									// First make sure the link isn't too close to the target
									ScalarV scTargetToRoutePointDistSq = DistSquared(vCurrentRoutePoint, vecThreatPosition);
									ScalarV scMinDistFromTargetToLink = ScalarVFromF32(CTaskCombat::ms_Tunables.m_TargetMinDistanceToAwayFacingNavLink);
									scMinDistFromTargetToLink = (scMinDistFromTargetToLink * scMinDistFromTargetToLink);
									if(IsLessThanAll(scTargetToRoutePointDistSq, scMinDistFromTargetToLink))
									{
										// If we are too close we need to make sure it's not facing away from the target
										Vec3V vTargetToRoutePoint = vCurrentRoutePoint - vecThreatPosition;
										Vec3V vLinkDir(-rage::Sinf(pWptFlags[i].GetHeading()), rage::Cosf(pWptFlags[i].GetHeading()), 0.0f);
										ScalarV scDot = Dot(vTargetToRoutePoint, vLinkDir);
										if(IsGreaterThanAll(scDot, ScalarV(V_ZERO)))
										{
											bFoundDangerousNavLink = true;
										}
									}
								}

								ScalarV T = geomTValues::FindTValueSegToPoint(vCurrentRoutePoint, vVec, vecThreatPosition);
								Vec3V vPos = vCurrentRoutePoint + Scale(vVec, T);
								Vec3V vDiff = vPos - vecThreatPosition;
								if( (MagSquared(vDiff) < scMinDistSq).Getb() )
								{
									CoverRejected(vCoverPosition, "Route came too close to target");
									SetState(State_SearchForCover);
									m_pPed->ReleaseDesiredCoverPoint();
									return FSM_Continue;
								}
							}

							// If we found any type of nav link
							if(bFoundDangerousNavLink || bFoundNavLink)
							{
								if(m_pCoverPointBeingTested)
								{
									// We need to make sure when leaving this state we don't clear the cover point from the array
									for(s32 i = 0; i < COVER_POINT_HISTORY_SIZE; i++)
									{
										// If we found this cover point in the array
										if(m_apFoundCoverPoints[i] == m_pCoverPointBeingTested)
										{
											// set our cached version to NULL to prevent it from clearing the array entry in the OnExit and set the proper fail reason
											m_pCoverPointBeingTested = NULL;
											m_aFailReasons[i] = bFoundDangerousNavLink ? (u8)FAIL_REASON_DANGEROUS_NAV_LINK : (u8)FAIL_REASON_NAV_LINK;
											break;
										}
									}
								}

								CoverRejected(vCoverPosition, "Link found in path - attempting to find different cover point");
								SetState(State_SearchForCover);
								m_pPed->ReleaseDesiredCoverPoint();
							}
							else
							{
								Reset(SEARCH_SUCCEEDED, true);
							}

							return FSM_Continue;
						}
						else
						{
							CoverRejected(vCoverPosition, "Route didn't get close enough");
						}
					}
				}
				else
				{
					CoverRejected(vLastPoint, "Route too long");
				}
			}
			else
			{
				CoverRejected(vLastPoint, "No route found");
			}
			
			SetState(State_SearchForCover);
			m_pPed->ReleaseDesiredCoverPoint();
			return FSM_Continue;
		}
	}
	else
	{
		SetState(State_SearchForCover);
		return FSM_Continue;
	}

	return FSM_Continue;
}


void CCoverFinderFSM::CheckRouteToCoverPoint_OnExit()
{
	// If we failed to find a route (or failed for any other reason here), NULL out this cover point so it isn't used as a fall back
	if(GetState() == State_SearchForCover && m_pCoverPointBeingTested)
	{
		for(s32 i = 0; i < COVER_POINT_HISTORY_SIZE; i++)
		{
			if(m_apFoundCoverPoints[i] == m_pCoverPointBeingTested)
			{
				m_apFoundCoverPoints[i] = NULL;
				m_aFailReasons[i] = FAIL_REASON_NONE;
			}
		}
	}

	m_RouteSearchHelper.ResetSearch();
}

Vector3 CCoverFinderFSM::GetThreatPosition() const
{
	return m_TargetPos;
}

Vector3 CCoverFinderFSM::GetThreatVantagePosition() const
{
	if( m_pTargetPed && m_pTargetPed->GetCoverPoint() )
	{	
		Vector3 vCoverPos;
		Vector3 vVantagePos;
		if(CCover::FindCoordinatesCoverPoint(m_pTargetPed->GetCoverPoint(), m_pTargetPed, VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition() - m_pTargetPed->GetTransform().GetPosition()), vCoverPos, &vVantagePos))
		{
			return vVantagePos;
		}
	}

	return m_TargetPos;
}

bool CCoverFinderFSM::EvaluateCoverPoint(Vector3 &vThreatPos )
{
	if (IsDesiredCoverPointValid())
	{
		Vector3 vCoverPosition;
		if( GetCoverPointPosition(vThreatPos, vCoverPosition, NULL) )
		{
			CCoverPoint* pDesiredCover = m_pPed->GetDesiredCoverPoint();

			// If the user passed in a filter construction function, make use of it and check if the point should be considered valid.
			CCoverPointFilterInstance filter;
			const CCoverPointFilter* pFilter = filter.Construct(m_pCoverPointFilterConstructFn, (void*)m_pPed);
			if( !pFilter || pFilter->CheckValidity(*pDesiredCover, RCC_VEC3V(vCoverPosition) ) )
			{
				CPedIntelligence* pPedIntelligence = m_pPed->GetPedIntelligence();
				const bool bDoesntRequireCover = m_iCoverSearchType != CCover::CS_MUST_PROVIDE_COVER ? true : false;

				if(bDoesntRequireCover || CCover::DoesCoverPointProvideCoverFromTargets( m_pPed, vThreatPos, pDesiredCover, pDesiredCover->GetArcToUse(*m_pPed, m_pTargetPed), vCoverPosition, m_pTargetPed ))
				{
					CCombatBehaviour& combatBehaviour = pPedIntelligence->GetCombatBehaviour();
					const Vector3 vPedPosition = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
					// Search starting defaults to the player position unless a position is specified
					Vector3 vSearchStartPos = vPedPosition;
					if( m_iCoverSearchMode == COVER_SEARCH_BY_POS )
					{
						vSearchStartPos = m_vCoverStartSearchPos;
					}
					const float fDistanceFromPedToPoint = vSearchStartPos.Dist2(vCoverPosition);
					const float fDistanceFromTargetToPoint =  vThreatPos.Dist2(vCoverPosition);
					const Vector3 vPedToTarget = vThreatPos - vSearchStartPos;
					const Vector3 vPedToCover = vCoverPosition - vSearchStartPos;
					bool bPointValid = true;

					// Make sure the point is infront if behind isn't ok
					if( m_flags.IsFlagSet(CTaskCover::CF_DontConsiderBackwardsPoints) && 
						vPedToTarget.Dot(vPedToCover) <= 0.0f)
					{
						CoverRejected(vCoverPosition, "BehindPed");
						bPointValid = false;
					}
					// AND (if flag set) make sure sure the cover point isn't too close to the target
					else if(CTaskCombat::ShouldMaintainMinDistanceToTarget(*m_pPed) &&
						fDistanceFromTargetToPoint < square(combatBehaviour.GetCombatFloat(kAttribFloatMinimumDistanceToTarget)) )
					{
						CoverRejected(vCoverPosition, "Too close to target");
						bPointValid = false;
					}
					// AND make sure the cover point is within the maximum range
					else if(!pPedIntelligence->GetDefensiveAreaForCoverSearch()->IsActive())
					{
						float fMaxDistance = MAX(MAX_DISTANCE_FROM_TARGET_TO_COVER, m_fMaxDistance);
						if(fDistanceFromPedToPoint >= rage::square(fMaxDistance))
						{
							CoverRejected(vCoverPosition, "too far from target");
							bPointValid = false;
						}
					}

					if ( bPointValid )
					{
						const CEntityScannerIterator entityList = pPedIntelligence->GetNearbyPeds();
						for(const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
						{
							const CPed* pOtherPed = static_cast<const CPed*>(pEnt);

							if(!pOtherPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AllowNearbyCoverUsage) && !pOtherPed->IsInjured())
							{
								Vector3 vOtherPedPosition = VEC3V_TO_VECTOR3(pOtherPed->GetTransform().GetPosition());

								if(pOtherPed->GetCoverPoint() && pOtherPed->GetCoverPoint()->GetCoverPointPosition(vOtherPedPosition))
								{
									// Have to add the additional unit upwards because vCoverPosition (the one we are trying to use) does it in CCoverFinderFSM::GetCoverPointPosition
									vOtherPedPosition.z += 1.0f;
								}
								else
								{
									CTaskMoveFollowNavMesh* pNavMeshTask = static_cast<CTaskMoveFollowNavMesh*>(pOtherPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH));
									if(pNavMeshTask && pNavMeshTask->HasTarget() && pNavMeshTask->GetRoute())
									{
										if(pNavMeshTask->GetLastRoutePointIsTarget() || !pNavMeshTask->WasTargetPosAdjusted())
										{
											vOtherPedPosition = pNavMeshTask->GetTarget();
										}
										else
										{
											pNavMeshTask->GetAdjustedTargetPos(vOtherPedPosition);
										}
									}
								}

								if(vOtherPedPosition.Dist2(vCoverPosition) < 1.0f)
								{
									bPointValid = false;
									CoverRejected(vCoverPosition, "Another ped or a ped's destination is too close");
									break;
								}
							}
						}

						if( bPointValid )
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

bool CCoverFinderFSM::CheckForFallbackCoverPoints(s32 &iCoverPointFailReason)
{
	// First try to find any cover point we failed due to nav links
	s32 iBestIndex = -1;
	for(s32 i = 0; i < COVER_POINT_HISTORY_SIZE; i++)
	{
		CCoverPoint* pCoverPoint = m_apFoundCoverPoints[i];
		if( pCoverPoint && m_aFailReasons[i] != FAIL_REASON_NONE &&
			pCoverPoint->GetType() != CCoverPoint::COVTYPE_NONE &&
			pCoverPoint->CanAccomodateAnotherPed() && !pCoverPoint->IsDangerous() &&
			!pCoverPoint->IsCloseToPlayer(*m_pPed) )
		{
			// Non dangerous nav link failures take priority, so set the best index and quit out right away
			if(m_aFailReasons[i] == FAIL_REASON_NAV_LINK)
			{
				iBestIndex = i;
				break;
			}
			else if(m_aFailReasons[i] == FAIL_REASON_DANGEROUS_NAV_LINK)
			{
				iBestIndex = i;
			}
		}
	}

	// If we found an index that failed due to a nav link then go ahead and use it
	if(iBestIndex >= 0)
	{
		iCoverPointFailReason = FAIL_REASON_NAV_LINK;
		m_pPed->SetDesiredCoverPoint(m_apFoundCoverPoints[iBestIndex]);
		return true;
	}

	iCoverPointFailReason = FAIL_REASON_NONE;
	if( m_pTargetPed && m_pPed->GetPedIntelligence()->GetDefensiveAreaForCoverSearch()->IsActive() )
	{
		CPedTargetting* pPedTargetting = m_pPed->GetPedIntelligence()->GetTargetting( false );
		if( pPedTargetting )
		{
			CSingleTargetInfo* pTargetInfo = pPedTargetting->FindTarget(m_pTargetPed);
			if(pTargetInfo && pTargetInfo->GetLosStatus() != Los_clear)
			{
				for(; m_iFallbackCoverPointIndex < COVER_POINT_HISTORY_SIZE; m_iFallbackCoverPointIndex++)
				{
					CCoverPoint* pCoverPoint = m_apFoundCoverPoints[m_iFallbackCoverPointIndex];
					if(pCoverPoint && pCoverPoint->GetType() != CCoverPoint::COVTYPE_NONE)
					{
						m_pPed->SetDesiredCoverPoint(pCoverPoint);
						m_iFallbackCoverPointIndex++;
						return true;
					}
				}
			}
		}
	}

	return false;
}

bool CCoverFinderFSM::GetCoverPointPosition( Vector3 &vThreatPos,Vector3& vCoverCoors, Vector3* vVantagePos )
{
	taskFatalAssertf(m_pPed->GetDesiredCoverPoint(), "Null cover point passed into GetCoverPointPosition");
	Vector3 vDirection = vThreatPos - VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
	vDirection.z = 0.0f;
	vDirection.Normalize();
	if(CCover::FindCoordinatesCoverPointNotReserved(m_pPed->GetDesiredCoverPoint(), vDirection, vCoverCoors, vVantagePos ) )
	{
		// Pull the cover position up to the peds root position rather than the ground, as its mostly
		// used for comparison at that height
		vCoverCoors.z += 1.0f;
		return true;
	}
	return false;
}

CTaskHelperFSM::FSM_Return CCoverFinderFSM::CoverSearchFailed_OnUpdate()
{
	s32 iFailReason = FAIL_REASON_NONE;
	if( CheckForFallbackCoverPoints(iFailReason) )
	{
		if(iFailReason == FAIL_REASON_NAV_LINK)
		{
			Reset(SEARCH_SUCCEEDED, true);
		}
		else
		{
			SetState(State_CheckCoverPointBlockedStatus);
		}
	}
	else
	{
		Reset(SEARCH_FAILED, true);
	}

	return FSM_Continue;
}

#if !__FINAL

const char* CCoverFinderFSM::GetStaticStateName( s32 iState ) 
{
	taskAssert(iState>=State_Waiting&&iState<=State_CoverSearchFailed);
	static const char* aStateNames[] = 
	{
		"State_Waiting",
		"State_SearchForCover",
		"State_CheckCoverPointAccessibility",
		"State_CheckTacticalAnalysisLos",
		"State_CheckCachedCoverPointBlockedStatus",
		"State_CheckCoverPointBlockedStatus",
		"State_CheckLosToCoverPoint",
		"State_CheckRouteToCoverPoint",
		"State_CoverSearchFailed",
	};

	return aStateNames[iState];	
}

void CCoverFinderFSM::Debug(s32 UNUSED_PARAM(iIndentLevel)) const
{
#if __DEV
	if(m_pPed && CPedDebugVisualiserMenu::ms_menuFlags.m_bDisplayCoverSearch &&
		(!CPedDebugVisualiserMenu::IsDisplayingOnlyForFocus() || m_pPed == CDebugScene::FocusEntities_Get(0) ))
	{
		if(m_pPed->GetDesiredCoverPoint())
		{
			Vector3 vCoverPosition;
			m_pPed->GetDesiredCoverPoint()->GetCoverPointPosition(vCoverPosition);
			grcDebugDraw::Line(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition()), vCoverPosition, Color_blue);
		}
	}
#endif
}

#endif // !__FINAL
