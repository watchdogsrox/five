// FILE :    TaskPatrol.cpp
// PURPOSE : Patrol and stand guard tasks 
// AUTHOR :  Phil H
// CREATED : 15-10-2008

// File header
#include "Task/Default/Patrol/TaskPatrol.h"

// Framework headers
#include "ai/task/taskchannel.h"

// Game headers
#include "Core/Game.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Task/Combat/TaskReact.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/Patrol/PatrolRoutes.h"
#include "Task/Default/TaskChat.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/Types/TaskUseScenario.h"

#if !__FINAL
#include "scene/world/GameWorld.h"
#endif // !__FINAL

AI_OPTIMISATIONS()

////////////////////////////
// STAND GUARD			  //
////////////////////////////

float CTaskStandGuardFSM::ms_fDistanceFromPatrolPositionTolerance = 2.0f;

CTaskStandGuardFSM::CTaskStandGuardFSM( const Vector3& vPosition, float fHeading, s32 iScenarioType /*=-1*/)
: 
m_vPosition(vPosition),
m_fHeading(fHeading),
m_ScenarioType(iScenarioType),
m_vLookAtPoint(Vector3::ZeroType),
m_iHighReactionClipDict(0),
m_iLowReactionClipDict(0),
m_lowReactionHelper(/*SF_LowThreatClips*/),
m_highReactionHelper(/*SF_HighThreatClips*/),
m_bTestedForWalls(false),
m_eCurrentProbeDirection(TD_Forward),
m_iTestDirections(0),
m_alertnessResetTimer(CTaskPatrol::ms_fAlertnessResetTime)
{
	m_pChatHelper = CChatHelper::CreateChatHelper();
	//taskAssertf(m_pChatHelper,"Max Number Of Chat Helpers Created");
	SetInternalTaskType(CTaskTypes::TASK_STAND_GUARD_FSM);
}

CTaskStandGuardFSM::~CTaskStandGuardFSM()
{
	if (m_pChatHelper)
	{	
		delete m_pChatHelper;
		m_pChatHelper = NULL;
	}
}

CTask::FSM_Return CTaskStandGuardFSM::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Try to create a new helper if resuming from investigating etc as we delete the original chat helper when the task was aborted
	if (GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		taskAssert(!m_pChatHelper); // Should be NULL
		m_pChatHelper = CChatHelper::CreateChatHelper();
	}

	// Decrease alertness over time
	if (m_alertnessResetTimer.Tick())
	{
		pPed->GetPedIntelligence()->DecreaseAlertnessState();
		m_alertnessResetTimer.Reset();
	}

	// The chat helper handles finding suitable chat targets so that information is available throughout all states
	if (m_pChatHelper)
	{		
		m_pChatHelper->ProcessChat(pPed);
	}

	m_lowReactionHelper.Update(pPed);
	m_highReactionHelper.Update(pPed);

#if !__FINAL
	// Continually check to see if clips have streamed in
	// Only do this for debug.. if we're reacting we'll only need to call this once in the cleanup
	m_iLowReactionClipDict = m_lowReactionHelper.GetSelectedDictionaryIndex();
	m_iHighReactionClipDict = m_highReactionHelper.GetSelectedDictionaryIndex();
#endif // !__FINAL

	return FSM_Continue;
}

CTask::FSM_Return CTaskStandGuardFSM::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

FSM_Begin
	// Entrance state for the task
	FSM_State(State_Start)
		FSM_OnUpdate
			Start_OnUpdate(pPed);

	// Standing at the guard position
	FSM_State(State_StandAtPosition)
		FSM_OnEnter
			StandAtPosition_OnEnter(pPed);
		FSM_OnUpdate
			return StandAtPosition_OnUpdate(pPed);

	// Return to the guard position
	FSM_State(State_ReturnToGuardPosition)
		FSM_OnEnter
			ReturnToGuardPosition_OnEnter(pPed);
		FSM_OnUpdate
			return ReturnToGuardPosition_OnUpdate(pPed);

	// ChatToPed
	FSM_State(State_ChatToPed)
		FSM_OnEnter
			ChatToPed_OnEnter(pPed);
		FSM_OnUpdate
			return ChatToPed_OnUpdate(pPed);

	// Quit
	FSM_State(State_Finish)
		FSM_OnUpdate
			return FSM_Quit;
FSM_End
}

void CTaskStandGuardFSM::CleanUp( )
{
	m_iLowReactionClipDict = m_lowReactionHelper.GetSelectedDictionaryIndex();
	m_iHighReactionClipDict = m_highReactionHelper.GetSelectedDictionaryIndex();

	m_lowReactionHelper.CleanUp();
	m_highReactionHelper.CleanUp();

	if (m_pChatHelper)
	{	
		delete m_pChatHelper;
		m_pChatHelper = NULL;
	}
}

CTask::FSM_Return CTaskStandGuardFSM::Start_OnUpdate( CPed* UNUSED_PARAM(pPed ))
{
	// Debug set up the suspicious scanning here
	//pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_CanInvestigate);
	
	SetState(State_StandAtPosition);
	return FSM_Continue;
}

void CTaskStandGuardFSM::StandAtPosition_OnEnter( CPed* pPed)
{	
	SetNewTask( rage_new CTaskAmbientClips(CTaskAmbientClips::Flag_PlayIdleClips | CTaskAmbientClips::Flag_PlayBaseClip, CScenarioManager::GetConditionalAnimsGroupForScenario(m_ScenarioType) ));
	// Get the end vector for probing and start probing
	m_eCurrentProbeDirection = TD_Forward;
	Vector3 vDirection = GetDirectionVector(m_eCurrentProbeDirection, pPed);
	const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vPositionToEndSearch = vPedPosition + 5.0f * vDirection;
	if (!m_probeTestHelper.IsProbeActive())
	{
		IssueWorldProbe(vPedPosition,vPositionToEndSearch);
	}
	else
	{
		m_probeTestHelper.ResetProbe();
	}
}

void CTaskStandGuardFSM::IssueWorldProbe(const Vector3& vPositionToSearchFrom, Vector3& vPositionToEndSearch)
{
	WorldProbe::CShapeTestProbeDesc probeData;
	probeData.SetStartAndEnd(vPositionToSearchFrom,vPositionToEndSearch);
	probeData.SetContext(WorldProbe::EMovementAI);
	probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
	m_probeTestHelper.StartTestLineOfSight(probeData);
}

CTask::FSM_Return CTaskStandGuardFSM::StandAtPosition_OnUpdate( CPed* pPed )
{
	if (!m_bTestedForWalls)
	{
		// If we're waiting for a sight test try and get the results
		if (m_probeTestHelper.IsProbeActive())
		{
			ProbeStatus probeStatus;
			Vector3 vIntersectionPos;

		    if (m_probeTestHelper.GetProbeResults(probeStatus) )
			{
				// If position is not clear, set the corresponding test direction flag
				if (probeStatus != PS_Clear)
				{
					m_iTestDirections.SetFlag(m_eCurrentProbeDirection);
				}

				// Set the next probe direction
				m_eCurrentProbeDirection = static_cast<TestDirections>(m_eCurrentProbeDirection*2);
				// We've checked all directions, no need for further probes
				if (m_eCurrentProbeDirection == TD_Max_Directions)
				{
					m_bTestedForWalls = true;
					m_probeTestHelper.ResetProbe();
				}
				else
				{
					// Get the end vector for probing and start probing
					Vector3 vDirection = GetDirectionVector(m_eCurrentProbeDirection, pPed);
					const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					Vector3 vPositionToEndSearch = vPedPosition + 5.0f * vDirection;
					IssueWorldProbe(vPedPosition,vPositionToEndSearch);
				}
			}
		}
	}
		
	pPed->SetDesiredHeading(fwAngle::LimitRadianAngle(m_fHeading));

	if (m_pChatHelper)
	{
		if (m_pChatHelper->ShouldGoToChatState())
		{
			m_pChatHelper->SetIsChatting(true);
			SetState(State_ChatToPed);
			return FSM_Continue;
		}
	}

	// Check for being away from position after waiting for 2 seconds
	if( GetTimeInState() > 2.0f &&
		m_vPosition.Dist2(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition())) > rage::square(CTaskStandGuardFSM::ms_fDistanceFromPatrolPositionTolerance) )
	{
		SetState(State_ReturnToGuardPosition);
		return FSM_Continue;
	}

	return FSM_Continue;
}

Vector3 CTaskStandGuardFSM::GetDirectionVector(TestDirections probeDirection, const CPed* pPed)
{
	Matrix34 newMatrix;
	newMatrix.Identity();
	Vector3 vForward;

	// Determine the direction vector for a given direction
	switch (probeDirection)
	{
		case TD_Forward:
			return VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
		case TD_Right:
			return VEC3V_TO_VECTOR3(pPed->GetTransform().GetA());
		case TD_Left:
			return -VEC3V_TO_VECTOR3(pPed->GetTransform().GetA());	
		case TD_Backward:
			return -VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
		case TD_Forward_Left:
 			newMatrix.MakeRotateZ(fwAngle::LimitRadianAngle(PI*0.25f));
 			vForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
 			newMatrix.Transform(vForward);
			return vForward;
		case TD_Forward_Right:
			newMatrix.MakeRotateZ(fwAngle::LimitRadianAngle(-PI*0.25f));
			vForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
			newMatrix.Transform(vForward);
			return vForward;
		case TD_Backward_Left:
			newMatrix.MakeRotateZ(fwAngle::LimitRadianAngle(PI*0.75f));
			vForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
			newMatrix.Transform(vForward);
			return vForward;
		case TD_Backward_Right:
			newMatrix.MakeRotateZ(fwAngle::LimitRadianAngle(-PI*0.75f));
			vForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
			newMatrix.Transform(vForward);
			return vForward;
		default:
			taskAssertf(0,"The probe direction is incorrectly set");
			break;
	}

	return Vector3(0.0f,0.0f,0.0f);
}

void CTaskStandGuardFSM::PlayingScenario_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(m_ScenarioType); //CScenarioManager::GetScenarioInfo(scenarioType);

	if (pScenarioInfo && pScenarioInfo->GetIsClass<CScenarioPlayAnimsInfo>())
	{
		CTaskUseScenario* pScenarioTask = rage_new CTaskUseScenario(m_ScenarioType, (CTaskUseScenario::SF_StartInCurrentPosition | CTaskUseScenario::SF_SkipEnterClip | CTaskUseScenario::SF_CheckConditions) );
		SetNewTask(pScenarioTask);
	}
	else // Handle actual invalid contexts by not creating a scenario
	{
		taskErrorf("Invalid Scenario type being used");
	}
}

CTask::FSM_Return CTaskStandGuardFSM::PlayingScenario_OnUpdate( CPed* UNUSED_PARAM(pPed) )
{
	if( GetIsSubtaskFinished(CTaskTypes::TASK_USE_SCENARIO) )
	{
		SetState(State_StandAtPosition);
	}
	return FSM_Continue;
}

void CTaskStandGuardFSM::ReturnToGuardPosition_OnEnter( CPed* UNUSED_PARAM(pPed) )
{
	SetNewTask( rage_new CTaskComplexControlMovement(rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, m_vPosition ) ) );
}

CTask::FSM_Return CTaskStandGuardFSM::ReturnToGuardPosition_OnUpdate( CPed* UNUSED_PARAM(pPed ))
{
	if( GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT) )
	{
		SetState(State_StandAtPosition);
	}
	return FSM_Continue;
}

void CTaskStandGuardFSM::ChatToPed_OnEnter( CPed* UNUSED_PARAM(pPed ))
{
	if (m_pChatHelper)
	{
		if (m_pChatHelper->ShouldStartTaskAsInitiator()) // This function contains common functionality for tasks using the chat helper
		{
			SetNewTask( rage_new CTaskChat(m_pChatHelper->GetOutGoingChatPed(),CTaskChat::CF_IsInitiator) );
		}
		else
		{
			SetNewTask( rage_new CTaskChat(m_pChatHelper->GetInComingChatPed(),0) );
		}
	}

	// 'Get the other ped's attention'
// 	CChatHelper* pChatHelper = GetChatHelper();
// 	if (pChatHelper)
// 	{
// 		CPed* pInComingChatPed = pChatHelper->GetInComingChatPed();
// 		if (pInComingChatPed)
// 		{
// 			pPed->NewSay("CONV_GANG_STATE", ATSTRINGHASH("M_RUS_FULL_02", 0x6808FFAE), true);
// 			SetNewTask( rage_new CTaskChat(pInComingChatPed, false) );
// 			if (!pPed->GetIkManager().IsLooking())
// 			{
// 				pPed->GetIkManager().LookAt(0, pInComingChatPed, 5000, BONETAG_HEAD, NULL, LF_ONLY_WHILE_IN_FOV);
// 			}
// 		}
// 	}
}

CTask::FSM_Return CTaskStandGuardFSM::ChatToPed_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (m_pChatHelper && GetIsSubtaskFinished(CTaskTypes::TASK_CHAT))
	{
		m_pChatHelper->FinishChat();
		SetState(State_Start);
	}
	else if (!m_pChatHelper || !GetSubTask()) // If we didn't set a chat subtask in OnEnter, go back to patrolling
	{
		SetState(State_Start);
	}

	return FSM_Continue;
}

#if !__FINAL
const char * CTaskStandGuardFSM::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_StandAtPosition",
		"State_ReturnToGuardPosition",
		"State_ChatToPed",
		"State_Finish"
	};

	return aStateNames[iState];
}

void CTaskStandGuardFSM::Debug() const
{
	// Debug chat
	if (m_pChatHelper)
	{
		m_pChatHelper->Debug();
	}
#if DEBUG_DRAW
	for (int i = TD_Forward; i < TD_Max_Directions; i*=2)
	{
		Vector3 vTestVector = GetDirectionVector(static_cast<TestDirections>(i),GetPed());
		Vector3 vPedPos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		if (m_iTestDirections.IsFlagSet(i))
		{
			grcDebugDraw::Line(vPedPos,vPedPos+5.0f*vTestVector, Color_red1, Color_red1);
		}
		else
		{	
			grcDebugDraw::Line(vPedPos,vPedPos+5.0f*vTestVector, Color_green, Color_green);
		}
	}
#endif // DEBUG_DRAW
}
#endif // !__FINAL

////////////////////////////
// PATROL				  //
////////////////////////////

bank_float CTaskPatrol::ms_fMaxTimeBeforeStartingNewHeadLookAt = 5.0f;
bank_float CTaskPatrol::ms_fAlertnessResetTime = 15.0f;		// Decrease alertness by a level after this time

CTaskPatrol::CTaskPatrol( u32 iRouteNameHash, AlertStatus alertStatus, s32 behaviourFlags)
: 
m_iRouteNameHash(iRouteNameHash), 
m_pCurrentPatrolNode(NULL), 
m_patrolDirection(PD_preferForwards), 
m_fTimeAtPatrolNode(0.0f), 
m_eAlertStatus(alertStatus), 
m_behaviourFlags(behaviourFlags), 
m_vLookAtPoint(Vector3(0.0f,0.0f,0.0f)),
m_iLowReactionClipDictionary(0),
m_iHighReactionClipDictionary(0),
m_lowReactionHelper(/*SF_LowThreatClips*/),
m_highReactionHelper(/*SF_HighThreatClips*/),
m_headLookAtTimer(ms_fMaxTimeBeforeStartingNewHeadLookAt),
m_bStartedHeadLookAt(false),
m_pChatHelper(NULL),
m_alertnessResetTimer(ms_fAlertnessResetTime)
{
	if (m_behaviourFlags.IsFlagSet(GF_CanChatToPeds))
	{
		m_pChatHelper = CChatHelper::CreateChatHelper();
	}
	
	//taskAssertf(m_pChatHelper,"Max Number Of Chat Helpers Created");

	// if this task has a valid route hash, increment the number of peds using that route
	if( m_iRouteNameHash != 0 )
	{
		CPatrolRoutes::ChangeNumberPedsUsingRoute(m_iRouteNameHash, +1);
	}
	SetInternalTaskType(CTaskTypes::TASK_PATROL);
}

aiTask* CTaskPatrol::Copy() const
{ 
	s32 iFlags = m_behaviourFlags.GetAllFlags();
	return rage_new CTaskPatrol(m_iRouteNameHash, m_eAlertStatus, iFlags); 
}

CTaskPatrol::~CTaskPatrol()
{
	if (m_pChatHelper)
	{	
		delete m_pChatHelper;
		m_pChatHelper = NULL;
	}

	// if this task has a valid route hash, decrement the number of peds using that route
	if( m_iRouteNameHash != 0 )
	{
		CPatrolRoutes::ChangeNumberPedsUsingRoute(m_iRouteNameHash, -1);
	}
}

void CTaskPatrol::SetState(s32 iState)
{
	AI_LOG_WITH_ARGS("[%s][TaskStateChange] - %s Ped %s is changing state from %s to %s\n", GetTaskName(), AILogging::GetDynamicEntityIsCloneStringSafe(GetPed()), AILogging::GetDynamicEntityNameSafe(GetPed()), GetStaticStateName(GetState()), GetStaticStateName(iState));
	CTaskFSMClone::SetState(iState);
}

void CTaskPatrol::HeadLookAt(CPed* pPed)
{
	static Vector3 vMin(( DtoR * -90.0f), 0.0f, ( DtoR * -180.0f));
	static Vector3 vMax(( DtoR *  90.0f), 0.0f, ( DtoR *  180.0f));

	// Find the closest nearby ped and look at them
	const CPed* pClosestPed = pPed->GetPedIntelligence()->GetClosestPedInRange();
	if (pClosestPed)
	{
		if (pPed->GetIkManager().IsTargetWithinFOV(VEC3V_TO_VECTOR3(pClosestPed->GetTransform().GetPosition()),vMin,vMax))
		{
			pPed->GetIkManager().LookAt(0, pClosestPed, fwRandom::GetRandomNumberInRange(1000,5000), BONETAG_INVALID, NULL);
			m_bStartedHeadLookAt = true;
			m_headLookAtTimer.Reset();
			return;
		}
	}

	float fRandomDist = fwRandom::GetRandomNumberInRange(3.0f, 20.0f);
	s32 iRandomHeading = static_cast<s32>(fwRandom::GetRandomNumberInRange(-90.0f,90.0f));

	// Get the forward vector and rotate it a bit
	Vector3 vForward = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());
	vForward.RotateZ(( DtoR * iRandomHeading));

	m_vLookAtPoint = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + fRandomDist * vForward;

	u32 iRandomLookAtTime = fwRandom::GetRandomNumberInRange(1000,5000);

	if (pPed->GetIkManager().IsTargetWithinFOV(m_vLookAtPoint,vMin,vMax))
	{
		m_bStartedHeadLookAt = true;
		m_headLookAtTimer.Reset();
		pPed->GetIkManager().LookAt(0, NULL, iRandomLookAtTime, BONETAG_INVALID, &m_vLookAtPoint);
	}
}

void CTaskPatrol::CleanUp( )
{
	CPed *pPed = GetPed();
	m_moveGroupClipSetRequestHelper.Release();
	CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
	
	pTask->ResetOnFootClipSet();

	m_iLowReactionClipDictionary = m_lowReactionHelper.GetSelectedDictionaryIndex();
	m_iHighReactionClipDictionary = m_highReactionHelper.GetSelectedDictionaryIndex();

	m_lowReactionHelper.CleanUp();
	m_highReactionHelper.CleanUp();

	if (m_pChatHelper)
	{
		delete m_pChatHelper;
		m_pChatHelper = NULL;
	}
}

CTask::FSM_Return CTaskPatrol::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		taskAssert(!m_pChatHelper);	// Should be NULL
		m_pChatHelper = CChatHelper::CreateChatHelper();
	}

	if (m_alertnessResetTimer.Tick())
	{
		pPed->GetPedIntelligence()->DecreaseAlertnessState();
		m_alertnessResetTimer.Reset();
	}

	// The chat helper handles finding suitable chat targets so that information is available throughout all states
	if (m_behaviourFlags.IsFlagSet(GF_CanChatToPeds) && m_pChatHelper)
	{
		m_pChatHelper->ProcessChat(pPed);
	}

	// Quit if the patrol node is removed
	if( GetState() > State_Start && m_pCurrentPatrolNode.Get() == NULL )
	{
		return FSM_Quit;
	}

	SetPatrollingAnimationGroup(pPed);

	m_lowReactionHelper.Update(pPed);
	m_highReactionHelper.Update(pPed);

#if !__FINAL
	// Continually check to see if clips have streamed in
	// Only do this for debug.. if we're reacting we'll only need to call this once in the cleanup
	m_iLowReactionClipDictionary = m_lowReactionHelper.GetSelectedDictionaryIndex();
	m_iHighReactionClipDictionary = m_highReactionHelper.GetSelectedDictionaryIndex();
#endif // !__FINAL

	return FSM_Continue;
}

#define MAX_NEIGHBOURS_TO_EVALUATE 32

CTask::FSM_Return CTaskPatrol::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		// Entrance state for the task
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter(pPed);
			FSM_OnUpdate
				return Start_OnUpdate(pPed);

		// Moving to the current patrol node
		FSM_State(State_MoveToPatrolNode)
			FSM_OnEnter
				MoveToPatrolNode_OnEnter(pPed);
			FSM_OnUpdate
				return MoveToPatrolNode_OnUpdate(pPed);

		// Chat to a friendly ped
		FSM_State(State_ChatToPed)
			FSM_OnEnter
				ChatToPed_OnEnter(pPed);
			FSM_OnUpdate
				return ChatToPed_OnUpdate(pPed);
		
		// Stand at the current patrol node for the duration specified
		FSM_State(State_StandAtPatrolNode)
			FSM_OnEnter
				StandAtPatrolNode_OnEnter(pPed);
			FSM_OnUpdate
				return StandAtPatrolNode_OnUpdate(pPed);

		// Play the scenario specified at the patrol node
		FSM_State(State_PlayScenario)
			FSM_OnEnter
				PlayScenario_OnEnter(pPed);
			FSM_OnUpdate
				return PlayScenario_OnUpdate(pPed);

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void CTaskPatrol::Start_OnEnter	(CPed* pPed)
{
	// If we're given a patrol task with alert status set, update the ped's alertness accordingly
	if (m_eAlertStatus == AS_alert)
	{
		pPed->GetPedIntelligence()->SetAlertnessState(CPedIntelligence::AS_Alert);
	}
	else
	{
		pPed->GetPedIntelligence()->SetAlertnessState(CPedIntelligence::AS_NotAlert);
	}

	//taskAssertf(pPed->GetPedIntelligence()->GetCombatBehaviour().GetFlags().IsFlagSet(CCombatBehaviour::BF_IsACivilian),"A ped with civilian flag is running a patrol task");
}

CTask::FSM_Return CTaskPatrol::Start_OnUpdate(CPed* pPed)
{
	// Debug set up the suspicious scanning here
	//pPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_CanInvestigate);

	if (!m_pCurrentPatrolNode)
	{
		// Find the correct patrol route
		m_pCurrentPatrolNode = CPatrolRoutes::FindNearestNodeInRoute(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), m_iRouteNameHash);
		if(m_pCurrentPatrolNode)
		{
			AI_LOG_WITH_ARGS("[%s] - %s Start_OnUpdate found node near: (x:%.2f, y:%.2f, z:%.2f) hash: %d\n"
				, GetTaskName()
				, pPed->GetLogName()
				, m_pCurrentPatrolNode->GetPosition().GetX(), m_pCurrentPatrolNode->GetPosition().GetY(), m_pCurrentPatrolNode->GetPosition().GetZ() 
				, m_pCurrentPatrolNode->GetNodeTypeHash());
		}
		else
		{
			AI_LOG_WITH_ARGS("[%s] - %s Start_OnUpdate failed to find node at: (x:%.2f, y:%.2f, z:%.2f)\n"
				, GetTaskName()
				, pPed->GetLogName()
				, pPed->GetTransform().GetPosition().GetXf(), pPed->GetTransform().GetPosition().GetYf(), pPed->GetTransform().GetPosition().GetZf());
		}
	}

	// Quit if node isn't found
	if (!m_pCurrentPatrolNode)
	{	
		SetState(State_Finish);
	}
	else
	{
		SetState(State_MoveToPatrolNode);
	}

	return FSM_Continue;
}

void CTaskPatrol::MoveToPatrolNode_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	Assertf(!m_pCurrentPatrolNode->GetPosition().IsClose(VEC3_ZERO, 0.01f), "Patrol node is located at origin (0,0,0).  This is very likely an error.");

	taskAssertf(m_pCurrentPatrolNode,"Current patrol node doesn't exist");
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_WALK, m_pCurrentPatrolNode->GetPosition(),0.2f);
	//pMoveTask->SetSlideToCoordAtEnd(0);

	// If we're not standing still, then avoid doing a goto-point-stand-still at the end.
	// We hope that the next navmesh route will be calculated quickly enough that the ped will not visible stop at the end of this section.
	if(GetSubTask() && GetSubTask()->GetTaskType()==CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT &&
		((CTaskComplexControlMovement*)GetSubTask())->GetMoveTaskType()==CTaskTypes::TASK_MOVE_FOLLOW_NAVMESH)
		pMoveTask->SetKeepMovingWhilstWaitingForPath(true, 1.0f);
	if(m_pCurrentPatrolNode->GetDuration()==0)
		pMoveTask->SetDontStandStillAtEnd(true);

	SetNewTask( rage_new CTaskComplexControlMovement(pMoveTask));
}

CTask::FSM_Return CTaskPatrol::MoveToPatrolNode_OnUpdate(CPed* pPed)
{
	// Stream in ambient clips for the next patrol node if we have them
	if( GetIsSubtaskFinished(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT) )
	{
		SetState(State_StandAtPatrolNode);
	}

	// If we're not already looking, do a random head look at
	if (m_behaviourFlags.IsFlagSet(GF_UseHeadLookAt))
	{
		if (m_bStartedHeadLookAt)
		{	
			if (!pPed->GetIkManager().IsLooking())
			{
				m_headLookAtTimer.Tick();
				if (m_headLookAtTimer.IsFinished())
				{
					HeadLookAt(pPed);
				}
			}
		}
		else
		{
			HeadLookAt(pPed);
		}
	}

	if (m_behaviourFlags.IsFlagSet(GF_CanChatToPeds) && m_pChatHelper)
	{
		if (m_pChatHelper->ShouldGoToChatState())
		{
			m_pChatHelper->SetIsChatting(true);
			SetState(State_ChatToPed);
		}
	}

	return FSM_Continue;
}

void CTaskPatrol::ChatToPed_OnEnter(CPed* UNUSED_PARAM(pPed))
{
	if (m_pChatHelper)
	{
		if (m_pChatHelper->ShouldStartTaskAsInitiator()) // This function contains common functionality for tasks using the chat helper
		{
			SetNewTask( rage_new CTaskChat(m_pChatHelper->GetOutGoingChatPed(),CTaskChat::CF_IsInitiator | CTaskChat::CF_AutoChat) );
		}
		else
		{
			SetNewTask( rage_new CTaskChat(m_pChatHelper->GetInComingChatPed(),CTaskChat::CF_AutoChat) );
		}
	}
}

CTask::FSM_Return CTaskPatrol::ChatToPed_OnUpdate(CPed* UNUSED_PARAM(pPed))
{
	if (m_pChatHelper && GetIsSubtaskFinished(CTaskTypes::TASK_CHAT))
	{
		m_pChatHelper->FinishChat();
		SetState(State_MoveToPatrolNode);
	}
	else if (!m_pChatHelper || !GetSubTask()) // If we didn't set a chat subtask in OnEnter, go back to patrolling
	{
		SetState(State_MoveToPatrolNode);
	}

	return FSM_Continue;
}

void CTaskPatrol::StandAtPatrolNode_OnEnter(CPed* pPed)
{
	m_fTimeAtPatrolNode = 0.0f;
	if (!pPed->GetIkManager().IsLooking())
	{
		// Look For 3 Seconds
		pPed->GetIkManager().LookAt(0, NULL, m_pCurrentPatrolNode->GetDuration(), BONETAG_INVALID, &m_pCurrentPatrolNode->GetHeading());
	}
}

CTask::FSM_Return CTaskPatrol::StandAtPatrolNode_OnUpdate(CPed* pPed)
{
	pPed->SetDesiredHeading(m_pCurrentPatrolNode->GetPosition() + m_pCurrentPatrolNode->GetHeading());

	// Handle scenario stuff if the node has it
	u32 iNodeTypeHash =  m_pCurrentPatrolNode->GetNodeTypeHash();
	if (iNodeTypeHash != 0)
	{
		s32 scenarioType = CScenarioManager::GetScenarioTypeFromHashKey(iNodeTypeHash);
		const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);

		if (pScenarioInfo && pScenarioInfo->GetIsClass<CScenarioPlayAnimsInfo>())
		{	
			if (pPed->GetDesiredHeading() - pPed->GetTransform().GetHeading() < 0.1f)
			{
				SetState(State_PlayScenario);
				return FSM_Continue;
			}
		}
	}

	// Otherwise after the ped has been at the node for enough time, move to a new location
	m_fTimeAtPatrolNode += fwTimer::GetTimeStep();

	if (m_fTimeAtPatrolNode > m_pCurrentPatrolNode->GetDuration())
	{
		CPatrolNode* pNeighbour = GetNextPatrolNode();

		if( pNeighbour )
		{
			m_pCurrentPatrolNode = pNeighbour;
			AI_LOG_WITH_ARGS("[%s] - %s StandAtPatrolNode_OnUpdate found next node at: (x:%.2f, y:%.2f, z:%.2f) hash: %d\n"
				, GetTaskName()
				, pPed->GetLogName()
				, pNeighbour->GetPosition().GetX(), pNeighbour->GetPosition().GetY(), pNeighbour->GetPosition().GetZ() 
				, m_pCurrentPatrolNode->GetNodeTypeHash());
			SetState(State_MoveToPatrolNode);
		}
		m_fTimeAtPatrolNode = 0;
	}

	if (m_behaviourFlags.IsFlagSet(GF_CanChatToPeds) && m_pChatHelper)
	{
		if (m_pChatHelper->ShouldGoToChatState())
		{
			m_pChatHelper->SetIsChatting(true);
			SetState(State_ChatToPed);
		}
	}

	return FSM_Continue;
}

void CTaskPatrol::PlayScenario_OnEnter (CPed* UNUSED_PARAM(pPed))
{
	u32 iNodeTypeHash =  m_pCurrentPatrolNode ? m_pCurrentPatrolNode->GetNodeTypeHash() : 0;

	if(aiVerifyf(iNodeTypeHash != 0, "CTaskPatrol::PlayScenario_OnEnter: Node Type Hash for PED %s is 0", AILogging::GetDynamicEntityNameSafe(GetPed())))
	{
		s32 scenarioType = CScenarioManager::GetScenarioTypeFromHashKey(iNodeTypeHash);
		const CScenarioInfo* pScenarioInfo = CScenarioManager::GetScenarioInfo(scenarioType);

		if (pScenarioInfo->GetIsClass<CScenarioPlayAnimsInfo>())
		{
			float fProb = CScenarioManager::GetScenarioInfo(scenarioType)->GetSpawnProbability();
			float fNewProb = fwRandom::GetRandomNumberInRange(0.0f,1.0f);
			if (fNewProb < fProb)
			{
				CTaskUseScenario* pScenarioTask = rage_new CTaskUseScenario(scenarioType, CTaskUseScenario::SF_StartInCurrentPosition | CTaskUseScenario::SF_CheckConditions | CTaskUseScenario::SF_CanExitToChain);
				if (CScenarioManager::GetScenarioInfo(scenarioType)->GetTimeTillPedLeaves() < 0.0f)
				{
					pScenarioTask->SetTimeToLeave(static_cast<float>(m_pCurrentPatrolNode->GetDuration()));
				}
				SetNewTask(pScenarioTask);
			}
		}
		else  // Handle actual invalid contexts by not creating a scenario
		{
			taskErrorf("Invalid Scenario type being used");
		}
	}
}

CTask::FSM_Return  CTaskPatrol::PlayScenario_OnUpdate (CPed* pPed)
{
	// If we're running a scenario at this patrol node, do not change state until we've completed it
	// Do not run this code for clones, clones will wait for the owner to change the state
	if (GetIsSubtaskFinished(CTaskTypes::TASK_USE_SCENARIO) && !pPed->IsNetworkClone())
	{
		CPatrolNode* pNeighbour = GetNextPatrolNode();
		if (pNeighbour )
		{
			m_pCurrentPatrolNode = pNeighbour;
			SetState(State_MoveToPatrolNode);
		}
		m_fTimeAtPatrolNode = 0;
	}
	return FSM_Continue;
}

void CTaskPatrol::SetCurrentPatrolNode(Vector3 pos, u32 routeHash)
{
	m_pCurrentPatrolNode = CPatrolRoutes::FindNearestNodeInRoute(pos, routeHash);
}

// Called from task investigate
void CTaskPatrol::MoveToNextPatrolNode()
{
	CPatrolNode* pNeighbour = GetNextPatrolNode();

	if( pNeighbour )
	{
		m_pCurrentPatrolNode = pNeighbour;
	}
}

s32 CTaskPatrol::GetTimeLeftAtNode() const
{
	int iTimeRemainingAtNode = 0;

	if (m_fTimeAtPatrolNode > 0)
	{
		if (m_pCurrentPatrolNode)
		{
			iTimeRemainingAtNode = static_cast<u32>(static_cast<float>(m_pCurrentPatrolNode->GetDuration()) - m_fTimeAtPatrolNode)*1000;
		}
	}
	return iTimeRemainingAtNode;
}

s32 CTaskPatrol::GetCurrentNode() const
{
	s32 iCurrentNode = -1;
	
		if (m_pCurrentPatrolNode)
		{
			iCurrentNode = m_pCurrentPatrolNode->GetNodeId();
		}
	return iCurrentNode;
}


CPatrolNode* CTaskPatrol::GetNextPatrolNode()
{
	// Find the next node to move to
	s32 aPreferredLinks[MAX_NEIGHBOURS_TO_EVALUATE];
	s32 iNumPreferredLinks = 0;
	const s32 iNumLinks = m_pCurrentPatrolNode->GetNumLinks();
	// Build up a list of preferred nodes, these are used at a higher priority so the route navigation is more
	for( s32 i = 0; i < iNumLinks; i++ )
	{
		CPatrolLink* pPatrolLink = m_pCurrentPatrolNode->GetPatrolLink(i);
		if( pPatrolLink )
		{
			// Build up a list of nodes that are in the preferred direction
			// If the end node is different to this one, it must be heading from this node
			if( pPatrolLink->GetDirection() == LD_forwards )
			{
				if( m_patrolDirection == PD_preferForwards )
				{
					aPreferredLinks[iNumPreferredLinks++] = i;
				}
			}
			else
			{
				if( m_patrolDirection == PD_preferBackwards )
				{
					aPreferredLinks[iNumPreferredLinks++] = i;
				}
			}
		}
	}
	CPatrolNode* pNeighbour = NULL;
	// If there are a number of patrol nodes in the prefered direction, choose one of those
	if( iNumPreferredLinks > 0 )
	{
		const s32 iRandomLink = fwRandom::GetRandomNumberInRange(0,iNumPreferredLinks);
		pNeighbour = m_pCurrentPatrolNode->GetLinkedNode(aPreferredLinks[iRandomLink]);
	}
	// No preferred nodes, choose any link at random and reverse the patrol direction as we must have hit a dead end
	else if( iNumLinks > 0 )
	{
		// Reverse the patrol direction as we must have reached a dead end
		if( m_patrolDirection == PD_preferForwards )
			m_patrolDirection = PD_preferBackwards;
		else if( m_patrolDirection == PD_preferBackwards )
			m_patrolDirection = PD_preferForwards;

		const s32 iRandomLink = fwRandom::GetRandomNumberInRange(0,iNumLinks);
		pNeighbour = m_pCurrentPatrolNode->GetLinkedNode(iRandomLink);
	}
	return pNeighbour;
}

void CTaskPatrol::SetPatrollingAnimationGroup( CPed* pPed )
{
	fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
	// Link patrol alert status with ped intelligence alertness

	weaponAssert(pPed->GetWeaponManager());
	if (pPed->GetPedIntelligence()->GetAlertnessState() != CPedIntelligence::AS_NotAlert)
	{
		m_eAlertStatus = AS_alert;
		if (pPed->GetWeaponManager()->GetIsArmed2Handed())
		{
			clipSetId = MOVE_GUARD_ALERT;
		}
		else
		{
			clipSetId = MOVE_PED_BASIC_LOCOMOTION;	// NB: Temporary until a specific clip set is made
		}
	}
	else
	{
		m_eAlertStatus = AS_casual;
		if (pPed->GetWeaponManager()->GetIsArmed2Handed())
		{
			clipSetId = MOVE_GUARD_NOT_ALERT;
		}
		else
		{
			clipSetId = MOVE_PED_BASIC_LOCOMOTION;	// NB: Temporary until a specific clip set is made
		}
	}

	if( m_moveGroupClipSetRequestHelper.Request(clipSetId) && clipSetId != CLIP_SET_ID_INVALID )
	{
		CTaskMotionBase* pTask = pPed->GetPrimaryMotionTask();
		pTask->SetDefaultOnFootClipSet(clipSetId);
	}
}

#if !__FINAL
const char * CTaskPatrol::GetStaticStateName( s32 iState  )
{
	Assert(iState>=State_Start&&iState<=State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_MoveToPatrolNode",
		"State_ChatToFriendlyPed",
		"State_StandAtPatrolNode",
		"State_PlayScenario",
		"State_Finish"
	};

	return aStateNames[iState];
}

void CTaskPatrol::Debug( ) const
{
	// Debug draw look at point
	//DEBUG_DRAW_ONLY(grcDebugDraw::Line(pPed->GetPosition(), m_vLookAtPoint, Color_yellow1, Color_red1));
	if (m_behaviourFlags.IsFlagSet(GF_CanChatToPeds))
	{
		DEBUG_DRAW_ONLY(grcDebugDraw::Text(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()),Color_red1,0,0,"GF_CanChatToPeds"));
	}

#if DEBUG_DRAW
	//TODO: This should be controlled through RAG at some point.
	static bool sShowVisionInfo = false;
	if (CPedDebugVisualiser::GetDebugDisplay() == CPedDebugVisualiser::eTasksFullDebugging && sShowVisionInfo)
	{
		float fPeripheralRange = 0.0f;
		float fFocusRange = 0.0f;
		GetPed()->GetPedIntelligence()->GetPedPerception().CalculatePerceptionDistances(fPeripheralRange, fFocusRange);
		char szBuffer[128];
		sprintf(szBuffer,"Peripheral Range : %.2f\nFocus Range : %.2f", fPeripheralRange, fFocusRange);
		grcDebugDraw::Text(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()),Color_red1,0,10,szBuffer);
	}
#endif // DEBUG_DRAW

	// Debug chat
	if (m_pChatHelper)
	{
		m_pChatHelper->Debug();
	}
}
#endif

CTaskInfo* CTaskPatrol::CreateQueriableState() const
{
	Vector3 currentRoutePos = m_pCurrentPatrolNode ? m_pCurrentPatrolNode->GetPosition() : VEC3_ZERO;
	return rage_new CClonesTaskPatrolInfo(GetState(), m_iRouteNameHash, currentRoutePos, (u8)m_eAlertStatus, m_behaviourFlags);
}

void CTaskPatrol::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	CClonesTaskPatrolInfo* patrolInfo = static_cast<CClonesTaskPatrolInfo*>(pTaskInfo);
	Assert(patrolInfo);
	if (patrolInfo)
	{
		m_eAlertStatus = patrolInfo->GetAlertStatus();
		m_iRouteNameHash = patrolInfo->GetRouteHash();
		m_behaviourFlags.SetAllFlags(patrolInfo->GetPatrolBehaviousFlags());

		m_pCurrentPatrolNode = CPatrolRoutes::FindNearestNodeInRoute(patrolInfo->GetCurrentNodePosition(), m_iRouteNameHash);

		CTaskFSMClone::ReadQueriableState(pTaskInfo);
	}
}

CTask::FSM_Return CTaskPatrol::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if (iEvent == OnUpdate)
	{
		s32 currentState = GetState();
		s32 networkState = GetStateFromNetwork();
		
		if ((networkState != -1) && (networkState > State_Start) && (currentState != networkState) && (currentState > State_Start))
		{
			SetState(networkState);
		}
	}

	CPed* pPed = GetPed();

	FSM_Begin
		// Entrance state for the task
		FSM_State(State_Start)
		FSM_OnEnter
		Start_OnEnter(pPed);
	FSM_OnUpdate
		return Start_OnUpdate(pPed);

	// Chat to a friendly ped
	FSM_State(State_ChatToPed)
		FSM_OnEnter
		ChatToPed_OnEnter(pPed);
	FSM_OnUpdate
		return ChatToPed_OnUpdate(pPed);


	// Play the scenario specified at the patrol node
	FSM_State(State_PlayScenario)
		FSM_OnEnter
		PlayScenario_OnEnter(pPed);
	FSM_OnUpdate
		return PlayScenario_OnUpdate(pPed);

	FSM_State(State_Finish)
		FSM_OnUpdate
		return FSM_Quit;

	FSM_State(State_MoveToPatrolNode)
	FSM_State(State_StandAtPatrolNode)
		return FSM_Continue;
	FSM_End
}

CTaskFSMClone* CTaskPatrol::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	s32 iFlags = m_behaviourFlags.GetAllFlags();

	CTaskPatrol* newTask = rage_new CTaskPatrol(m_iRouteNameHash, m_eAlertStatus, iFlags);

	if (m_pCurrentPatrolNode)
	{
		newTask->SetCurrentPatrolNode(m_pCurrentPatrolNode->GetPosition(), m_iRouteNameHash);
	}

	return newTask;
}

CTaskFSMClone* CTaskPatrol::CreateTaskForLocalPed(CPed* pPed)
{
	// do the same as clone ped
	return CreateTaskForClonePed(pPed);
}

void CClonesTaskPatrolInfo::Serialise(CSyncDataBase& serialiser)
{
	static const int NUM_BITS_ALERT_STATUS = datBitsNeeded<CTaskPatrol::AlertStatus::AS_alert>::COUNT;
	static const int NUM_BITS_PATROL_FLAGS = datBitsNeeded<GuardBehaviourFlags::GF_UseHeadLookAt>::COUNT;
	CSerialisedFSMTaskInfo::Serialise(serialiser);

	SERIALISE_HASH(serialiser, m_iRouteNameHash, "Route Hash");
	SERIALISE_UNSIGNED(serialiser, m_alertStatus, NUM_BITS_ALERT_STATUS, "Alert Status");
	SERIALISE_UNSIGNED(serialiser, m_patrolBehaviourFlags, NUM_BITS_PATROL_FLAGS, "Patrol Flags");
	SERIALISE_POSITION(serialiser, m_currentNodePosition, "Current Route Position");
}
