// Filename   :	TaskNMFallDown.cpp
// Description:	Natural Motion fall down class (FSM version)

// --- Include Files ------------------------------------------------------------

// C headers
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
#include "fwanimation/animmanager.h"
#include "fwanimation/pointcloud.h"
#include "grcore/debugdraw.h"
#include "fwmaths\Angle.h"

// Game headers
#include "camera/CamInterface.h"
#include "Event\EventDamage.h"
#include "Network\NetworkInterface.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Peds\PedPlacement.h"
#include "PedGroup\PedGroup.h"
#include "Physics\GtaInst.h"
#include "Physics\Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task/Physics/TaskNMFallDown.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CClonedNMFallDownInfo
//////////////////////////////////////////////////////////////////////////

CClonedNMFallDownInfo::CClonedNMFallDownInfo(u32 nFallType, const Vector3& fallDirection, float fGroundHeight, bool bForceFatal)
: m_nFallType(nFallType)
, m_fallDirection(fallDirection)
, m_fGroundHeight(fGroundHeight)
, m_bForceFatal(bForceFatal)
{

}

CClonedNMFallDownInfo::CClonedNMFallDownInfo()
: m_nFallType(0)
, m_fallDirection(Vector3(0.0f, 0.0f, 0.0f))
, m_fGroundHeight(0.0f)
, m_bForceFatal(false)
{

}

CTaskFSMClone *CClonedNMFallDownInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMFallDown(10000, 10000, static_cast<CTaskNMFallDown::eNMFallType>(m_nFallType), m_fallDirection, m_fGroundHeight, NULL, NULL, VEC3_ZERO, m_bForceFatal);
}

//////////////////////////////////////////////////////////////////////////
// CTaskNMFallDown
//////////////////////////////////////////////////////////////////////////

CTaskNMFallDown::CTaskNMFallDown(u32 nMinTime, u32 nMaxTime, eNMFallType nFallType, const Vector3& vecDirn, float fGroundHeight,
								 CEntity* pEntityResponsible, const CGrabHelper* UNUSED_PARAM(pGrabHelper), const Vector3 &vecWallPosition, bool bForceFatal)
:	CTaskNMBehaviour(nMinTime, nMaxTime),
m_nFallType(nFallType),
m_nFallState(STATE_READY),
m_vecDirn(vecDirn),
m_fGroundHeight(fGroundHeight),
m_fStartHeight(0.0f),
m_nFailBalanceTime(0),
m_pEntityResponsible(pEntityResponsible),
//m_GrabHelper(pGrabHelper),
m_vecWallPos(vecWallPosition),
m_bForceFatal(bForceFatal)
{
	SetInternalTaskType(CTaskTypes::TASK_NM_FALL_DOWN);
}

CTaskNMFallDown::~CTaskNMFallDown()
{
}

void CTaskNMFallDown::BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFailure(pPed, pFeedbackInterface);

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		m_nFailBalanceTime = fwTimer::GetTimeInMilliseconds();
	}
}

dev_u32 snFallDownMaxSteps = 10;
dev_u32 snFallDownMaxStepsStairs = 5;

dev_float sfFallDownBalAbort = 0.7f;
dev_float sfFallDownBalAbortStairs = 0.55f;

dev_float sfFallDownBalStepClamp = 0.6f;
dev_float sfFallDownBalStepClampStairs = 0.6f;

dev_float sfFallDownLean = 0.45f;
dev_float sfFallDownLeanBack = 0.25f;
dev_float sfFallDownLeanOverWall = 0.25f;

dev_float sfFallDownImpulse = 0.6f;
dev_float sfFallDownImpulseBack = 0.2f;

dev_float sfFallDownOverWallForceMag = 0.15f;
dev_float sfFallDownOverWallMaxDistToHitPoint = 0.25f;
//
void CTaskNMFallDown::StartBehaviour(CPed* pPed)
{
	bool bStairs = m_nFallType==TYPE_DOWN_STAIRS || m_nFallType==TYPE_DIE_DOWN_STAIRS;
	bool bWall = m_nFallType==TYPE_OVER_WALL || m_nFallType==TYPE_DIE_OVER_WALL;

	float fForwardMult = 1.0f;
	Matrix34 ragdollCompMatrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());
	pPed->GetRagdollComponentMatrix(ragdollCompMatrix, RAGDOLL_SPINE0);
	Vector3 vecFwd(-ragdollCompMatrix.c);
	vecFwd.z = 0.0f;
	vecFwd.NormalizeSafe();
	fForwardMult = 0.5f * (m_vecDirn.Dot(vecFwd) + 1.0f);

	ART::MessageParams msgBalance;
	msgBalance.addBool(NMSTR_PARAM(NM_START), true);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_BALANCE_MSG), &msgBalance);

	ART::MessageParams msgConfigBalance;
	msgConfigBalance.addBool(NMSTR_PARAM(NM_START), true);
	msgConfigBalance.addInt(NMSTR_PARAM(NM_CONFIGURE_BALANCE_MAX_STEPS), bStairs ? snFallDownMaxStepsStairs : snFallDownMaxSteps);
	msgConfigBalance.addFloat(NMSTR_PARAM(NM_CONFIGURE_BALANCE_ABORT_THRESHOLD), bStairs ? sfFallDownBalAbort : sfFallDownBalAbortStairs);
	msgConfigBalance.addFloat(NMSTR_PARAM(NM_CONFIGURE_BALANCE_STEP_CLAMP_SCALE), bStairs ? sfFallDownBalStepClamp : sfFallDownBalStepClampStairs);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_CONFIGURE_BALANCE_MSG), &msgConfigBalance);

	ART::MessageParams msgBalanceLean;
	msgBalanceLean.addBool(NMSTR_PARAM(NM_START), true);
	msgBalanceLean.addVector3(NMSTR_PARAM(NM_BALANCE_LEAN_DIR_VEC3), m_vecDirn.x, m_vecDirn.y, m_vecDirn.z);
	// lean different amounts forward vs back
	float fLeanAmount = fForwardMult * (bWall ? sfFallDownLeanOverWall : sfFallDownLean) + (1.0f - fForwardMult) * sfFallDownLeanBack;
	msgBalanceLean.addFloat(NMSTR_PARAM(NM_BALANCE_LEAN_DIR_AMOUNT), fLeanAmount);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_BALANCE_LEAN_DIR_MSG), &msgBalanceLean);

	if(m_nFallType==TYPE_OVER_WALL || m_nFallType==TYPE_DIE_OVER_WALL)
	{
		// m_vecWallPos stores the position of the detected wall at knee height. Work out an orthonormal vector and
		// construct a short line segment to pass to the NM behaviour which will use the line to compute the closest point
		// on the wall anyway.
		Vector3 vOrthonormal = m_vecDirn;
		vOrthonormal.Cross(ZAXIS);
		vOrthonormal.Normalize();
		Vector3 vWallEndA = m_vecWallPos;
		Vector3 vWallEndB = m_vecWallPos;
		// This is where we move a little along the direction orthogonal to m_vecDirn to create the line segment.
		const float fDisplacement = 0.5f; // 50cm
		Vector3 vOrthoRight = vOrthonormal;
		Vector3 vOrthoLeft = vOrthonormal;
		vOrthoRight.Scale(fDisplacement);
		vOrthoLeft.Scale(-1.0f*fDisplacement);
		vWallEndA.Add(vOrthoRight);
		vWallEndB.Add(vOrthoLeft);

#if DEBUG_DRAW
		taskAssert(GetParent());
		taskAssertf(dynamic_cast<CTaskNMControl*>(GetParent()), "NM Behaviour tasks must have CTaskNMControl as an immediate parent.");
		smart_cast<CTaskNMControl*>(GetParent())->AddDebugSphere(RCC_VEC3V(vWallEndA), 0.15f, Color32(100,100,100));
		smart_cast<CTaskNMControl*>(GetParent())->AddDebugSphere(RCC_VEC3V(vWallEndB), 0.15f, Color32(100,100,100));
		smart_cast<CTaskNMControl*>(GetParent())->AddDebugLine(RCC_VEC3V(vWallEndA), RCC_VEC3V(vWallEndB), Color32(200,200,100));		

#endif //DEBUG_DRAW

		ART::MessageParams msgFallOverWall;
		msgFallOverWall.addBool(NMSTR_PARAM(NM_START), true);
		msgFallOverWall.addFloat(NMSTR_PARAM(NM_FALLOVER_WALL_FORCE_MAG), sfFallDownOverWallForceMag);
		msgFallOverWall.addFloat(NMSTR_PARAM(NM_FALLOVER_WALL_MAX_DIST_TO_HIT_POINT), sfFallDownOverWallMaxDistToHitPoint);
		msgFallOverWall.addVector3(NMSTR_PARAM(NM_FALLOVER_WALL_FALL_OVER_WALL_END_A), vWallEndA.x, vWallEndA.y, vWallEndA.z);
		msgFallOverWall.addVector3(NMSTR_PARAM(NM_FALLOVER_WALL_FALL_OVER_WALL_END_B), vWallEndB.x, vWallEndB.y, vWallEndB.z);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_FALLOVER_WALL_MSG), &msgFallOverWall);
	}

	// apply an initial impulse to push the ped toward the edge or whatever
	Vector3 vecImpulse(m_vecDirn);
	// use different impulse magnitudes forward vs back
	float fImpulse = fForwardMult * sfFallDownImpulse + (1.0f - fForwardMult) * sfFallDownImpulse;
	vecImpulse.Scale(fImpulse * pPed->GetMass());

	pPed->ApplyImpulse(vecImpulse, VEC3_ZERO, RAGDOLL_SPINE3, true);

	pPed->GetRagdollComponentMatrix(ragdollCompMatrix, RAGDOLL_SPINE0);
	m_fStartHeight = ragdollCompMatrix.d.z;

	m_nFailBalanceTime = 0;
	m_nFallState = STATE_BALANCE;
}


dev_float sfFallDownForce = 0.9f;
dev_float sfFallDownForceBack = 0.3f;
//
void CTaskNMFallDown::ControlBehaviour(CPed* pPed)
{
	switch(m_nFallState)
	{
	case STATE_BALANCE:
		{
			float fForwardMult = 1.0f;
			Matrix34 ragdollCompMatrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());
			pPed->GetRagdollComponentMatrix(ragdollCompMatrix, RAGDOLL_SPINE0);
			Vector3 vecFwd(-ragdollCompMatrix.c);
			vecFwd.z = 0.0f;
			vecFwd.NormalizeSafe();
			fForwardMult = 0.5f * (m_vecDirn.Dot(vecFwd) + 1.0f);

			pPed->GetRagdollComponentMatrix(ragdollCompMatrix, RAGDOLL_SPINE0);

			// different conditions for going to next stage for 3 different fall types
			bool bGoToFalling = false;
			switch(m_nFallType)
			{
			case TYPE_FROM_HIGH:
			case TYPE_DIE_FROM_HIGH:
				if(m_nFailBalanceTime > 0 || ragdollCompMatrix.d.z < m_fStartHeight - 0.3f)
					bGoToFalling = true;
				break;
			case TYPE_OVER_WALL:
			case TYPE_DIE_OVER_WALL:
				if(ragdollCompMatrix.d.z < m_fStartHeight - 0.3f || (m_nFailBalanceTime > 0 && fwTimer::GetTimeInMilliseconds() > m_nFailBalanceTime + 3000))
					bGoToFalling = true;
				break;
			case TYPE_DOWN_STAIRS:
			case TYPE_DIE_DOWN_STAIRS:
				if(m_nFailBalanceTime > 0 )
					bGoToFalling = true;
				break;
			default:
				break;
			}

			// force go to the next stage after 5sec in case something went wrong.
			if(fwTimer::GetTimeInMilliseconds() > m_nStartTime + 5000)
				bGoToFalling = true;

			if(bGoToFalling)
			{
				m_nFailBalanceTime = fwTimer::GetTimeInMilliseconds();

				ART::MessageParams msg;
				pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_STOP_ALL_MSG), &msg);

				if(m_nFallType==TYPE_DOWN_STAIRS || m_nFallType==TYPE_DIE_DOWN_STAIRS)
				{
					ART::MessageParams msgRollDownStairs;
					msgRollDownStairs.addBool(NMSTR_PARAM(NM_START), true);
					msgRollDownStairs.addVector3(NMSTR_PARAM(NM_ROLLDOWN_STAIRS_CUSTOM_ROLLDIR_VEC3), m_vecDirn.x, m_vecDirn.y, m_vecDirn.z);
					msgRollDownStairs.addBool(NMSTR_PARAM(NM_ROLLDOWN_STAIRS_USE_CUSTOM_ROLLDIR), true);
					pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_ROLLDOWN_STAIRS_MSG), &msgRollDownStairs);
				}
				else
				{
					ART::MessageParams msgCatchFall;
					msgCatchFall.addBool(NMSTR_PARAM(NM_START), true);
					msgCatchFall.addFloat(NMSTR_PARAM(NM_CATCHFALL_ARMS_STIFFNESS), 10.0f);
					msgCatchFall.addFloat(NMSTR_PARAM(NM_CATCHFALL_TORSO_STIFFNESS), 7.0f);
					pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_CATCHFALL_MSG), &msgCatchFall);

					ART::MessageParams msgFallReaction;
					msgFallReaction.addBool(NMSTR_PARAM(NM_SET_FALLING_REACTION_RESIST_ROLLING), true);
					msgCatchFall.addBool(NMSTR_PARAM(NM_START), true);
					msgCatchFall.addFloat(NMSTR_PARAM(NM_SET_FALLING_REACTION_GROUND_FRICTION), 4.0f);

					pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_SET_FALLING_REACTION_MSG), &msgFallReaction);

					ART::MessageParams msgPedal;
					msgPedal.addBool(NMSTR_PARAM(NM_START), true);
					msgPedal.addFloat(NMSTR_PARAM(NM_PEDAL_LEG_STIFFNESS), 7.0f);
					pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_PEDAL_MSG), &msgPedal);


					// apply an impulse to help the ped tumble over the edge
					Vector3 vecImpulse(m_vecDirn);
					// use different impulse magnitudes forward vs back
					float fImpulseMag = fForwardMult * sfFallDownImpulse + (1.0f - fForwardMult) * sfFallDownImpulseBack;
					vecImpulse.Scale(fImpulseMag * pPed->GetMass());

					// const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					pPed->ApplyImpulse(vecImpulse, VEC3_ZERO, RAGDOLL_HEAD, true);
					pPed->ApplyImpulse(-vecImpulse, VEC3_ZERO, RAGDOLL_FOOT_RIGHT, true);
				}
				m_nFallState = STATE_FALLING2;
			}
			else
			{
				// apply a continuous force to help the ped lean to and fall over stuff
				Vector3 vecForce(m_vecDirn);
				// use different force magnitudes forward vs back
				float fForceMag = fForwardMult * sfFallDownForce + (1.0f - fForwardMult) * sfFallDownForceBack;
				vecForce.Scale(fForceMag * pPed->GetMass());

				Matrix34 ragdollComponentMatrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());
				pPed->GetRagdollComponentMatrix(ragdollComponentMatrix, RAGDOLL_SPINE3);
				pPed->ApplyForce(vecForce, ragdollComponentMatrix.d - VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), RAGDOLL_SPINE3);
			}
		}
		break;
	case STATE_FALLING:
	case STATE_FALLING2:
		if(pPed->GetTransform().GetPosition().GetZf() < m_fGroundHeight + 1.0f)
		{
			ART::MessageParams msgRelax;
			msgRelax.addBool(NMSTR_PARAM(NM_START), true);
			msgRelax.addFloat(NMSTR_PARAM(NM_RELAX_RELAXATION), 70.0f);
			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_RELAX_MSG), &msgRelax);

			if(m_nFallType > TYPE_DIE_TYPES)
			{
				CEventDeath deathEvent(false, true);
				pPed->GetPedIntelligence()->AddEvent(deathEvent);
			}

			m_nFallState = STATE_RELAX;
		}
		break;
	case STATE_GRABBING:
		{

		}
		break;
	case STATE_RELAX:
		{

		}
		break;
	default:
		Assertf(false, "unhandled fall state");
		break;
	}

	// force control of ragdoll, stop the ped dying until we're done with them
	if(m_nFallType > TYPE_DIE_TYPES && m_nFallState < STATE_RELAX)
	{
		pPed->SetPedResetFlag( CPED_RESET_FLAG_ForceScriptControlledRagdoll, true );
	}
}

bool CTaskNMFallDown::FinishConditions(CPed* pPed)
{
	if (pPed->GetVelocity().z < -4.0f)
	{
		m_bHasSucceeded = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_HIGH_FALL;
		m_nSuggestedBlendOption = BLEND_FROM_NM_GETUP;
	}

	int nTestFlags = FLAG_RELAX_AP_LOW_HEALTH;
	if(m_nFallState==STATE_RELAX || m_nFallState==STATE_FALLING2)
		nTestFlags |= FLAG_VEL_CHECK;

	bool bReturn = CTaskNMBehaviour::ProcessFinishConditionsBase(pPed, MONITOR_FALL, nTestFlags);

	if(bReturn && m_bForceFatal && m_nFallType > TYPE_DIE_TYPES)
	{
		//pPed->SetHealth(0.0f);
		CEventDeath deathEvent(false, true);
		pPed->GetPedIntelligence()->AddEvent(deathEvent);
	}

	return bReturn;
}
