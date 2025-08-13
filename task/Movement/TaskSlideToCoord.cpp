#include "TaskSlideToCoord.h"

// rage headers
#include "math/angmath.h"
//game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Task/Animation/TaskAnims.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"

AI_OPTIMISATIONS()

//**********************************
// CTaskSlideToCoord
//

float CTaskSlideToCoord::ms_fPosAccuracy = 0.025f;
// was 0.01f, but this caused visible jittering as it couldn't hit target accurately enough.
// this could be to FP inaccuracy when away from the origin.
float CTaskSlideToCoord::ms_fHeadingAccuracy = 0.05f;
float CTaskSlideToCoord::ms_fSpeedMin = 0.1f;
float CTaskSlideToCoord::ms_fSpeedMax = 2.0f;

CTaskSlideToCoord::CTaskSlideToCoord(const Vector3& vPos, const float fHeading, const float fSpeed) :
  m_vPos(vPos),
  m_fHeading( fwAngle::LimitRadianAngle(fHeading) ),
  m_fHeadingIncrement(0.0f),
  m_fSpeed(fSpeed),
  m_bFirstTime(true),
  m_bRunningClip(false),
  m_vToTarget(0.0f, 0.0f),
  m_fPosAccuracy(ms_fPosAccuracy),
  // vars passed in to CTaskSimpleRunClip subtask
  m_iClipFlags(0),
  m_bClipExtractingVelocity(0),
  m_fClipBlendInDelta(0.0f),
  m_iClipTime(-1),
  m_bClipRunInSequence(false),
  m_bUseZeroVelocityIfGroundPhsyicalIsMoving(false)
{
	m_ClipId.Clear();
	m_ClipSetId.Clear();
	SetInternalTaskType(CTaskTypes::TASK_SLIDE_TO_COORD);
}


CTaskSlideToCoord::CTaskSlideToCoord(
	const Vector3& vPos, const float fHeading,
	const float fSpeed, const char* pClipName, const strStreamingObjectName pClipDictName,
	const s32 flags, const float fBlendDelta, const bool bRunInSequence, const int iTime) :
  m_vPos(vPos),
  m_fHeading( fwAngle::LimitRadianAngle(fHeading) ),
  m_fHeadingIncrement(0.0f),
  m_fSpeed(fSpeed),
  m_bFirstTime(true),
  m_bRunningClip(true),
  m_vToTarget(0.0f, 0.0f),
  m_fPosAccuracy(ms_fPosAccuracy),
  // vars passed in to CTaskSimpleRunClip subtask
  m_iClipFlags(flags),
  m_fClipBlendInDelta(fBlendDelta),
  m_iClipTime(iTime),
  m_bClipRunInSequence(bRunInSequence),
  m_bClipExtractingVelocity(0),
  m_bUseZeroVelocityIfGroundPhsyicalIsMoving(false)
{
	m_ClipId.SetFromString( pClipName );
	m_ClipSetId = (fwMvClipSetId)pClipDictName.GetHash();
	SetInternalTaskType(CTaskTypes::TASK_SLIDE_TO_COORD);
}

CTaskSlideToCoord::CTaskSlideToCoord( const Vector3& vPos, const float fHeading, 
									  const float fSpeed, fwMvClipId clipId, fwMvClipSetId clipSetId, 
									  const float fBlendDelta, const s32 flags, const bool bRunInSequence, const int iTime) 
:	m_vPos(vPos),
	m_fHeading( fwAngle::LimitRadianAngle(fHeading) ),
	m_fHeadingIncrement(0.0f),
	m_fSpeed(fSpeed),
	m_bFirstTime(true),
	m_bRunningClip(true),
	m_vToTarget(0.0f, 0.0f),
	m_fPosAccuracy(ms_fPosAccuracy),
	// vars passed in to CTaskSimpleRunClip subtask
	m_iClipFlags(flags),
	m_fClipBlendInDelta(fBlendDelta),
	m_iClipTime(iTime),
	m_bClipRunInSequence(bRunInSequence),
	m_bClipExtractingVelocity(0),
	m_bUseZeroVelocityIfGroundPhsyicalIsMoving(false)
{
	m_ClipId = clipId;
	m_ClipSetId = clipSetId;
	SetInternalTaskType(CTaskTypes::TASK_SLIDE_TO_COORD);
}

CTaskSlideToCoord::~CTaskSlideToCoord()
{
}

#if !__FINAL
void CTaskSlideToCoord::Debug() const
{
#if DEBUG_DRAW
	grcDebugDraw::Line(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), m_vPos, Color_tomato);
	Matrix34 headingMat;
	headingMat.MakeRotateZ(m_fHeading);
	grcDebugDraw::Line(m_vPos, m_vPos + (headingMat.b*2.0f), Color_BlueViolet);
#endif
}
#endif

CTask::FSM_Return
CTaskSlideToCoord::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Zero extracted velocity, provided we're not using trying to extract it from the clip we're playing
	/*
	if(!m_bClipExtractingVelocity || !GetSubTask())
	{
		pPed->SetAnimatedVelocity(Vector3(0.0f,0.0f,0.0f));
	}
	*/
	// need to only allow the ability to turn, unless a custom heading increment has been provided, in which case we want to do nothing
	pPed->GetMotionData()->SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);

	FSM_Begin
		FSM_State(State_AchieveHeading)
			FSM_OnUpdate
				return StateAchieveHeading_OnUpdate(pPed);
		FSM_State(State_PlayClip)
			FSM_OnEnter
				return StatePlayClip_OnEnter(pPed);
			FSM_OnUpdate
				return StatePlayClip_OnUpdate(pPed);
	FSM_End
}

CTask * CTaskSlideToCoord::CreateClipTask()
{
	Assert( m_ClipId.GetHash() != 0 && m_ClipSetId.GetHash() != 0 );

	// If we have an clip specified then fire up a subtask to play it.
	if( m_ClipId.GetHash() != 0 && m_ClipSetId.GetHash() != 0 )
	{
		CTaskRunClip * pClipTask = rage_new CTaskRunClip(
			m_ClipSetId,
			m_ClipId,
			m_fClipBlendInDelta,
			NORMAL_BLEND_OUT_DELTA,
			m_iClipTime,
			m_iClipFlags );

		// if the task is running on a clone ped we don't want it quitting out early because it is not in the queriable state
		pClipTask->SetRunningLocally(true);

		return pClipTask;
	}
	return NULL;
}


CTask::FSM_Return
CTaskSlideToCoord::StateAchieveHeading_OnUpdate(CPed * pPed)
{
	if(m_bFirstTime)
	{
		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		m_vToTarget.x = m_vPos.x - vPedPosition.x;
		m_vToTarget.y = m_vPos.y - vPedPosition.y;

		//Get the ped to stand still.
		pPed->GetMotionData()->SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);
		if(pPed->IsPlayer())
		{
			// Abort the default task, but only if the default task slot is not the active one - or we'll abort ourselves.
			if(pPed->GetPedIntelligence()->GetActiveTaskPriority() != PED_TASK_PRIORITY_DEFAULT)
				pPed->GetPedIntelligence()->GetTaskDefault()->MakeAbortable( CTask::ABORT_PRIORITY_IMMEDIATE, NULL);
		}

		m_bFirstTime = false;
	}

	// If we have a user-specified heading increment, then apply it every frame until we've reached the target heading.
	if(m_fHeadingIncrement != 0.0f)
	{
		float fCurrHeading = pPed->GetCurrentHeading();
		const float fHeadingBefore = fCurrHeading;
		const float fTheta = SubtractAngleShorter(fCurrHeading, m_fHeading);
		fCurrHeading += m_fHeadingIncrement * fwTimer::GetTimeStep() * Sign(-fTheta);

		if((fHeadingBefore <= m_fHeading && fCurrHeading >= m_fHeading) ||
			(fHeadingBefore >= m_fHeading && fCurrHeading <= m_fHeading))
		{
			m_fHeadingIncrement = 0.0f;
			fCurrHeading = m_fHeading;
		}
		fCurrHeading = fwAngle::LimitRadianAngle(fCurrHeading);
		if (!pPed->GetUsingRagdoll())
			pPed->SetHeading(fCurrHeading);
		
		pPed->SetDesiredHeading(fCurrHeading);
	}
	else
	{
		pPed->SetDesiredHeading(m_fHeading);
	}

	float fHeadingDiff = m_fHeading - pPed->GetCurrentHeading();
	fHeadingDiff = fwAngle::LimitRadianAngle(fHeadingDiff);
	if(rage::Abs(fHeadingDiff) < ms_fHeadingAccuracy)
	{
		SetState(State_PlayClip);
	}

	// tell the ped we need to call ProcesPhysics for this task
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

	return FSM_Continue;
}

CTask::FSM_Return
CTaskSlideToCoord::StatePlayClip_OnEnter(CPed * UNUSED_PARAM(pPed))
{
	if( m_ClipId.GetHash() != 0 && m_ClipSetId.GetHash() != 0 )
	{
		CTask * pClipTask = CreateClipTask();
		SetNewTask(pClipTask);
	}
	else
	{
		SetNewTask(NULL);
	}

	return FSM_Continue;
}

CTask::FSM_Return
CTaskSlideToCoord::StatePlayClip_OnUpdate(CPed * pPed)
{
	Assert(!GetSubTask() || (GetSubTask()->GetTaskType()==CTaskTypes::TASK_RUN_CLIP));

	const bool bFinishedClip = (GetSubTask() && GetIsFlagSet(aiTaskFlags::SubTaskFinished));

	if(!m_TaskTimer.IsSet())
	{
		if(!m_bRunningClip)
		{
			// Not running an clip. 
			// Set maximum for the slide.
			m_TaskTimer.Set(fwTimer::GetTimeInMilliseconds(), 8000);
		}
		else if(m_bRunningClip && bFinishedClip)
		{
			// Running an clip that has finished.
			// Set maximum for the slide to run post-clip.
			m_TaskTimer.Set(fwTimer::GetTimeInMilliseconds(), 500);
		}
	}

	Vector3 vDiff = m_vPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vDiff.z = 0;
	
	if(m_TaskTimer.IsSet() && m_TaskTimer.IsOutOfTime())
	{
		return FSM_Quit;
	}
		
	//If the clip is finished then check if the position and heading are complete.
	if(bFinishedClip || !m_bRunningClip)
	{
		if(vDiff.Mag2() < (m_fPosAccuracy*m_fPosAccuracy))
		{
			return FSM_Quit;
		}	
	}

	// Tell the ped we need to call ProcesPhysics for this task
	// Although not in this state if the clip is extracting velocity

	m_bClipExtractingVelocity = false;

	if(GetSubTask())
	{
		if (GetSubTask()->GetTaskType()==CTaskTypes::TASK_RUN_CLIP)
		{
			CTaskRunClip * pClipTask = static_cast<CTaskRunClip*>(GetSubTask());
			if (pClipTask->GetClipHelper() && (pClipTask->GetClipHelper()->ContributesToLinearMovement() || pClipTask->GetClipHelper()->ContributesToAngularMovement()))
			{
				m_bClipExtractingVelocity = true;
			}
		}
	}

	if (!m_bClipExtractingVelocity)
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

#if FPS_MODE_SUPPORTED
	if (pPed->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	}
#endif	//FPS_MODE_SUPPORTED

	return FSM_Continue;
}

bool CTaskSlideToCoord::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	CPed *pPed = GetPed(); //Get the ped ptr.
	if( GetUseZeroVelocityIfGroundPhsyicalIsMoving() )
	{
		CPhysical* pGroundPhysical = pPed->GetGroundPhysical();
		if( pGroundPhysical )
		{
			Vector3 vGroundPhysicalVelocity = pGroundPhysical->GetVelocity();
			if( vGroundPhysicalVelocity.Mag2() > rage::square( 0.1f ) )
			{
				Vector3 vDesiredVel = pPed->GetAnimatedVelocity();

				//Account for the ground.
				if( pPed->GetGroundPhysical() )
					vDesiredVel += VEC3V_TO_VECTOR3( pPed->GetGroundVelocityIntegrated() );

				pPed->SetDesiredVelocity( vDesiredVel );
				return true;
			}
		}
	}

	if (GetState() == State_AchieveHeading && m_fHeadingIncrement == 0.f)
	{
		float fDesiredHeadingChange =  SubtractAngleShorter(
			fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetDesiredHeading()), 
			fwAngle::LimitRadianAngleSafe(pPed->GetMotionData()->GetCurrentHeading()));

		static const float fSlideHeadingThreshold = 0.4f;

		if (IsNearZero(fDesiredHeadingChange))
		{
			pPed->SetDesiredAngularVelocity(VEC3_ZERO);
		}
		else if (abs(fDesiredHeadingChange) < fSlideHeadingThreshold)	// Just slide last bit to speed things up
		{
			float fRate = 2.f;
			float fRotateSpeed;

			// try not to overshoot
			if ((fRate*fTimeStep) > abs(fDesiredHeadingChange))
				fRotateSpeed = abs(fDesiredHeadingChange) / fTimeStep;
			else
				fRotateSpeed = fRate;

			if (fDesiredHeadingChange < 0.0f)
				fRotateSpeed *= -1.0f;

			pPed->SetDesiredAngularVelocity(Vector3(0.0f, 0.0f, fRotateSpeed));
		}
	}

	return ProcessSlidePhysics(m_vPos, pPed, fTimeStep, m_fSpeed, m_fPosAccuracy);
}

bool
CTaskSlideToCoord::ProcessSlidePhysics(const Vector3 & vTargetPos, CPed* pPed, const float fTimeStep, const float fDesiredSpeed, const float fDesiredAccuracy)
{
	Vector3 vecDelta = vTargetPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vecDelta.z = 0;

#if __DEV
	static bool bDebug = false;
	if(bDebug)
	{
		Printf("CTaskSlideToCoord : XY distance to target = %.3f, Required distance = %.3f\n", vecDelta.Mag(), fDesiredAccuracy);
	}
#endif

	// Set the extracted velocity of the ped so safely get him to the target point.
	if(vecDelta.Mag2() >= fDesiredAccuracy*fDesiredAccuracy)
	{
		const float fDist = vecDelta.Mag();

		// apply some basic clamps to the speed we're using to slide
		float fSpeed = Clamp(fDesiredSpeed, CTaskSlideToCoord::ms_fSpeedMin, CTaskSlideToCoord::ms_fSpeedMax);

		// apply velocity clamps to ensure we can slow down in time
		static dev_float fTweakVal = 0.8f;	// was 0.9f but peds jittered horribly
		float fMaxSpeedToStop = fTweakVal * pPed->GetCurrentMotionTask()->GetStdVelChangeLimit() * fDist;
		fMaxSpeedToStop = rage::Sqrtf(fMaxSpeedToStop);
		if(fMaxSpeedToStop < fSpeed)
			fSpeed = fMaxSpeedToStop;

		// apply velocity clamp to make sure we don't overshoot
		static dev_float fTweakOvershootVal = 0.9f;
		if(fTweakOvershootVal*fSpeed*fTimeStep > fDist)
			fSpeed = fTweakOvershootVal*fDist/fTimeStep;

		// normalise the delta and scale by the desired speed
		Vector3 vecDesiredSpeed;
		vecDesiredSpeed.Scale(vecDelta, fSpeed / fDist);

#if __DEV
		if(bDebug)
		{
			Printf("CTaskSlideToCoord :   fSpeed = %f fMaxSpeedToStop = %f\n", fSpeed, fMaxSpeedToStop);
		}
#endif

		Vector3 vCurrVel = pPed->GetDesiredVelocity();
		vCurrVel.x = vecDesiredSpeed.x;
		vCurrVel.y = vecDesiredSpeed.y;
		pPed->SetPedResetFlag(CPED_RESET_FLAG_ApplyVelocityDirectly, true);
		pPed->SetDesiredVelocityClamped(vCurrVel,CTaskMotionBase::ms_fAccelLimitStd*fTimeStep);
		return true;
	}
	// If inside of completion range then set extracted velocity to zero - we've finished
	else
	{
		pPed->SetDesiredVelocityClamped(VEC3_ZERO, CTaskMotionBase::ms_fAccelLimitStd*fTimeStep);
		return false;
	}
}


void CTaskSlideToCoord::RemoveShuffleStopClip(CPed* UNUSED_PARAM(pPed))
{
	/*CClipPlayer * pShuffle = pPed->GetClipBlender()->GetFirstClipPlayerByPriority(AP_MEDIUM);
	while(pShuffle)
	{
		if(pShuffle->GetClipId()==CLIP_MOVE_SHUFFLESTOP)
			pShuffle->SetBlendDelta(-4.0f);

		pShuffle = pPed->GetClipBlender()->GetNextClipPlayerByPriority(AP_MEDIUM);
	}*/
}

 void CTaskSlideToCoord::CleanUp()
 {
 }

 aiTask* CTaskSlideToCoord::Copy() const 
 {
	 CTaskSlideToCoord * pClone;
	 if(m_bRunningClip)
	 {
		 pClone = rage_new CTaskSlideToCoord(m_vPos, m_fHeading, m_fSpeed, m_ClipId, m_ClipSetId, m_fClipBlendInDelta, m_iClipFlags, m_bClipRunInSequence, m_iClipTime );
	 }
	 else
	 {
		 pClone = rage_new CTaskSlideToCoord(m_vPos, m_fHeading, m_fSpeed);			
	 }
	 pClone->SetHeadingIncrement(m_fHeadingIncrement);
	 pClone->SetPosAccuracy(GetPosAccuracy());
	 return pClone;
 }

const CClipHelper* CTaskSlideToCoord::GetSubTaskClip( void )
{
	CTask* pSubTask = GetSubTask();
	if( pSubTask )
	{
		return pSubTask->GetClipHelper();
	}

	return NULL;
}

CTaskInfo* CTaskSlideToCoord::CreateQueriableState() const
{
	return rage_new CClonedSlideToCoordInfo(m_vPos, m_fHeading, m_fSpeed, m_ClipSetId, m_ClipId);
}

//*****************************************************************
//	CTaskMoveSlideToCoord.
//	This is a cut-down version of CTaskSlideToCoord, which
//	will run in the movement task tree.
//*****************************************************************

CTaskMoveSlideToCoord::CTaskMoveSlideToCoord(const Vector3 & vPos, const float fHeading, const float fSpeed, const float fHeadingInc, const int iTime)
: CTaskMove(MOVEBLENDRATIO_STILL),
	m_vPos(vPos),
	m_fHeading( fwAngle::LimitRadianAngle(fHeading) ),
	m_fHeadingIncrement(fHeadingInc),
	m_fSpeed(fSpeed),
	m_bFirstTime(true),
	m_iTimer(iTime),
	m_vToTarget(0.0f,0.0f),
	m_fPosAccuracy(CTaskSlideToCoord::ms_fPosAccuracy)
{
	SetInternalTaskType(CTaskTypes::TASK_MOVE_SLIDE_TO_COORD);
}


CTaskMoveSlideToCoord::~CTaskMoveSlideToCoord()
{
}

#if !__FINAL
void CTaskMoveSlideToCoord::Debug() const
{
#if DEBUG_DRAW
	grcDebugDraw::Line(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), m_vPos, Color_tomato);

	Matrix34 headingMat;
	headingMat.MakeRotateZ(m_fHeading);
	grcDebugDraw::Line(m_vPos, m_vPos + (headingMat.b*2.0f), Color_BlueViolet);
#endif
}
#endif

CTask::FSM_Return CTaskMoveSlideToCoord::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(Running)
			FSM_OnUpdate
				return StateRunning_OnUpdate(pPed);
	FSM_End
}

CTask::FSM_Return CTaskMoveSlideToCoord::StateRunning_OnUpdate(CPed * pPed)
{
	//pPed->SetAnimatedVelocity(Vector3(0.0f,0.0f,0.0f));

	// need to only allow the ability to turn, unless a custom heading increment has been provided, in which case we want to do nothing
	pPed->GetMotionData()->SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);

	//First time round set the desired heading and remove any move clips.
	if(m_bFirstTime)
	{
		if(m_iTimer != -1)
			m_iTimer += fwTimer::GetTimeInMilliseconds();

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		m_vToTarget.x = m_vPos.x - vPedPosition.x;
		m_vToTarget.y = m_vPos.y - vPedPosition.y;

		//Get the ped to stand still.
		pPed->GetMotionData()->SetDesiredMoveBlendRatio(MOVEBLENDRATIO_STILL);
		if(pPed->IsPlayer())
		{
			// Abort the default task, but only if the default task slot is not the active one - or we'll abort ourselves.
			if(pPed->GetPedIntelligence()->GetActiveTaskPriority() != PED_TASK_PRIORITY_DEFAULT)
				pPed->GetPedIntelligence()->GetTaskDefault()->MakeAbortable(CTask::ABORT_PRIORITY_IMMEDIATE,0);
		}

		//Only do this bit once.
		m_bFirstTime = false;
	}

	// If we have a user-specified heading increment, then apply it every frame until we've reached the target heading.
	if(m_fHeadingIncrement != 0.0f)
	{
		float fCurrHeading = pPed->GetCurrentHeading();
		float fHeadingBefore = fCurrHeading;
		float fTheta = SubtractAngleShorter(fCurrHeading, m_fHeading);
		fCurrHeading += m_fHeadingIncrement * fwTimer::GetTimeStep() * Sign(-fTheta);

		if((fHeadingBefore <= m_fHeading && fCurrHeading >= m_fHeading) ||
			(fHeadingBefore >= m_fHeading && fCurrHeading <= m_fHeading))
		{
			m_fHeadingIncrement = 0.0f;
			fCurrHeading = m_fHeading;
		}
		fCurrHeading = fwAngle::LimitRadianAngle(fCurrHeading);
		pPed->SetHeading(fCurrHeading);
		pPed->SetDesiredHeading(fCurrHeading);
	}
	else
	{
		pPed->SetDesiredHeading(m_fHeading);
	}

	Vector3 vDiff = m_vPos - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	vDiff.z = 0;

	if(m_iTimer != -1 && m_iTimer < (s32)fwTimer::GetTimeInMilliseconds())
	{
		float fHeadingDiff = m_fHeading - pPed->GetCurrentHeading();
		fHeadingDiff=fwAngle::LimitRadianAngle(fHeadingDiff);
		if(rage::Abs(fHeadingDiff)<CTaskSlideToCoord::ms_fHeadingAccuracy)
		{
			CTaskSlideToCoord::RemoveShuffleStopClip(pPed);
			return FSM_Quit;
		}
	}

	if(vDiff.Mag2() < (m_fPosAccuracy*m_fPosAccuracy))
	{
		float fHeadingDiff = m_fHeading - pPed->GetCurrentHeading();
		fHeadingDiff=fwAngle::LimitRadianAngle(fHeadingDiff);
		if(rage::Abs(fHeadingDiff)<CTaskSlideToCoord::ms_fHeadingAccuracy)
		{
			CTaskSlideToCoord::RemoveShuffleStopClip(pPed);
			return FSM_Quit;
		}
	}	

	// tell the ped we need to call ProcesPhysics for this task
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );

	return FSM_Continue;
}

void CTaskMoveSlideToCoord::CleanUp()
{
}

bool CTaskMoveSlideToCoord::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	CPed *pPed = GetPed(); //Get the ped ptr.


	return CTaskSlideToCoord::ProcessSlidePhysics(m_vPos, pPed, fTimeStep, m_fSpeed, m_fPosAccuracy);
}

void CTaskMoveSlideToCoord::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	CTaskSlideToCoord::RemoveShuffleStopClip(pPed);

	// Base class
	CTaskMove::DoAbort(iPriority, pEvent);
}



aiTask* CTaskMoveSlideToCoord::Copy() const 
{
	CTaskMoveSlideToCoord * pClone = rage_new CTaskMoveSlideToCoord(m_vPos, m_fHeading, m_fSpeed, m_fHeadingIncrement, m_iTimer);
	pClone->SetPosAccuracy(GetPosAccuracy());
	return pClone;
}
