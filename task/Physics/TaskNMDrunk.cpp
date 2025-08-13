// Filename   :	TaskNMDrunk.h
// Description:	Code version of original script drunk routine.

#include "Task/Physics/TaskNMDrunk.h"

#if ENABLE_DRUNK

//---- Include Files ---------------------------------------------------------------------------------------------------------------------------------
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/shapetest.h"

// Framework headers
//#include "fwmaths\Angle.h"

// Game headers
#include "debug\DebugScene.h"
#include "Peds/ped.h"
#include "Task/motion/TaskMotionBase.h"
//----------------------------------------------------------------------------------------------------------------------------------------------------

AI_OPTIMISATIONS()

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMDrunk::Tunables CTaskNMDrunk::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMDrunk, 0x4b5b78d5);
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMDrunk //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMDrunk::CTaskNMDrunk(u32 nMinTime, u32 nMaxTime)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: CTaskNMBehaviour(nMinTime, nMaxTime)
, m_bBalanceFailed(false)
, m_bWasInFixedTurnRange(false)
, m_bFixedTurnRight(false)
, m_iNextChangeTime(0)
, m_fExtraHeading(0.0f)
, m_fExtraPitch(0.0f)
{
#if !__FINAL
	m_strTaskName = "NMDrunk";
#endif // !__FINAL
	SetInternalTaskType(CTaskTypes::TASK_NM_DRUNK);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMDrunk::~CTaskNMDrunk()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMDrunk::StartBehaviour(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	AddTuningSet(&sm_Tunables.m_Start);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMDrunk::ControlBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	/////////////////////////
	// base drunk messages:
	/////////////////////////
	AddTuningSet(&sm_Tunables.m_Base);

	bool bMoving = pPed->GetMotionData()->GetDesiredMbrY()>0.0f;

	if (bMoving)
	{
		/////////////////////////
		// moving drunk messages:
		/////////////////////////
		AddTuningSet(&sm_Tunables.m_Moving);
	}
	else
	{
		/////////////////////////
		// idle drunk messages:
		/////////////////////////
		AddTuningSet(&sm_Tunables.m_Idle);
	}

	CNmMessageList list;
	ApplyTuningSetsToList(list);
	Vector3 headTargetPos(VEC3_ZERO);
	ART::MessageParams& msgBalance = list.GetMessage(atFinalHashString(NMSTR_MSG(NM_BALANCE_MSG)));

	// send some code dependent parameters
	if (bMoving)
	{
		//calculate the true desired movement vector
		Vector3 targetPos(0.0f, pPed->GetMotionData()->GetDesiredMbrY(), 0.0f);
		targetPos.RotateZ(pPed->GetMotionData()->GetDesiredHeading());

		headTargetPos = targetPos;
		headTargetPos += pPed->GetBonePositionCached(BONETAG_HEAD);

		CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
		float desiredHeading = pTask ? pTask->CalcDesiredDirection(): 0.0f;

		if (abs(desiredHeading)>sm_Tunables.m_fMinHeadingDeltaToFixTurn)
		{
			if (!m_bWasInFixedTurnRange)
			{
				m_bFixedTurnRight = desiredHeading > 0.0f ? false : true;
			}

			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_TO_VEL_PROB), 0.0f);
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_LEFT_PROB), m_bFixedTurnRight ? 0.0f : 1.0f);
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_RIGHT_PROB), m_bFixedTurnRight ? 1.0f : 0.0f);
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_TO_TARGET_PROB), 0.0f);

			m_bWasInFixedTurnRange = true;
		}
		else
		{
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_TO_VEL_PROB), 1.0f);
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_LEFT_PROB), 0.0f);
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_RIGHT_PROB), 0.0f);
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_TO_TARGET_PROB), 1.0f);
			m_bWasInFixedTurnRange = false;
		}

		// add some randomization to the leaning movement
		if (fwTimer::GetTimeInMilliseconds()>m_iNextChangeTime)
		{

			m_fExtraHeading = fwRandom::GetRandomNumberInRange(-sm_Tunables.m_fHeadingRandomizationRange, sm_Tunables.m_fHeadingRandomizationRange);
			m_iNextChangeTime = fwTimer::GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange(sm_Tunables.m_iHeadingRandomizationTimeMin, sm_Tunables.m_iHeadingRandomizationTimeMax);
		}

		targetPos.RotateZ(m_fExtraHeading);

		float fLeanRatio = pPed->GetMotionData()->GetDesiredMbrY() / MOVEBLENDRATIO_SPRINT;

		float fSpeedRatio = pPed->GetVelocity().Mag();
		fSpeedRatio = (fSpeedRatio-sm_Tunables.m_fForceRampMinSpeed) / (sm_Tunables.m_fForceRampMaxSpeed - sm_Tunables.m_fForceRampMinSpeed);
		fSpeedRatio = Clamp(fSpeedRatio, 0.0f, 1.0f);

		float forceAmount = Lerp(fSpeedRatio, sm_Tunables.m_fForceLeanInDirectionAmountMax, sm_Tunables.m_fForceLeanInDirectionAmountMin);

		// Request a lean in the direction of motion
		ART::MessageParams& msgLean = list.GetMessage(atFinalHashString(NMSTR_MSG(NM_FORCELEANDIR_MSG)));
		msgLean.addVector3(NMSTR_PARAM(NM_FORCELEANDIR_DIR), targetPos.x, targetPos.y, targetPos.z );
		msgLean.addFloat(NMSTR_PARAM(NM_FORCELEANDIR_LEAN_AMOUNT), Clamp(forceAmount*fLeanRatio, -1.0f, 1.0f));
	}
	else
	{
		Vector3 targetPos(0.0f, 1.0f, 0.0f);
		targetPos.RotateZ(pPed->GetCurrentHeading());

		// add some randomization to the head look
		if (fwTimer::GetTimeInMilliseconds()>m_iNextChangeTime)
		{

			m_fExtraHeading = fwRandom::GetRandomNumberInRange(-sm_Tunables.m_fHeadLookHeadingRandomizationRange, sm_Tunables.m_fHeadLookHeadingRandomizationRange);
			m_fExtraPitch = fwRandom::GetRandomNumberInRange(-sm_Tunables.m_fHeadLookPitchRandomizationRange, sm_Tunables.m_fHeadLookPitchRandomizationRange);
			m_iNextChangeTime = fwTimer::GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange(sm_Tunables.m_iHeadLookRandomizationTimeMin, sm_Tunables.m_iHeadlookRandomizationTimeMax);
		}

		targetPos.RotateX(m_fExtraPitch);
		targetPos.RotateZ(m_fExtraHeading);

		targetPos += pPed->GetBonePositionCached(BONETAG_HEAD);
		headTargetPos = targetPos;

#if __DEV
		if (sm_Tunables.m_bDrawIdleHeadLookTarget)
		{
			grcDebugDraw::Sphere(targetPos, 0.1f, Color_yellow);
		}
#endif //__DEV

		// idle turn
		CTaskMotionBase* pTask = pPed->GetCurrentMotionTask();
		float desiredHeading = pTask ? pTask->CalcDesiredDirection(): 0.0f;

		if (abs(desiredHeading)>sm_Tunables.m_fMinHeadingDeltaToIdleTurn)
		{
			bool bTurnRight = desiredHeading > 0.0f ? false : true;

			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_TO_VEL_PROB), 0.0f);
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_LEFT_PROB), bTurnRight ? 0.0f : 1.0f);
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_RIGHT_PROB), bTurnRight ? 1.0f : 0.0f);
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_TO_TARGET_PROB), 0.0f);
		}
		else
		{
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_TO_VEL_PROB), 1.0f);
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_LEFT_PROB), 0.0f);
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_RIGHT_PROB), 0.0f);
			msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_TURN_TO_TARGET_PROB), 1.0f);
		}
	}

	// if script has specified a head look, use that instead
	if (pPed->GetIkManager().GetLookAtFlags()&LF_FROM_SCRIPT)
	{
		//grab the look at target position
		pPed->GetIkManager().GetLookAtTargetPosition(headTargetPos);
	}

	//add a head look in the move direction
	msgBalance.addVector3(NMSTR_PARAM(NM_BALANCE_LOOKAT_POS_VEC3), headTargetPos.x, headTargetPos.y, headTargetPos.z);

	if (sm_Tunables.m_bUseStayUpright)
	{
		float stayUprightStrength;
		bool bVelocityBased;

		if ((fwTimer::GetTimeInMilliseconds() - m_nStartTime) < sm_Tunables.m_iRunningTimeForVelocityBasedStayupright)
		{
			
			bVelocityBased = false;
			stayUprightStrength = sm_Tunables.m_fStayUprightForceNonVelocityBased;
		}
		else
		{
			if (bMoving)
			{
				// TODO - decrease this over time?
				stayUprightStrength = sm_Tunables.m_fStayUprightForceMoving;
			}
			else
			{
				stayUprightStrength = sm_Tunables.m_fStayUprightForceIdle;
			}

			bVelocityBased = true;
		}

		ART::MessageParams& msgStayUpright = list.GetMessage(atFinalHashString(NMSTR_MSG(NM_STAYUPRIGHT_MSG)));
		msgStayUpright.addBool(NMSTR_PARAM(NM_STAYUPRIGHT_VEL_BASED), bVelocityBased);
		msgStayUpright.addFloat(NMSTR_PARAM(NM_STAYUPRIGHT_FORCE_STRENGTH), stayUprightStrength);
	}

	list.Post(pPed->GetRagdollInst());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMDrunk::FinishConditions(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{

	if (pPed->GetVelocity().z < -sm_Tunables.m_fFallingSpeedForHighFall)
	{
		m_bHasFailed = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_HIGH_FALL;
	}

	if (m_bBalanceFailed)
	{
		return ProcessFinishConditionsBase(pPed, MONITOR_FALL, FLAG_VEL_CHECK|FLAG_QUIT_IN_WATER) || !pPed->IsDrunk();
	}
	else
	{
		return !pPed->IsDrunk();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMDrunk::BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFailure(pPed, pFeedbackInterface);

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		m_bBalanceFailed = true;
	}
}

#endif // ENABLE_DRUNK