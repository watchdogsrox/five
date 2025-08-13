// FILE :    TaskInvestigate.cpp
// PURPOSE : Investigate task, used when an event has occurred at a location that should be investigated.
//			 Can be a distant sighting of a threat.
// AUTHOR :  Phil H
// CREATED : 13-10-2008

// File header
#include "Task/Combat/TaskInvestigate.h"

// Framework headers
#include "ai/task/taskchannel.h"
#include "grcore/debugdraw.h"

// Game headers
#include "Event/EventReactions.h"
#include "Game/Clock.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "scene/world/GameWorld.h" 
#include "Task/Combat/TaskSearch.h"
#include "Task/Default/Patrol/TaskPatrol.h"
#include "Task/Default/Patrol/PatrolRoutes.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskGoTo.h"
#include "Task/Movement/TaskGoToPointAiming.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Default/TaskWander.h"
#include "Task/Combat/TaskSearch.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Default/TaskChat.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/WeaponController.h"
#include "Event/EventWeapon.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleGoTo.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"

AI_OPTIMISATIONS()

bool	  CTaskInvestigate::ms_bToggleRenderInvestigationPosition   = false;

CTaskInvestigate::Tunables CTaskInvestigate::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskInvestigate, 0x6f2e3d7c);

CTaskInvestigate::CTaskInvestigate( const Vector3& vInvestigationPosition, CPed* pWatchPed, s32 iInvestigationEventType, s32 iTimeToSpendAtSearchPoint, bool bReturnToPosition)
: m_vInvestigatePosition(vInvestigationPosition)
, m_vInitialPosition(0.0f, 0.0f, 0.0f)
, m_vVehicleInvestigatePosition(0.0f, 0.0f, 0.0f)
, m_pWatchPed(pWatchPed)
, m_InvestigationState(State_Start)
, m_iInvestigationEventType(iInvestigationEventType)
, m_iTimeToSpendAtSearchPoint(iTimeToSpendAtSearchPoint)
, m_standTimer(sm_Tunables.m_fTimeToStandAtPerimeter)
, m_newPositionTimer(sm_Tunables.m_fNewPositionThreshold)
, m_iPreviousWeaponHash(0)
, m_bClearingCorner(false)
, m_bQuitForDeadPedInvestigation(false)
, m_bNewPosition(false)
, m_bCanSetNewPosition(false)
, m_bInvestigatingWhistlingEvent(false)
, m_bRandomiseInvestigationPosition(true)
, m_bAllowedToDropCached(false)
, m_bReturnToPositionAfterInvestigate(bReturnToPosition)
{
	m_pChatHelper = CChatHelper::CreateChatHelper();
	SetInternalTaskType(CTaskTypes::TASK_INVESTIGATE);
}

CTaskInvestigate::~CTaskInvestigate()
{
	if (m_pChatHelper)
	{	
		delete m_pChatHelper;
		m_pChatHelper = NULL;
	}
}

void CTaskInvestigate::SetInvestigatingAnimationGroup( CPed* pPed )
{
	fwMvClipSetId clipSetId;
	CPedIntelligence* pPedIntelligence = pPed->GetPedIntelligence();
	taskFatalAssertf(pPedIntelligence,"Ped has no intelligence");

	if (pPedIntelligence->GetAlertnessState() == CPedIntelligence::AS_VeryAlert || (pPedIntelligence->GetCurrentEvent() && pPedIntelligence->GetCurrentEvent()->GetEventPriority() == E_PRIORITY_REACTION_INVESTIGATE_HIGH_THREAT))
	{
		clipSetId = MOVE_PED_BASIC_LOCOMOTION;	// NB: Temporary until a specific clip set is made

		// Run to investigation point
		// TODO: Cowardly peds flee at this point?
		aiTask* pTask = pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
		if (pTask)
		{
			static_cast<CTaskMoveFollowNavMesh*>(pTask)->SetMoveBlendRatio(MOVEBLENDRATIO_RUN);
		}
	}
	else
	{
		clipSetId = MOVE_GUARD_ALERT;
	}

	if (pPedIntelligence->GetCurrentEventType() == EVENT_REACTION_INVESTIGATE_DEAD_PED)
	{
		clipSetId = MOVE_PED_BASIC_LOCOMOTION;	// NB: Temporary until a specific clip set is made
	}

	if( m_moveGroupClipSetRequestHelper.Request(clipSetId) && clipSetId != CLIP_SET_ID_INVALID )
	{
		// set the ped's motion group
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
		fwMvClipSetId set = clipSetId;
		pTask->SetDefaultOnFootClipSet(set);
	}
}

CTask::FSM_Return CTaskInvestigate::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (m_newPositionTimer.Tick())
	{
		m_bCanSetNewPosition = true;
	}

	if (m_bQuitForDeadPedInvestigation)
	{
		// We've stored the dead ped in m_pWatchPed when we call QuitForDeadPedInvestigation
		CEventDeadPedFound deadPedEvent(m_pWatchPed,true);
		pPed->GetPedIntelligence()->AddEvent(deadPedEvent);
		return FSM_Quit;
	}

	// If we have a chat helper, look for chats
	if (m_pChatHelper)
	{		
		m_pChatHelper->ProcessChat(pPed);
	}

	// Stream and set the investigating movement group
	SetInvestigatingAnimationGroup(pPed);

	return FSM_Continue;
}

void CTaskInvestigate::CleanUp( )
{
	m_moveGroupClipSetRequestHelper.Release();
	CTaskMotionBase* pTask = GetPed()->GetPrimaryMotionTask();
	pTask->ResetOnFootClipSet();

	Assert(GetPed());
	GetPed()->GetPedIntelligence()->GetNavCapabilities().SetFlag(CPedNavCapabilityInfo::FLAG_MAY_DROP, m_bAllowedToDropCached);
	GetPed()->SetUsingActionMode(false, CPed::AME_Investigate);
}

CTask::FSM_Return CTaskInvestigate::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// Initial start to the task
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter(pPed);
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		// Watch an investigating ped
		FSM_State(State_WatchInvestigationPed)
			FSM_OnEnter
				WatchInvestigationPed_OnEnter(pPed);
			FSM_OnUpdate
				return WatchInvestigationPed_OnUpdate(pPed);

		// Go to the disturbance point that has to be investigated
		FSM_State(State_GotoInvestigatePosOnFoot)
			FSM_OnEnter
				GotoInvestigatePosOnFoot_OnEnter(pPed);
			FSM_OnUpdate
				return GotoInvestigatePosOnFoot_OnUpdate(pPed);		

		FSM_State(State_StandAtPerimeter)
			FSM_OnEnter
				StandAtPerimeter_OnEnter(pPed);
			FSM_OnUpdate
				return StandAtPerimeter_OnUpdate(pPed);		

		FSM_State(State_UseRadio)
			FSM_OnEnter
			UseRadio_OnEnter(pPed);
		FSM_OnUpdate
			return UseRadio_OnUpdate(pPed);

		// Talk to a fellow investigator if we see them (Not running watch investigation)
		FSM_State(State_ChatToPed)
			FSM_OnEnter
				ChatToPed_OnEnter(pPed);
			FSM_OnUpdate
				return ChatToPed_OnUpdate(pPed);

		// Search Investigation Position
		FSM_State(State_SearchInvestigationPosition)
			FSM_OnEnter
				SearchInvestigationPosition_OnEnter(pPed);
			FSM_OnUpdate
				return SearchInvestigationPosition_OnUpdate(pPed);

		// Go to the disturbance point that has to be investigated
		FSM_State(State_ReturnToOriginalPositionOnFoot)
			FSM_OnEnter
				ReturnToOriginalPositionOnFoot_OnEnter(pPed);
			FSM_OnUpdate
				return ReturnToOriginalPositionOnFoot_OnUpdate(pPed);

		// Get in a nearby vehicle
		FSM_State(State_EnterNearbyVehicle)
			FSM_OnEnter
				EnterNearbyVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return EnterNearbyVehicle_OnUpdate(pPed);

		// Goto the investigate position in a vehicle
		FSM_State(State_GotoInvestigatePosInVehicle)
			FSM_OnEnter
				GotoInvestigatePosInVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return GotoInvestigatePosInVehicle_OnUpdate(pPed);

		// Exit the vehicle once the investigate position has been reached
		FSM_State(State_ExitVehicleAtPos)
			FSM_OnEnter
				ExitVehicleAtPos_OnEnter(pPed);
			FSM_OnUpdate
				return ExitVehicleAtPos_OnUpdate(pPed);

		// Return to the initial position in a vehicle
		FSM_State(State_ReturnToOriginalPositionInVehicle)
			FSM_OnEnter
				ReturnToOriginalPositionInVehicle_OnEnter(pPed);
			FSM_OnUpdate
				return ReturnToOriginalPositionInVehicle_OnUpdate(pPed);

		// Enter the peds vehicle to return to the initial position
		FSM_State(State_EnterNearbyVehicleToReturn)
			FSM_OnEnter
				EnterNearbyVehicleToReturn_OnEnter(pPed);
			FSM_OnUpdate
				return EnterNearbyVehicleToReturn_OnUpdate(pPed);

		// Quit
		FSM_State(State_Finish)
			FSM_OnEnter
				CleanUp();
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

//-------------------------------------------------------------------------------------------
//  Start state
//-------------------------------------------------------------------------------------------

void CTaskInvestigate::Start_OnEnter(CPed* pPed)
{
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	// Search for nearby peds running an investigation task
	// If none are found, we should investigate
	// If someone is found but they're not close enough, we should investigate
	// If someone is found and they are close enough to be watched, watch them
	CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext())
	{
		CPed* pNearestPed = static_cast<CPed*>(pEnt);
		float fDistToNearestPed = VEC3V_TO_VECTOR3(pNearestPed->GetTransform().GetPosition()).Dist(vPedPosition);
		
		// Make sure the ped is within id range and is friendly
		if (fDistToNearestPed < 10.0f)	
		{
			aiTask* pTask = pNearestPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INVESTIGATE);
			if (pTask)
			{	
				m_pWatchPed = pNearestPed; // This will do a watch investigation
				return;
			}
		}
	 }	

	if (m_iInvestigationEventType == EVENT_WHISTLING_HEARD)
	{
		m_bInvestigatingWhistlingEvent = true;
	}

	// Store the peds initial position
	m_vInitialPosition = vPedPosition;

	//Set Action Mode on our ped
	pPed->SetUsingActionMode(true, CPed::AME_Investigate);
}

CTask::FSM_Return CTaskInvestigate::Start_OnUpdate( CPed* pPed )
{
	 // If we have a ped to watch and we're not in a vehicle watch them
	if (m_pWatchPed && !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		SetState(State_WatchInvestigationPed);
		return FSM_Continue;
	}

	if (pPed->GetMyVehicle())
	{
		// If this ped is associated with a vehicle, work out if there is a car node suitably close
		// To drive to, otherwise go on foot
		float fDistToPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).Dist(m_vInvestigatePosition);
		if (fDistToPos > sm_Tunables.m_fMinDistanceToUseVehicle)
		{
			float fNodeSearchDistance = fDistToPos*sm_Tunables.m_fMinDistanceSavingToUseVehicle;
			CNodeAddress NearestNode = ThePaths.FindNodeClosestToCoors(m_vInvestigatePosition, fNodeSearchDistance);
			if (!NearestNode.IsEmpty())
			{
				// Store the node position in m_vVehicleInvestigatePosition
				ThePaths.FindNodePointer(NearestNode)->GetCoors(m_vVehicleInvestigatePosition);

				if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
				{
					m_InvestigationState = State_GotoInvestigatePosInVehicle;	// Save the state we'll be moving to after using the radio
					SetState(State_UseRadio);
				}
				else
				{
					m_InvestigationState = State_EnterNearbyVehicle;
					SetState(State_UseRadio);
				}
				return FSM_Continue;
			}
		}
	}

	// We're gonna investigate on foot, if its a high threat, radio it in before moving
	m_InvestigationState = State_GotoInvestigatePosOnFoot;	
	SetState(State_GotoInvestigatePosOnFoot);
	return FSM_Continue;
}

//-------------------------------------------------------------------------------------------
// WatchInvestigationPed
//-------------------------------------------------------------------------------------------

void CTaskInvestigate::WatchInvestigationPed_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	//SetNewTask(rage_new CTaskWatchInvestigation(m_pWatchPed,m_vInvestigatePosition));
}

CTask::FSM_Return CTaskInvestigate::WatchInvestigationPed_OnUpdate( CPed* UNUSED_PARAM(pPed))
{
	if (GetIsSubtaskFinished(CTaskTypes::TASK_WATCH_INVESTIGATION))
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

//-------------------------------------------------------------------------------------------
// GotoInvestigatePosOnFoot
//-------------------------------------------------------------------------------------------

void CTaskInvestigate::GotoInvestigatePosOnFoot_OnEnter( CPed* pPed)
{
	// Randomise the investigation position depending on the distance and the alertness
	// if we're more alert, make it more likely to go to the correct area
	if (m_bRandomiseInvestigationPosition)
	{
		float fDistToInvestPos = 0.5f*(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vInvestigatePosition).Mag();

		s32 iAlertness = pPed->GetPedIntelligence()->GetAlertnessState();
		float fScaleModifier = 1.0f;

		switch (iAlertness)
		{
		case CPedIntelligence::AS_VeryAlert:		fScaleModifier = 0.75f; break;
		case CPedIntelligence::AS_Alert:			fScaleModifier = 0.5f;	break;
		case CPedIntelligence::AS_MustGoToCombat:	 fScaleModifier = 0.1f; break;
		default:
			break;
		}

		fDistToInvestPos *= fScaleModifier;

		m_vInvestigatePosition.x += fwRandom::GetRandomNumberInRange(-fDistToInvestPos,fDistToInvestPos);
		m_vInvestigatePosition.y += fwRandom::GetRandomNumberInRange(-fDistToInvestPos,fDistToInvestPos);
	}

	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, m_vInvestigatePosition, 2.0f, CTaskMoveFollowNavMesh::ms_fSlowDownDistance, -1, true, false, NULL, 30.0f);
	pMoveTask->SetCompletionRadius(20.0f);	// If we can't get to the exact position, accept a location within 20m
	// JB: Added these to alleviate severe spamming of the pathfinder by multiple distant peds
	pMoveTask->SetTimeToPauseBetweenSearchAttempts(5000);
	pMoveTask->SetTimeToPauseBetweenRepeatSearches(5000);
	// Start walking to the investigation position
	SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));

	// Don't allow the ped to drop from a fatal height whilst moving
	m_bAllowedToDropCached = pPed->GetPedIntelligence()->GetNavCapabilities().IsFlagSet(CPedNavCapabilityInfo::FLAG_MAY_DROP);
	pPed->GetPedIntelligence()->GetNavCapabilities().ClearFlag(CPedNavCapabilityInfo::FLAG_MAY_DROP);

	m_bClearingCorner = false;
}

bool CTaskInvestigate::ShouldPedClearCorner(const CPed* pPed, CTaskMoveFollowNavMesh* pNavMeshTask, Vector3& vOutLastCornerCleared, Vector3& vOutStartPos, Vector3& vOutCornerPosition, Vector3& vOutBeyondCornerPosition, bool bCheckWalkingIntoInterior)
{
	if(!pPed || !pNavMeshTask)
	{
		return false;
	}

	if( pNavMeshTask->GetThisWaypointLine( pPed, vOutStartPos, vOutCornerPosition, 0 ) &&
			pNavMeshTask->GetThisWaypointLine( pPed, vOutCornerPosition, vOutBeyondCornerPosition, 1 ) )
	{
		bool bWalkingIntoInterior = false;
		if(bCheckWalkingIntoInterior && (pNavMeshTask->GetProgress() + 1) < pNavMeshTask->GetRouteSize())
		{
			bWalkingIntoInterior = !(pNavMeshTask->GetWaypointFlag(pNavMeshTask->GetProgress()).m_iSpecialActionFlags & WAYPOINT_FLAG_IS_INTERIOR ) &&
				(pNavMeshTask->GetWaypointFlag(pNavMeshTask->GetProgress()+1).m_iSpecialActionFlags & WAYPOINT_FLAG_IS_INTERIOR );
		}

		static bool DEBUG_CORNER_SEARCH = false;
		if( DEBUG_CORNER_SEARCH )
		{
			DEBUG_DRAW_ONLY(grcDebugDraw::Line(vOutStartPos, vOutCornerPosition, Color_orange, Color_orange ));
			DEBUG_DRAW_ONLY(grcDebugDraw::Line(vOutCornerPosition, vOutBeyondCornerPosition, Color_orange, Color_orange ));
		}

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		if(	vOutLastCornerCleared.Dist2(vOutCornerPosition) > rage::square(4.0f) &&
			vPedPosition.Dist2(vOutCornerPosition) < rage::square(5.0f) )
		{
			Vector3 vLength1 = vOutCornerPosition - vOutStartPos;
			Vector3 vLength2 = vOutBeyondCornerPosition - vOutCornerPosition;
			vLength1.z = 0.0f;
			vLength2.z = 0.0f;
			vLength1.Normalize();
			vLength2.Normalize();
			if( vLength1.Dot(vLength2) < 0.98f )
			{
				vOutLastCornerCleared = vOutCornerPosition;
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetStartAndEnd(vPedPosition, vOutBeyondCornerPosition+ZAXIS);
				probeDesc.SetExcludeEntity(pPed);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_OBJECT_TYPE);
				probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
				if( bWalkingIntoInterior || WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc) )
				{
					return true;
				}
			}
		}
	}

	return false;
}

CTask::FSM_Return CTaskInvestigate::GotoInvestigatePosOnFoot_OnUpdate( CPed* pPed)
{
	// New investigation position due to new events
	if (m_bNewPosition)
	{
		m_bNewPosition = false;
		SetFlag(aiTaskFlags::RestartCurrentState);
		return FSM_Continue;
	}

	// If we've reached the investigation position, search for the threat from here
	if (!m_bClearingCorner && GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT) )
	{
		SetState(State_SearchInvestigationPosition);
		return FSM_Continue;
	}
	else if (m_bClearingCorner && GetIsSubtaskFinished(CTaskTypes::TASK_GO_TO_POINT_AIMING) )
	{
		// Restart the follow nav mesh task
		CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, m_vInvestigatePosition, 2.0f, CTaskMoveFollowNavMesh::ms_fSlowDownDistance, -1, true, false, NULL, 30.0f);
		pMoveTask->SetCompletionRadius(20.0f);	// If we can't get to the exact position, accept a location within 20m
		// Start walking to the investigation position
		SetNewTask(rage_new CTaskComplexControlMovement(pMoveTask));
		m_bClearingCorner = false;
	}

	if (!m_bClearingCorner) 
	{
		CTaskMoveFollowNavMesh* pNavMeshTask = (CTaskMoveFollowNavMesh*) pPed->GetPedIntelligence()->FindMovementTaskByType(CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH);
		Vector3 vStartPos, vCornerPosition, vBeyondCornerPosition;
		const bool bCheckWalkingIntoInterior = true;
		if( pNavMeshTask && ShouldPedClearCorner(pPed, pNavMeshTask, m_vLastCornerCleared, vStartPos, vCornerPosition, vBeyondCornerPosition, bCheckWalkingIntoInterior))
		{
			if( GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_URGENT, NULL ) )
			{
				SetNewTask(rage_new CTaskGoToPointAiming( CAITarget(vCornerPosition), CAITarget(vBeyondCornerPosition), MOVEBLENDRATIO_RUN ));
				m_bClearingCorner = true;
			}
		}
	}


	CDefensiveArea* pDefArea = pPed->GetPedIntelligence()->GetDefensiveArea();
	if (pDefArea && pDefArea->IsActive())
	{
		if (!pDefArea->IsPointInDefensiveArea(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())))
		{
			SetState(State_StandAtPerimeter);
			return FSM_Continue;
		}
	}

	// Look for chats
	if (m_pChatHelper)
	{
		if (m_pChatHelper->ShouldGoToChatState())
		{
			m_pChatHelper->SetIsChatting(true); 
			SetState(State_ChatToPed);
		}
		else
		{
			CPed* pChatPed = m_pChatHelper->GetBestChatPed();
			if (pChatPed)
			{
				aiTask* pTask = pChatPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INVESTIGATE);
				if (pTask)
				{
					s32 iOtherPedsEvent = static_cast<CTaskInvestigate*>(pTask)->GetInvestigationEventType();
					// We're investigating different things, should I investigate what you're investigating?
					if (iOtherPedsEvent != m_iInvestigationEventType)
					{
						if (ShouldInvestigateOtherEvent(iOtherPedsEvent) && m_bCanSetNewPosition)
						{
							SetInvestigationPos(static_cast<CTaskInvestigate*>(pTask)->GetInvestigationPos());
						}
					}
				}
			}
		}
	}

	return FSM_Continue;
}

bool CTaskInvestigate::ShouldInvestigateOtherEvent(s32 iOtherEvent)
{
	// No way to get event priorities from eventtype, so just select a few important ones
	if (m_iInvestigationEventType != EVENT_REACTION_INVESTIGATE_THREAT && m_iInvestigationEventType != EVENT_UNIDENTIFIED_PED && m_iInvestigationEventType != EVENT_SHOT_FIRED)
	{
		// Seeing an enemy or hearing gunfire is more important to investigate
		if (iOtherEvent == EVENT_REACTION_INVESTIGATE_THREAT || iOtherEvent == EVENT_UNIDENTIFIED_PED)
		{
			return true;
		}
	}
	return false;
}

void CTaskInvestigate::StandAtPerimeter_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	m_standTimer.Reset();
}

CTask::FSM_Return CTaskInvestigate::StandAtPerimeter_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	// TODO: Play some aiming idles
	m_standTimer.Tick();
	if (m_standTimer.IsFinished())
	{
		SetState(State_ReturnToOriginalPositionOnFoot);
	}
	return FSM_Continue;
}

void CTaskInvestigate::UseRadio_OnEnter(CPed* UNUSED_PARAM(pPed))
{
// 	switch (m_InvestigationState)
// 	{
// 	case State_GotoInvestigatePosInVehicle:
// 	case State_EnterNearbyVehicle:
// 	case State_GotoInvestigatePosOnFoot:
// 		// If reacting, then get the actual current event (from the initial reaction)
// 		if (pPed->GetPedIntelligence()->GetCurrentEventType() == EVENT_REACTION_INVESTIGATE_THREAT)
// 		{
// 			s32 iEventType = static_cast<CEventReactionInvestigateThreat*>(pPed->GetPedIntelligence()->GetCurrentEvent())->GetInitialEventType();
// 			if (iEventType == EVENT_SHOT_FIRED ||
// 				iEventType == EVENT_SHOT_FIRED_BULLET_IMPACT || 
// 				iEventType == EVENT_SHOT_FIRED_WHIZZED_BY)
// 			{
// 				SetNewTask(rage_new CTaskUseRadio("RADIO_REPORT_GUNSHOT",CAITarget(m_vInvestigatePosition), false, true));
// 			}
// 		}
// 		else
// 		{
// 			SetNewTask(rage_new CTaskUseRadio("RADIO_INVESTIGATING", CAITarget(m_vInvestigatePosition), false, true));
// 		}
// 		break;
// 	case State_ReturnToOriginalPositionOnFoot:
// 	case State_ReturnToOriginalPositionInVehicle:
// 		SetNewTask(rage_new CTaskUseRadio("RADIO_ABANDON_SEARCH"));
// 		break;
// 	default:
// 		break;
// 	}
}

CTask::FSM_Return CTaskInvestigate::UseRadio_OnUpdate(CPed* UNUSED_PARAM(pPed ))
{
	// Goto the store state once we've finished radioing in
	//if (GetIsSubtaskFinished(CTaskTypes::TASK_USE_RADIO))
	{
		SetState(m_InvestigationState);
	}
	return FSM_Continue;
}

void CTaskInvestigate::ChatToPed_OnEnter( CPed* UNUSED_PARAM(pPed ))
{
	// Start the chat task
	taskAssertf(m_pChatHelper,"Chat to ped state entered with no chathelper");
	if (m_pChatHelper)
	{
		if (m_pChatHelper->ShouldStartTaskAsInitiator()) // This function contains common functionality for tasks using the chat helper
		{
			SetNewTask( rage_new CTaskChat(m_pChatHelper->GetOutGoingChatPed(),CTaskChat::CF_IsInitiator) );
			m_pWatchPed = m_pChatHelper->GetOutGoingChatPed();
		}
		else
		{
			SetNewTask( rage_new CTaskChat(m_pChatHelper->GetInComingChatPed(),0) );
		}
	}
}

CTask::FSM_Return CTaskInvestigate::ChatToPed_OnUpdate( CPed*  UNUSED_PARAM(pPed ))
{
	// Once chat has finished, go back to investigating (or watching)
	if (m_pChatHelper && GetIsSubtaskFinished(CTaskTypes::TASK_CHAT))
	{
		m_pChatHelper->FinishChat();

		if (m_pWatchPed)
		{
			SetState(State_WatchInvestigationPed);
		}
		else
		{
			SetState(State_GotoInvestigatePosOnFoot);
		}
	}

	return FSM_Continue;
}

void CTaskInvestigate::SearchInvestigationPosition_OnEnter (CPed* pPed)
{
	// Start a search for an unknown threat in a forward direction
	SetNewTask(rage_new CTaskSearchForUnknownThreat(VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()), m_iInvestigationEventType, m_iTimeToSpendAtSearchPoint ));
}

CTask::FSM_Return CTaskInvestigate::SearchInvestigationPosition_OnUpdate (CPed* pPed)
{
	// If the search has finished, return
	if (GetIsSubtaskFinished(CTaskTypes::TASK_SEARCH_FOR_UNKNOWN_THREAT))
	{
		if (pPed->GetMyVehicle())
		{
			m_InvestigationState = State_EnterNearbyVehicleToReturn;
		}
		else
		{
			m_InvestigationState = State_ReturnToOriginalPositionOnFoot;
		}
		SetState(State_UseRadio);
	}
	return FSM_Continue;
}

//-------------------------------------------------------------------------------------------
// ReturnToOriginalPositionOnFoot
//-------------------------------------------------------------------------------------------

void CTaskInvestigate::ReturnToOriginalPositionOnFoot_OnEnter( CPed* pPed )
{
	if (m_bReturnToPositionAfterInvestigate)
	{
		aiTask* pTask = pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_PATROL);
		if (pTask)
		{
			Vector3 vCurrentPatrolNodePos = static_cast<CTaskPatrol*>(pTask)->GetCurrentPatrolNode()->GetPosition();
			Vector3 vCurrentPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());

			// Get the ped's forward vector
			Vector3 fv = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
			fv.Normalize();

			// Get the vector to the current patrol node
			Vector3 tv = vCurrentPatrolNodePos - vCurrentPosition;
			tv.Normalize();

			// Find the angle between the vectors
			float cos_theta = fv.Dot(tv);

			if (cos_theta <= 0.0f)
			{
				// The current patrol node is behind of the way we're facing, change to the next patrol node and don't return to original position
				static_cast<CTaskPatrol*>(pTask)->MoveToNextPatrolNode();
			}
			return;
		}

		SetNewTask(rage_new CTaskComplexControlMovement( rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, m_vInitialPosition)));
	}
}

CTask::FSM_Return CTaskInvestigate::ReturnToOriginalPositionOnFoot_OnUpdate( CPed* UNUSED_PARAM(pPed))
{
	if( GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT) )
	{
		SetState(State_Finish);
	}

	return FSM_Continue;
}

//-------------------------------------------------------------------------------------------
// EnterNearbyVehicle
//-------------------------------------------------------------------------------------------

void CTaskInvestigate::EnterNearbyVehicle_OnEnter( CPed* pPed )
{
	SetNewTask(rage_new CTaskEnterVehicle(pPed->GetMyVehicle(), SR_Prefer, pPed->GetMyVehicle()->GetDriverSeat()));
}

CTask::FSM_Return CTaskInvestigate::EnterNearbyVehicle_OnUpdate( CPed* pPed )
{
	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
		{
			SetState( State_GotoInvestigatePosInVehicle);
		}
		else
		{
			SetState( State_GotoInvestigatePosOnFoot);
		}
	}
	return FSM_Continue;
}

//-------------------------------------------------------------------------------------------
// GotoInvestigatePosInVehicle
//-------------------------------------------------------------------------------------------

void CTaskInvestigate::GotoInvestigatePosInVehicle_OnEnter( CPed* pPed )
{
	CTaskControlVehicle* pTask = NULL;

	sVehicleMissionParams params;
	params.m_iDrivingFlags = DMode_AvoidCars;
	params.m_fCruiseSpeed = 15.0f;
	params.SetTargetPosition(m_vVehicleInvestigatePosition);
	params.m_fTargetArriveDist = 5.0f;

	CTaskVehicleMissionBase* pVehicleTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
	pTask = rage_new CTaskControlVehicle(pPed->GetMyVehicle(), pVehicleTask);

	SetNewTask(pTask);
}

CTask::FSM_Return CTaskInvestigate::GotoInvestigatePosInVehicle_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	if( GetIsSubtaskFinished(CTaskTypes::TASK_CONTROL_VEHICLE) )
	{
		SetState(State_ExitVehicleAtPos);
	}
	return FSM_Continue;
}

//-------------------------------------------------------------------------------------------
// ExitVehicleAtPos
//-------------------------------------------------------------------------------------------

void CTaskInvestigate::ExitVehicleAtPos_OnEnter( CPed* pPed )
{
	VehicleEnterExitFlags vehicleFlags;
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DontCloseDoor);
	SetNewTask( rage_new CTaskExitVehicle(pPed->GetMyVehicle(), vehicleFlags) );
}

CTask::FSM_Return CTaskInvestigate::ExitVehicleAtPos_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	if( GetIsSubtaskFinished(CTaskTypes::TASK_EXIT_VEHICLE) )
	{
		SetState(State_GotoInvestigatePosOnFoot);
	}
	return FSM_Continue;
}

//-------------------------------------------------------------------------------------------
// ReturnToOriginalPositionInVehicle
//-------------------------------------------------------------------------------------------

void CTaskInvestigate::ReturnToOriginalPositionInVehicle_OnEnter( CPed* pPed )
{
	if (m_bReturnToPositionAfterInvestigate)
	{
		CTaskControlVehicle* pTask = NULL;

		sVehicleMissionParams params;
		params.m_iDrivingFlags = DMode_AvoidCars;
		params.m_fCruiseSpeed = 15.0f;
		params.SetTargetPosition(m_vInitialPosition);
		params.m_fTargetArriveDist = 5.0f;
		CTaskVehicleMissionBase* pCarTask = CVehicleIntelligence::CreateAutomobileGotoTask(params, sVehicleMissionParams::DEFAULT_SWITCH_TO_STRAIGHT_LINE_DIST);
		pTask = rage_new CTaskControlVehicle(pPed->GetMyVehicle(), pCarTask);

		SetNewTask(pTask);
	}
}

CTask::FSM_Return CTaskInvestigate::ReturnToOriginalPositionInVehicle_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	if( GetIsSubtaskFinished(CTaskTypes::TASK_CONTROL_VEHICLE) )
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

//-------------------------------------------------------------------------------------------
// EnterNearbyVehicleToReturn
//-------------------------------------------------------------------------------------------

void CTaskInvestigate::EnterNearbyVehicleToReturn_OnEnter( CPed* pPed )
{
	SetNewTask(rage_new CTaskEnterVehicle(pPed->GetMyVehicle(), SR_Prefer, pPed->GetMyVehicle()->GetDriverSeat()));
}

CTask::FSM_Return CTaskInvestigate::EnterNearbyVehicleToReturn_OnUpdate( CPed* pPed )
{
	if( GetIsSubtaskFinished(CTaskTypes::TASK_ENTER_VEHICLE) )
	{
		if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) )
		{
			SetState( State_ReturnToOriginalPositionInVehicle);
		}
		else
		{
			SetState( State_ReturnToOriginalPositionOnFoot);
		}
	}
	return FSM_Continue;
}

#if !__FINAL

void CTaskInvestigate::Debug() const
{
#if DEBUG_DRAW
	const CPed *pPed = GetPed();
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	if( GetState() == State_GotoInvestigatePosInVehicle )
	{
		grcDebugDraw::Line(vPedPosition, m_vVehicleInvestigatePosition, Color_blue, Color_blue);
		grcDebugDraw::Sphere(m_vVehicleInvestigatePosition, 5.0f, Color_blue, false);
		grcDebugDraw::Line(m_vVehicleInvestigatePosition, m_vInvestigatePosition, Color_blue, Color_green);
	}
	grcDebugDraw::Line(vPedPosition, m_vInvestigatePosition, Color_green, Color_green);
	grcDebugDraw::Sphere(m_vInvestigatePosition, 5.0f,Color_green, false);
#endif // DEBUG_DRAW
}

const char * CTaskInvestigate::GetStaticStateName( s32 iState  )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_WatchInvestigationPed",
		"State_GotoInvestigatePosOnFoot",
		"State_StandAtPerimeter",
		"State_UseRadio",
		"State_ChatToPed",
		"State_SearchInvestigationPosition",
		"State_ReturnToOriginalPositionOnFoot",
		"State_EnterNearbyVehicle",
		"State_GotoInvestigatePosInVehicle",
		"State_ExitVehicleAtPos",
		"State_EnterNearbyVehicleToReturn",
		"State_ReturnToOriginalPositionInVehicle",
		"State_Finish",
	};
	return aStateNames[iState];
}

#endif // !__FINAL

bank_s32 CTaskWatchInvestigation::ms_iTimeToStandAndWatchPed = 30000; 

CTaskWatchInvestigation::CTaskWatchInvestigation(CPed* pWatchPed, const Vector3& vInvestigationPosition )
: m_pWatchPed(pWatchPed)
, m_vInitialPosition(Vector3(0.0f,0.0f,0.0f))
, m_vInvestigationPosition(vInvestigationPosition)
, m_bCanSeeWatchPed(false)
{
	SetInternalTaskType(CTaskTypes::TASK_WATCH_INVESTIGATION);
}

CTaskWatchInvestigation::~CTaskWatchInvestigation()
{

}

CTask::FSM_Return CTaskWatchInvestigation::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// If watch ped becomes null or dead, quit the task
	if (!m_pWatchPed || m_pWatchPed->GetIsDeadOrDying())
	{
		return FSM_Quit;
	}

	// If I can't see the watch ped, go and follow
	if (!m_probeHelper.IsProbeActive()) // Start a line of sight test
	{
		WorldProbe::CShapeTestProbeDesc probeData;
		probeData.SetStartAndEnd(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()),VEC3V_TO_VECTOR3(m_pWatchPed->GetTransform().GetPosition()));
		probeData.SetContext(WorldProbe::EPedAcquaintances);
		probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
		m_probeHelper.StartTestLineOfSight(probeData);
	}
	else 
	{
		ProbeStatus probeStatus;
		if (m_probeHelper.GetProbeResults(probeStatus))
		{
			if (probeStatus == PS_Blocked)
			{
				m_bCanSeeWatchPed = false;
			}
			else 
			{
				// Test if the ped is directly in front of him
				Vector3 vToWatchPed = VEC3V_TO_VECTOR3(m_pWatchPed->GetTransform().GetPosition() - pPed->GetTransform().GetPosition());
				vToWatchPed.Normalize();
				Vector3 vForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
				vForward.Normalize();
				if (vForward.Dot(vToWatchPed) > 0.9f) // i.e. within an angle ~ 25 degrees
				{
					m_bCanSeeWatchPed = true;
				}
			}
		}
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskWatchInvestigation::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// Initial start to the task
		FSM_State(State_Start)
			FSM_OnUpdate
				Start_OnUpdate(pPed);

		// Play a you go and investigate clip
		FSM_State(State_PlayClip)
			FSM_OnEnter
			PlayClip_OnEnter(pPed);
		FSM_OnUpdate
			return PlayClip_OnUpdate(pPed);

		// Go to the disturbance point that has to be investigated
		FSM_State(State_FollowInvestigationPed)
			FSM_OnEnter
				FollowInvestigationPed_OnEnter(pPed);
			FSM_OnUpdate
				return FollowInvestigationPed_OnUpdate(pPed);

		// Stand and watch the ped investigate
		FSM_State(State_StandAndWatchInvestigationPed)
			FSM_OnEnter
				StandAndWatchInvestigationPed_OnEnter(pPed);
			FSM_OnUpdate
				return StandAndWatchInvestigationPed_OnUpdate(pPed);

		// Return to the original position
		FSM_State(State_ReturnToOriginalPositionOnFoot)
			FSM_OnEnter
				ReturnToOriginalPositionOnFoot_OnEnter(pPed);
			FSM_OnUpdate
				return ReturnToOriginalPositionOnFoot_OnUpdate(pPed);

		// Quit
		FSM_State(State_Finish)
			FSM_OnUpdate
			return FSM_Quit;

		FSM_End
}

CTask::FSM_Return CTaskWatchInvestigation::Start_OnUpdate( CPed* pPed )
{
	m_Timer.Set(fwTimer::GetTimeInMilliseconds(), ms_iTimeToStandAndWatchPed); // Total time to watch for
	m_vInitialPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	SetState(State_PlayClip);
	return FSM_Continue;
}

void CTaskWatchInvestigation::PlayClip_OnEnter (CPed* pPed)
{
	// Play a - you go and investigate clip
	// TODO: variation and direction
	StartClipBySetAndClip(pPed, CLIP_SET_RGUARDR_LOW_2, CLIP_REACT_GUARD_FORWARD, NORMAL_BLEND_IN_DELTA);
}

CTask::FSM_Return CTaskWatchInvestigation::PlayClip_OnUpdate (CPed* pPed)
{
	CClipHelper* pClipPlayer = GetClipHelper();

	// If clip has finished
	if(!pClipPlayer)
	{
		// If we can't see the ped we're watching, follow them until we can see them
		if (!pPed->GetPedIntelligence()->GetPedPerception().IsInSeeingRange(m_pWatchPed))
		{
			SetState(State_FollowInvestigationPed);
		}
		else
		{
			SetState(State_StandAndWatchInvestigationPed);
		}
	}

	return FSM_Continue;
}

void CTaskWatchInvestigation::FollowInvestigationPed_OnEnter(CPed* pPed)
{
	SetNewTask(rage_new CTaskComplexControlMovement(rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, VEC3V_TO_VECTOR3(m_pWatchPed->GetTransform().GetPosition()))));
	// Look towards the investigation position
	pPed->GetIkManager().LookAt(0, NULL, 5000, BONETAG_INVALID, &m_vInvestigationPosition);
}

CTask::FSM_Return CTaskWatchInvestigation::FollowInvestigationPed_OnUpdate(CPed* UNUSED_PARAM(pPed))
{	
	if (GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT))
	{
		SetState(State_StandAndWatchInvestigationPed);
	}
	else if (m_bCanSeeWatchPed)
	{
		// If we can see the ped stand and watch
		SetState(State_StandAndWatchInvestigationPed);
	}

	return FSM_Continue;
}

void CTaskWatchInvestigation::StandAndWatchInvestigationPed_OnEnter( CPed* pPed)
{
	if (!pPed->GetIkManager().IsLooking())
	{
		pPed->GetIkManager().LookAt(0, m_pWatchPed, 10000, BONETAG_INVALID, NULL);
	}
	SetNewTask( rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips | CTaskAmbientClips::Flag_PlayBaseClip, CONDITIONALANIMSMGR.GetConditionalAnimsGroup("Scenario_Watch_Investigation")) );
}

CTask::FSM_Return CTaskWatchInvestigation::StandAndWatchInvestigationPed_OnUpdate( CPed* pPed)
{
	// Ped not visible, follow them
	if (!m_bCanSeeWatchPed)
	{
		SetState(State_FollowInvestigationPed);
		return FSM_Continue;
	}

	pPed->SetDesiredHeading(VEC3V_TO_VECTOR3(m_pWatchPed->GetTransform().GetPosition()));

	if (m_Timer.IsSet() && m_Timer.IsOutOfTime())
	{
		SetState(State_ReturnToOriginalPositionOnFoot);
	}

	return FSM_Continue;
}

//-------------------------------------------------------------------------------------------
// ReturnToOriginalPositionOnFoot
//-------------------------------------------------------------------------------------------

void CTaskWatchInvestigation::ReturnToOriginalPositionOnFoot_OnEnter( CPed* UNUSED_PARAM(pPed))
{
	SetNewTask(rage_new CTaskComplexControlMovement( rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, m_vInitialPosition)));
}

CTask::FSM_Return CTaskWatchInvestigation::ReturnToOriginalPositionOnFoot_OnUpdate( CPed* UNUSED_PARAM(pPed))
{
	if( GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT) )
	{
		SetState(State_Finish);
	}
	return FSM_Continue;
}

#if !__FINAL

const char * CTaskWatchInvestigation::GetStaticStateName( s32 iState  )
{
	taskAssert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_PlayAnim",
		"State_FollowInvestigationPed",
		"State_StandAndWatchInvestigationPed",
		"State_ReturnToOriginalPositionOnFoot",
		"State_Finish",
	};
	return aStateNames[iState];
}

#endif
