// FILE :    TaskStealth.cpp
// PURPOSE : Subtask of combat used for AI stealth

// File header
#include "Task/Combat/Subtasks/TaskStealth.h"

// Game includes
#include "Peds/Ped.h"
#include "Peds/Action/ActionManager.h"
#include "Task/Combat/TaskCombatMelee.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskNavMesh.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskStealthKill
//=========================================================================

dev_float CTaskStealthKill::m_fMinKillDistThesholdSq = 0.6f;
dev_float CTaskStealthKill::m_fMinApproachDistThreshold = 0.25f;

CTaskStealthKill::CTaskStealthKill( const CPed* pTarget, const CActionResult* pActionResult, float fDesiredMoveBlendRatio, fwFlags32 iFlags )
: m_pTargetPed( pTarget )
, m_pActionResult( pActionResult ) 
, m_fDesiredMoveBlendRatio( fDesiredMoveBlendRatio )
, m_nFlags( iFlags )
, m_vDesiredWorldPosition( VEC3_ZERO )
, m_fDesiredHeading( 0.0f )
{
	Assertf( m_fDesiredMoveBlendRatio > 0.0, "CTaskStealthKill - desired move blend ratio needs to be > 0.0f" );
	SetInternalTaskType(CTaskTypes::TASK_STEALTH_KILL);
}

CTaskStealthKill::~CTaskStealthKill()
{

}

#if !__FINAL
void CTaskStealthKill::Debug() const
{
	CTask::Debug();
}

const char* CTaskStealthKill::GetStaticStateName(s32 iState)
{
	Assert(iState >= State_Start && iState <= State_Finish);
	static const char* aStateNames[] = 
	{
		"State_Start",
		"State_Decide",
		"State_Approach",
		"State_Streaming",
		"State_Kill",
		"State_Finish"
	};

	return aStateNames[iState];
}
#endif // !__FINAL

void CTaskStealthKill::CalculateTargetPosition( CPed* pPed )
{
	Assert(m_pTargetPed);
	Assert(m_pActionResult);

	m_vDesiredWorldPosition = m_pTargetPed->GetTransform().GetPosition();
	m_fDesiredHeading = m_pTargetPed->GetCurrentHeading();
	CHoming* pHoming = m_pActionResult->GetHoming();
	if( pHoming )
	{
		Vec3V vTargetDirection( VEC3_ZERO );
		int rnBoneCacheIdx = -1;
		if( !CActionManager::TargetTypeGetPosDirFromEntity( m_vDesiredWorldPosition,
															vTargetDirection,
															pHoming->GetTargetType(),
															m_pTargetPed,
															pPed,
															pPed->GetTransform().GetPosition(),
															0.0f,
															rnBoneCacheIdx ) )
			{
				// Default to use the target entity position
				m_vDesiredWorldPosition = m_pTargetPed->GetTransform().GetPosition();
				vTargetDirection = m_vDesiredWorldPosition - pPed->GetTransform().GetPosition();
			}

			Vec3V vMoverDisplacement( VEC3_ZERO );
			pHoming->CalculateHomingWorldOffset( pPed, 
												 m_vDesiredWorldPosition,
												 vTargetDirection,
												 vMoverDisplacement, 
												 m_vDesiredWorldPosition, 
												 m_fDesiredHeading,
												 true,
												 true );
	}
}

void CTaskStealthKill::CleanUp()
{
	CPed* pPed = GetPed();
	Assert( pPed );

	// Tell the ped to sneak up 
	pPed->GetMotionData()->SetUsingStealth( false );

	// Release the equipped weapon anims
	CEquipWeaponHelper::ReleaseEquippedWeaponAnims( *pPed );
}

CTask::FSM_Return CTaskStealthKill::ProcessPreFSM()
{	
	// Ensure the target is valid.
	if( !m_pTargetPed )
	{
		return FSM_Quit;
	}

	// Ensure the action definition is valid.
	if( !m_pActionResult )
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskStealthKill::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed* pPed = GetPed();
	Assert( pPed );

	FSM_Begin
		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate( pPed );

		FSM_State(State_Decide)
			FSM_OnUpdate
				Decide_OnUpdate( pPed );
				
		FSM_State(State_Approach)
			FSM_OnEnter
				Approach_OnEnter();
			FSM_OnUpdate
				return Approach_OnUpdate( pPed );

		FSM_State(State_Streaming)
			FSM_OnUpdate
				Streaming_OnUpdate( pPed );

		FSM_State(State_Kill)
			FSM_OnEnter
				Kill_OnEnter();
			FSM_OnUpdate
				return Kill_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

CTask::FSM_Return CTaskStealthKill::Start_OnUpdate( CPed* pPed )
{
	Assert( m_pTargetPed );
	Assert( m_pActionResult );

	// Tell the main ped to sneak up if this is a stealth kill
	if( GetIsTaskFlagSet( SKTF_ForceStealthMode ) )
		pPed->GetMotionData()->SetUsingStealth( true );

	CalculateTargetPosition( pPed );

	// Request the equipped weapon anims
	CEquipWeaponHelper::RequestEquippedWeaponAnims( *pPed );

	SetState( State_Decide );
	return FSM_Continue;
}

CTask::FSM_Return CTaskStealthKill::Decide_OnUpdate( CPed* pPed )
{
	Vec3V vPedToPosition = m_vDesiredWorldPosition - pPed->GetTransform().GetPosition();
	float fDistSq = MagSquared( vPedToPosition ).Getf();
	if( fDistSq > m_fMinKillDistThesholdSq )
		SetState( State_Approach );
	else 
		SetState( State_Streaming );

	return FSM_Continue;
}

void CTaskStealthKill::Approach_OnEnter( void )
{
	CTaskMoveFollowNavMesh* pMoveTask = rage_new CTaskMoveFollowNavMesh( GetDesiredMoveBlendRatio(), 
																		 VEC3V_TO_VECTOR3( m_vDesiredWorldPosition ),
																		 m_fMinApproachDistThreshold,
																		 CTaskMoveFollowNavMesh::ms_fSlowDownDistance,
																		 -1,
																		 true,
																		 false,
																		 NULL,
																		 CTaskMoveFollowNavMesh::ms_fDefaultCompletionRadius,
																		 false );
	SetNewTask( rage_new CTaskComplexControlMovement( pMoveTask ) );
}

CTask::FSM_Return CTaskStealthKill::Approach_OnUpdate( CPed* pPed )
{
	if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) )
	{
		// Analyze target and make sure we do not need to adjust our approach
		CalculateTargetPosition( pPed );
		SetState( State_Decide );
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskStealthKill::Streaming_OnUpdate( CPed* pPed )
{
	CalculateTargetPosition( pPed );
	Vec3V vPedToPosition = m_vDesiredWorldPosition - pPed->GetTransform().GetPosition();
	float fDistSq = MagSquared( vPedToPosition ).Getf();

	// Only allow a transition to the approach state from here
	if( fDistSq > m_fMinKillDistThesholdSq )
		SetState( State_Approach );

	// Otherwise we are close enough so check to see if anims are streamed
	if( CEquipWeaponHelper::AreEquippedWeaponAnimsStreamedIn( *pPed ) )
	{
		SetState( State_Kill );
	}

	return FSM_Continue;
}

void CTaskStealthKill::Kill_OnEnter( void )
{
	Assert( m_pTargetPed );
	Assert( m_pActionResult );
	SetNewTask( rage_new CTaskMelee( m_pActionResult.Get(), const_cast<CPed*>( m_pTargetPed.Get() ) ) );
}

CTask::FSM_Return CTaskStealthKill::Kill_OnUpdate( void )
{
	if( GetIsFlagSet( aiTaskFlags::SubTaskFinished ) )
	{
		SetState( State_Finish );
	}
	
	return FSM_Continue;
}
