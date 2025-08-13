// Filename   :	TaskNMBalance.cpp
// Description:	Natural Motion balance class (FSM version)

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "fragmentnm/nm_channel.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/Collider.h"
#include "phbound/boundcomposite.h"
#include "physics/shapetest.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "fwanimation/pointcloud.h"
#include "grcore/debugdraw.h"
#include "fwmaths\Angle.h"

// Game headers

#include "camera/CamInterface.h"
#include "debug/DebugScene.h"
#include "Event\EventDamage.h"
#include "modelinfo/PedModelInfo.h"
#include "Network\NetworkInterface.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Peds\PedPlacement.h"
#include "Peds/Ped.h"
#include "PedGroup\PedGroup.h"
#include "Physics\GtaInst.h"
#include "Physics\Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task/Physics/NmDebug.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMBrace.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"
#include "Weapons/Weapon.h"

AI_OPTIMISATIONS()

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMBalance::Tunables CTaskNMBalance::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMBalance, 0xb26787a6);
//////////////////////////////////////////////////////////////////////////

#define NUM_FRAMES_PER_TEETER_PROBE 3

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CClonedNMBalanceInfo //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CClonedNMBalanceInfo::CClonedNMBalanceInfo(const CEntity* pLookAt, u32 nGrabFlags, const Vector3* pVecLeanDir, float fLeanAmount, u32 balanceType)
: m_nGrabFlags(0)
, m_vecLeanDir(Vector3(0.0f, 0.0f, 0.0f))
, m_leanAmount((u8)(fLeanAmount * (float)((1<<SIZEOF_LEAN_AMOUNT)-1)))
, m_balanceType((u8)balanceType)
, m_bHasLookAt(false)
, m_bHasLeanDir(false)
, m_bHasBalanceType(balanceType != CTaskNMBalance::BALANCE_DEFAULT )
{
	// mask out the runtime grab flags
	m_nGrabFlags = (u16)(nGrabFlags & ((1<<SIZEOF_GRAB_FLAGS)-1));

	Assert(fLeanAmount <= 1.0f);

	if (pLookAt && pLookAt->GetIsDynamic() && static_cast<const CDynamicEntity*>(pLookAt)->GetNetworkObject())
	{
		m_lookAtEntity = static_cast<const CDynamicEntity*>(pLookAt)->GetNetworkObject()->GetObjectID();
		m_bHasLookAt = true;
	}

	if (pVecLeanDir)
	{
		m_vecLeanDir = *pVecLeanDir;
		m_bHasLeanDir = true;
	}
}

CClonedNMBalanceInfo::CClonedNMBalanceInfo()
: m_nGrabFlags(0)
, m_vecLeanDir(Vector3(0.0f, 0.0f, 0.0f))
, m_leanAmount(0)
, m_balanceType(CTaskNMBalance::BALANCE_DEFAULT)
, m_bHasLookAt(false)
, m_bHasLeanDir(false)
, m_bHasBalanceType(false)
{
}

CTaskFSMClone *CClonedNMBalanceInfo::CreateCloneFSMTask()
{
	CEntity *pLookAtEntity = NULL;

	if (m_bHasLookAt)
	{
		netObject* pObj = NetworkInterface::GetNetworkObject(m_lookAtEntity);

		if (pObj)
		{
			pLookAtEntity = pObj->GetEntity();
		}
	}

	float leanAmount = (float) m_leanAmount / (float)((1<<SIZEOF_LEAN_AMOUNT)-1);

	return rage_new CTaskNMBalance(10000, 10000, pLookAtEntity, (u32)m_nGrabFlags, m_bHasLeanDir ? &m_vecLeanDir : NULL, leanAmount, NULL, static_cast<CTaskNMBalance::eBalanceType>(m_balanceType));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMBalance ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMBalance::CTaskNMBalance(u32 nMinTime, u32 nMaxTime, CEntity* pLookAt, u32 nGrabFlags, const Vector3* pVecLeanDir,
							   float fLeanAmount, const CGrabHelper* pGrabHelper, eBalanceType eType)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: CTaskNMBehaviour(nMinTime, nMaxTime)
, m_vecLean(pVecLeanDir ? *pVecLeanDir : Vector3::ZeroType)
, m_vLookAt(Vector3::ZeroType)
, m_vEdgeLeft(Vector3::ZeroType)
, m_vEdgeRight(Vector3::ZeroType)
, m_eState(STANDING)
, m_pLookAt(pLookAt)
, m_fLeanAmount(fLeanAmount)
, m_fAbortLeanTimer(0.0f)
, m_pGrabHelper(NULL)
, m_eType(eType)
, m_bBalanceFailed(false)
, m_bRollingDownStairs(false)
, m_bRollingFallInitd(false)
, m_bTeeterInitd(false)
, m_bCatchFallInitd(false)
, m_bPedalling(false)
, m_bLookAtPosition(false)
, m_rollStartTime(0)
, m_FirstTeeterProbeDetectedDrop(false)
, m_probeResult(1)
, m_bBeingPushed(false)
, m_bPushedTooLong(false)
, m_PushStartTime(0)
{
#if !__FINAL
	m_strTaskName = "NMBalance";
#endif // !__FINAL

	m_probeResult.Reset();

	if (CGrabHelper::GetPool()->GetNoOfFreeSpaces() > 0)
		m_pGrabHelper = rage_new CGrabHelper(pGrabHelper);

	// ignore nGrabFlags if we're copying grab stuff from an existing grab helper
	if(pGrabHelper==NULL && GetGrabHelper())
		GetGrabHelper()->SetFlags(nGrabFlags);
	SetInternalTaskType(CTaskTypes::TASK_NM_BALANCE);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMBalance::~CTaskNMBalance()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_pGrabHelper)
		delete m_pGrabHelper;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMBalance::eBalanceType CTaskNMBalance::EvaluateAndSetType(const CPed* const pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	//- set weak option ?
	static float fHealthThresholdMissionCharPlayer = 110.0f, fHealthThresholdOther = 140.0f;

	if (ms_bUseParameterSets)
	{
		const float fHealthThreshold = (pPed->IsPlayer() || (pPed->PopTypeIsMission()) ? fHealthThresholdMissionCharPlayer : fHealthThresholdOther);

		if (!pPed->CheckAgilityFlags(AF_BALANCE_STRONG) || (pPed->GetHealth() < fHealthThreshold)) {
			SetType(BALANCE_WEAK);
		}
		else if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeapon() && pPed->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetIsGun()) {
			SetType(BALANCE_AGGRESSIVE);
		}
		else {
			SetType(BALANCE_DEFAULT);
		}
	}

	return GetType();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBalance::BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFailure(pPed, pFeedbackInterface);

	// pass the message on to the grab helper
	if (GetGrabHelper())
		GetGrabHelper()->BehaviourFailure(pPed, pFeedbackInterface);

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		if (GetGrabHelper())
			GetGrabHelper()->SetStatusFlag(CGrabHelper::BALANCE_STATUS_FAILED);

		if(!m_bBalanceFailed)
		{
			Matrix34 ragdollCompMatrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());
			pPed->GetRagdollComponentMatrix(ragdollCompMatrix, RAGDOLL_SPINE3);

			// If ped fell on stairs, try and make them roll down the slope.
			WorldProbe::CShapeTestFixedResults<> probeResult;
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(ragdollCompMatrix.d, ragdollCompMatrix.d - Vector3(0.0f, 0.0f, 2.5f));
			probeDesc.SetResultsStructure(&probeResult);
			probeDesc.SetExcludeEntity(NULL);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
			{
				if(PGTAMATERIALMGR->GetPolyFlagStairs(probeResult[0].GetHitMaterialId()))
				{
					ART::MessageParams msg;
					msg.addBool(NMSTR_PARAM(NM_START), true);
					pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_ROLLDOWN_STAIRS_MSG), &msg);

					m_bRollingDownStairs = true;
				}
			}

			// If we're a cop & have been knocked over by a player, then give them a 1-star wanted level.
			if(pPed->GetPedType()==PEDTYPE_COP && m_pLookAt && m_pLookAt->GetIsTypePed() && ((CPed*)m_pLookAt.Get())->IsPlayer())
			{
				CWanted * pPlayerWanted = ((CPed*)m_pLookAt.Get())->GetPlayerWanted();
				if(pPlayerWanted->GetWantedLevel() < WANTED_LEVEL1)
					pPlayerWanted->SetWantedLevel( VEC3V_TO_VECTOR3(((CPed*)m_pLookAt.Get())->GetTransform().GetPosition()), WANTED_LEVEL1, 0, WL_FROM_TASK_NM_BALANCE);
			}

			pPed->SetPedResetFlag( CPED_RESET_FLAG_KnockedToTheFloorByPlayer, true );
			
			//Note that we were knocked to the ground.
			pPed->GetPedIntelligence()->KnockedToGround(m_pLookAt);

			sm_Tunables.m_OnBalanceFailed.Post(*pPed, this);
		}
		m_bBalanceFailed = true;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBalance::BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourSuccess(pPed, pFeedbackInterface);

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		// check min time and then flag success
		if(fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMinTime && !m_bBeingPushed)
		{
			m_bHasSucceeded = true;
			m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
			m_nSuggestedBlendOption = BLEND_FROM_NM_STANDING;

			if (GetGrabHelper())
				GetGrabHelper()->SetStatusFlag(CGrabHelper::BALANCE_STATUS_SUCCEEDED);
		}
	}

	// Use success message from "catchFall" to switch states from FALLING-->ON_GROUND.
	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_CATCHFALL_FB))
	{
		m_eState = ON_GROUND;
	}

	// pass the message on to the grab helper
	if (GetGrabHelper())
		GetGrabHelper()->BehaviourSuccess(pPed, pFeedbackInterface);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBalance::BehaviourStart(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourStart(pPed, pFeedbackInterface);

	// If a catchfall behaviour gets started, we must be falling:
	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_CATCHFALL_FB))
	{
		m_eState = FALLING;
	}

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBalance::BehaviourFinish(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFinish(pPed, pFeedbackInterface);

	// pass the message on to the grab helper
	if (GetGrabHelper())
		GetGrabHelper()->BehaviourFinish(pPed, pFeedbackInterface);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBalance::BehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourEvent(pPed, pFeedbackInterface);

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{

		if(fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMinTime)
		{
			if(CTaskNMBehaviour::ProcessBalanceBehaviourEvent(pPed, pFeedbackInterface))
			{
				m_bHasSucceeded = true;
				m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
				m_nSuggestedBlendOption = BLEND_FROM_NM_STANDING;

				if (GetGrabHelper())
					GetGrabHelper()->SetStatusFlag(CGrabHelper::BALANCE_STATUS_SUCCEEDED);
			}
		}
	}
}


//CTaskNMBalance::Parameters CTaskNMBalance::ms_Parameters[NUM_BALANCE_TYPES];
const char* CTaskNMBalance::ms_TypeToString[NUM_BALANCE_TYPES] = 
{
	"DEFAULT",
	"WEAK",
	"AGGRESSIVE",
	"BALANCE_FORCE_FALL_FROM_MOVING_CAR"
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
aiTask* CTaskNMBalance::Copy() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CTaskNMBalance* clone = rage_new CTaskNMBalance(m_nMinTime, m_nMaxTime, m_pLookAt, 0, &m_vecLean, m_fLeanAmount, m_pGrabHelper, m_eType);

	clone->m_vLookAt = m_vLookAt;
	clone->m_bLookAtPosition = m_bLookAtPosition;

	return clone;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBalance::SetLookAtPosition(const Vector3& vLookAt)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_vLookAt = vLookAt;
	m_bLookAtPosition = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBalance::StartBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_eState = STANDING;

	// start the full-body balance behaviour

	nmDebugf2("[%u] Starting nm balance task:%s(%p) Look at entity:%s(%p), type:%d", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, m_pLookAt ? m_pLookAt->GetModelName(): "NONE", m_pLookAt.Get(), m_eType);

	switch (m_eType)
	{
	case BALANCE_DEFAULT:
		AddTuningSet(&sm_Tunables.m_StartDefault);
		break;
	case BALANCE_WEAK:
		AddTuningSet(&sm_Tunables.m_StartWeak);
		break;		
	case BALANCE_AGGRESSIVE:
		AddTuningSet(&sm_Tunables.m_StartAggressive);
		break;
	case BALANCE_FORCE_FALL_FROM_MOVING_CAR:
		AddTuningSet(&sm_Tunables.m_FallOffAMovingCar);
		break;
	default:
		AssertMsg(0, "state not handled");
		AddTuningSet(&sm_Tunables.m_StartDefault);
		break;
	}

	if (OnMovingGround(pPed))
		AddTuningSet(&sm_Tunables.m_OnMovingGround);

	bool onStairs = false, onSteepSlope = false;
	ProbeForStairsOrSlope(pPed, onStairs, onSteepSlope);

	if (onStairs)
		AddTuningSet(&sm_Tunables.m_OnStairs);
	else if (onSteepSlope)
		AddTuningSet(&sm_Tunables.m_OnSteepSlope);

	bool bHeadLookAtBumpTarget = m_pLookAt && m_pLookAt->GetIsTypePed();
	if (bHeadLookAtBumpTarget)
		AddTuningSet(&sm_Tunables.m_BumpedByPed);

	CNmMessageList list;
	ApplyTuningSetsToList(list);

	// Initialize the arms windmill offset to something slightly random
	if (m_eType == BALANCE_FORCE_FALL_FROM_MOVING_CAR)
	{
		ART::MessageParams &armsWindmill = list.GetMessage(NMSTR_MSG(NM_WINDMILL_MSG));
		armsWindmill.addFloat(NMSTR_PARAM(NM_WINDMILL_PHASE_OFFSET), g_DrawRand.GetRanged(-100.f,100.f));
	}

	ART::MessageParams &msgBalance = list.GetMessage(NMSTR_MSG(NM_BALANCE_MSG));

	Assert(static_cast<CEntity*>(pPed) != m_pLookAt);
	if(m_pLookAt)
	{
		Vector3 vecLookAtPos = VEC3V_TO_VECTOR3(m_pLookAt->GetTransform().GetPosition());
		if(m_pLookAt->GetIsTypeVehicle() && ((CVehicle*)m_pLookAt.Get())->GetDriver())
		{
			((CVehicle*)m_pLookAt.Get())->GetDriver()->GetBonePosition(vecLookAtPos, BONETAG_HEAD);
		}
		else if(m_pLookAt == pPed->GetGroundPhysical())
		{
			vecLookAtPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(pPed->GetTransform().GetB()) * 5.0f);
		}

		if (!bHeadLookAtBumpTarget)
		{
			msgBalance.addBool(NMSTR_PARAM(NM_BALANCE_LOOKAT_B), true);
			msgBalance.addVector3(NMSTR_PARAM(NM_BALANCE_LOOKAT_POS_VEC3), vecLookAtPos.x, vecLookAtPos.y, vecLookAtPos.z);
		}
	}
	else if(m_bLookAtPosition)
	{
		msgBalance.addBool(NMSTR_PARAM(NM_BALANCE_LOOKAT_B), true);
		msgBalance.addVector3(NMSTR_PARAM(NM_BALANCE_LOOKAT_POS_VEC3), m_vLookAt.x, m_vLookAt.y, m_vLookAt.z);
	}

	if (!bHeadLookAtBumpTarget)
	{
		msgBalance.addFloat(NMSTR_PARAM(NM_BALANCE_HEAD_LOOK_AT_VEL_PROB), sm_Tunables.m_lookAtVelProbIfNoBumpTarget);
	}


	// tell the grab helper that we're balancing
	if (GetGrabHelper())
		GetGrabHelper()->SetStatusFlag(CGrabHelper::BALANCE_STATUS_ACTIVE);

	// configure the balance
	//	static float TEST_STEP_HEIGHT = 0.24f;
	//	ART::MessageParams msgConfigure;
	//	msgConfigure.addFloat(NMSTR_PARAM(NM_CONFIGURE_BALANCE_STEP_HEIGHT), TEST_STEP_HEIGHT);
	//	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_CONFIGURE_BALANCE_MSG), &msgConfigure);

	list.Post(pPed->GetRagdollInst());

	// Apply a tunable additional initial force to the ped
	CPhysical* pBumpedByPhysical = m_pLookAt != NULL && m_pLookAt->GetIsPhysical() ? static_cast<CPhysical*>(m_pLookAt.Get()) : NULL;
	if (pBumpedByPhysical == NULL)
	{
		const CCollisionRecord * pCollisionRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
		if (pCollisionRecord != NULL && pCollisionRecord->m_pRegdCollisionEntity != NULL && pCollisionRecord->m_pRegdCollisionEntity->GetIsPhysical())
		{
			pBumpedByPhysical = static_cast<CPhysical*>(pCollisionRecord->m_pRegdCollisionEntity.Get());
		}
	}
	if(pBumpedByPhysical != NULL && sm_Tunables.m_InitialBumpForce.m_Enable)
	{				
		// Calculate the normalized force vector
		Vec3V impulseVecNormalized = pPed->GetMatrix().d() - pBumpedByPhysical->GetMatrix().d();
		impulseVecNormalized = NormalizeSafe(impulseVecNormalized, Vec3V(V_ZERO));

		// Work out the bump ped velocity in the direction of this ped
		Vec3V bumpPedVel = VECTOR3_TO_VEC3V(pBumpedByPhysical->GetVelocity());
		bumpPedVel = impulseVecNormalized * Dot(bumpPedVel, impulseVecNormalized);

		// Get impulse from tuning
		Vec3V impulseVec(impulseVecNormalized);
		sm_Tunables.m_InitialBumpForce.GetImpulseVec(impulseVec, bumpPedVel, pPed);

		ART::MessageParams msg;
		msg.addBool(NMSTR_PARAM(NM_START), true);
		msg.addInt(NMSTR_PARAM(NM_SHOTNEWBULLET_BODYPART), sm_Tunables.m_InitialBumpComponent);
		msg.addBool(NMSTR_PARAM(NM_SHOTNEWBULLET_LOCAL_HIT_POINT_INFO), true);
		msg.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_POS_VEC3), sm_Tunables.m_InitialBumpOffset.x, sm_Tunables.m_InitialBumpOffset.y, sm_Tunables.m_InitialBumpOffset.z);
		msg.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_VEL_VEC3), impulseVec.GetXf(), impulseVec.GetYf(), impulseVec.GetZf());
		msg.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_NORMAL_VEC3), impulseVecNormalized.GetXf(), impulseVecNormalized.GetYf(), impulseVecNormalized.GetZf());
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_SHOTNEWBULLET_MSG), &msg);
	}

	// start grab behaviour (if it has anything to grab yet)
    if (GetGrabHelper() && (!ms_bDisableBumpGrabForPlayer || !pPed->IsPlayer()))
    {
	    GetGrabHelper()->StartBehaviour(pPed);
    }
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMBalance::HandleRagdollImpact(float fMag, const CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId)
//////////////////////////////////////////////////////////////////////////
{
	if (m_pGrabHelper)
	{
		// Handles any impacts with the entity we're grabbing
		if (pEntity != NULL && pEntity == m_pGrabHelper->GetGrabTargetEntity())
		{
			return true;
		}
	}

	return CTaskNMBehaviour::HandleRagdollImpact(fMag, pEntity, vPedNormal, nComponent, nMaterialId);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBalance::ControlBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Cache a reference to the NM control task.
	CTaskNMControl *pNmControlTask = GetControlTask();
	// Velocity of ragdoll's centre of mass.
	Vector3 vPedComVel; pPed->GetRagdollInst()->GetCOMVel(vPedComVel);

	// Threshold speed to determine if ped has come to rest.
	//static dev_float fSmallSpeed = 0.316f;
	//float fSmallSpeed2 = fSmallSpeed*fSmallSpeed;

	const CCollisionRecord * pColRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
	const CEntity * pColEntity = pColRecord != NULL ? pColRecord->m_pRegdCollisionEntity.Get() : NULL;

	/////////////////////////////////////////////////////////////////
	// TODO RA: Move all this code into the relevant states below. //
	/////////////////////////////////////////////////////////////////

	// Ensure that we're being pushed *and* that we're moving fast enough on the ground to maintain the push
	if(pColEntity != NULL && pColEntity->GetIsTypePed() 
		&& static_cast<const CPed*>(pColEntity)->IsPlayer() && pPed->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f
		&& (!m_bBalanceFailed || !ProcessFinishConditionsBase(pPed, MONITOR_RELAX, FLAG_VEL_CHECK | FLAG_SKIP_MIN_TIME_CHECK, &sm_Tunables.m_PushedThresholdOnGround)))
	{
		m_nStartTime = fwTimer::GetTimeInMilliseconds();

		if (!m_bBeingPushed)
		{
			m_PushStartTime = fwTimer::GetTimeInMilliseconds();
			if (m_bBalanceFailed)
			{
				sm_Tunables.m_OnBeingPushedOnGround.Post(*pPed, this);
			}
			else
			{
				sm_Tunables.m_OnBeingPushed.Post(*pPed, this);
			}
			m_bBeingPushed = true;

		}
	}
	else
	{
		if (m_bBeingPushed && ((fwTimer::GetTimeInMilliseconds() - m_nStartTime) > (m_bBalanceFailed ? sm_Tunables.m_NotBeingPushedOnGroundDelayMS : sm_Tunables.m_NotBeingPushedDelayMS)))
		{
			m_PushStartTime = 0;
			sm_Tunables.m_OnNotBeingPushed.Post(*pPed, this);
			m_bBeingPushed = false;
			m_bPushedTooLong = false;

		}
	}

	if (m_bBalanceFailed && m_bBeingPushed && !m_bPushedTooLong && ((fwTimer::GetTimeInMilliseconds() - m_PushStartTime) > sm_Tunables.m_BeingPushedOnGroundTooLongMS))
	{
		sm_Tunables.m_OnBeingPushedOnGroundTooLong.Post(*pPed, this);
		m_bPushedTooLong = true;
	}

	// Apply a tunable additional initial force to the ped
	if(pColEntity && m_bBeingPushed && sm_Tunables.m_InitialBumpForce.ShouldApply(m_PushStartTime))
	{				
		// Calculate the normalized force vector
		Vec3V impulseVec = pPed->GetMatrix().d() - pColEntity->GetMatrix().d();
		impulseVec = NormalizeSafe(impulseVec, Vec3V(V_ZERO));

		// Work out the bump ped velocity in the direction of this ped
		Vec3V bumpPedVel = pColEntity->GetIsPhysical() ? VECTOR3_TO_VEC3V(((CPhysical*)pColEntity)->GetVelocity()) : Vec3V(V_ZERO);
		bumpPedVel = impulseVec*Dot(bumpPedVel, NormalizeSafe(pPed->GetMatrix().d() - pColEntity->GetMatrix().d(), Vec3V(V_ZERO)));

		// Get impulse from tuning
		sm_Tunables.m_InitialBumpForce.GetImpulseVec(impulseVec, bumpPedVel, pPed);

		// send to nm
		pPed->ApplyImpulse(RCC_VECTOR3(impulseVec), sm_Tunables.m_InitialBumpOffset, sm_Tunables.m_InitialBumpComponent, true);
	}

	// Balancer messages must be called before teeter!
	if (!m_bBalanceFailed && sm_Tunables.m_ScaleStayUprightWithVel & (sm_Tunables.m_MaxVel > sm_Tunables.m_MinVel))
	{
		float pedVel = pPed->GetVelocity().Mag();
		float t = (pedVel-sm_Tunables.m_MinVel)/(sm_Tunables.m_MaxVel - sm_Tunables.m_MinVel);
		t=Clamp(t, 0.0f, 1.0f);
		float forceMag = Lerp(t, sm_Tunables.m_StayUprightAtMinVel, sm_Tunables.m_StayUprightAtMaxVel);

		// send to nm
		ART::MessageParams msgStayUpright;
		msgStayUpright.addBool(NMSTR_PARAM(NM_STAYUPRIGHT_USE_FORCES), true);
		msgStayUpright.addFloat(NMSTR_PARAM(NM_STAYUPRIGHT_FORCE_STRENGTH), forceMag);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_STAYUPRIGHT_MSG), &msgStayUpright);
	}

	if(m_pLookAt)
	{
		Vector3 vecLookAtPos = VEC3V_TO_VECTOR3(m_pLookAt->GetTransform().GetPosition());
		if(m_pLookAt->GetIsTypePed())
		{
			CPed* pBumpPed = (CPed *)m_pLookAt.Get();
			 
			pBumpPed->GetBonePosition(vecLookAtPos, BONETAG_HEAD);

			Vector3 vecFlinchPosition(VEC3_ZERO);

			pBumpPed->GetBonePosition(vecFlinchPosition, BONETAG_SPINE1);

			Vec3V pedPosition(pPed->GetTransform().GetPosition());
			Vector3 distanceToLookat(RC_VECTOR3(pedPosition));
			distanceToLookat -= vecFlinchPosition;
			distanceToLookat.z = 0;

			ART::MessageParams msgFlinchUpd;
			msgFlinchUpd.addVector3(NMSTR_PARAM(NM_FLINCH_POS_VEC3), vecFlinchPosition.x, vecFlinchPosition.y, vecFlinchPosition.z + (m_bBalanceFailed ? sm_Tunables.m_fFlinchTargetZOffsetOnGround : sm_Tunables.m_fFlinchTargetZOffset));
			
			bool useLeftArm = false;
			bool useRightArm = false;

			if ((m_bBalanceFailed && sm_Tunables.m_fMaxTargetDistToUpdateFlinchOnGround > distanceToLookat.Mag2()) ||
				(!m_bBalanceFailed && sm_Tunables.m_fMaxTargetDistToUpdateFlinch > distanceToLookat.Mag2()))
			{
				Vec3V pedPos = pPed->GetMatrix().d();
				Vec3V targetPedPos = m_pLookAt->GetTransform().GetPosition();
				Vector3 threatVector = RC_VECTOR3(targetPedPos) - RC_VECTOR3(pedPos);
				threatVector.NormalizeSafe();

				Vec3V forwardVector = pPed->GetMatrix().b();
				float dotProduct = threatVector.Dot(RC_VECTOR3(forwardVector));
				if ((m_bBalanceFailed && dotProduct > sm_Tunables.m_fMinForwardVectorToFlinchOnGround) ||
					(!m_bBalanceFailed && dotProduct > sm_Tunables.m_fMinForwardVectorToFlinch))
				{
					Vec3V rightVector = pPed->GetMatrix().a();
					useLeftArm = threatVector.Dot(RC_VECTOR3(rightVector))<0.0f;
					useRightArm = !useLeftArm;
				}
			}
			
			msgFlinchUpd.addBool(NMSTR_PARAM(NM_FLINCH_LEFT_HANDED), useLeftArm);
			msgFlinchUpd.addBool(NMSTR_PARAM(NM_FLINCH_RIGHT_HANDED), useRightArm);

			msgFlinchUpd.addBool(NMSTR_PARAM(NM_FLINCH_USE_LEFT_ARM), useLeftArm);
			msgFlinchUpd.addBool(NMSTR_PARAM(NM_FLINCH_USE_RIGHT_ARM), useRightArm);

			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_FLINCH_MSG), &msgFlinchUpd);

			ART::MessageParams msgLookAt;
			msgLookAt.addVector3(NMSTR_PARAM(NM_HEADLOOK_POS), vecLookAtPos.x, vecLookAtPos.y, vecLookAtPos.z + (m_bBalanceFailed ? sm_Tunables.m_fHeadLookZOffsetOnGround : sm_Tunables.m_fHeadLookZOffset));
			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_HEADLOOK_MSG), &msgLookAt);

		}
		else
		{
			ART::MessageParams msgLookAt;
			msgLookAt.addBool(NMSTR_PARAM(NM_BALANCE_LOOKAT_B), true);
			msgLookAt.addVector3(NMSTR_PARAM(NM_BALANCE_LOOKAT_POS_VEC3), vecLookAtPos.x, vecLookAtPos.y, vecLookAtPos.z);
			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_BALANCE_MSG), &msgLookAt);
		}
	}

	if(m_fLeanAmount > 0.0f)
	{
		// if grab has just succeeded, we need to stop leaning
		// If a lean timer has been set, abort leaning after that time
		bool bAbortLean = false;
		ART::MessageParams msgLean;
		if(GetGrabHelper() && GetGrabHelper()->IsHoldingOn())
			bAbortLean = true;
		else if( m_fAbortLeanTimer > 0.0f )
		{
			m_fAbortLeanTimer -= fwTimer::GetTimeStep();
			if( m_fAbortLeanTimer <= 0.0f )
				bAbortLean = true;
		}

		if( bAbortLean )
		{
			msgLean.addBool(NMSTR_PARAM(NM_START), false);
			m_fLeanAmount = 0.0f;
		}
		else
		{
			Vector3 vecLeanTarget = m_vecLean;
			vecLeanTarget.z = 0.0f;
			if(vecLeanTarget.Mag2() > 0.01f)
				vecLeanTarget.NormalizeFast();
			else
				vecLeanTarget = VEC3V_TO_VECTOR3(pPed->GetTransform().GetB());

			msgLean.addBool(NMSTR_PARAM(NM_START), true);
			msgLean.addVector3(NMSTR_PARAM(NM_FORCELEANDIR_DIR), vecLeanTarget.x, vecLeanTarget.y, vecLeanTarget.z);
			msgLean.addFloat(NMSTR_PARAM(NM_FORCELEANDIR_LEAN_AMOUNT), m_fLeanAmount);
		}
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_FORCELEANDIR_MSG), &msgLean);
	}

	if (GetGrabHelper() && GetGrabHelper()->IsHoldingOn())
	{
		CEntity *pGrabTargetEntity = GetGrabHelper()->GetGrabTargetEntity();
		if (pGrabTargetEntity && pGrabTargetEntity->GetIsTypeVehicle())
		{
			CVehicle *pGrabTargetVehicle = static_cast< CVehicle * >(pGrabTargetEntity);

			CComponentReservationManager* pCRM = pGrabTargetVehicle->GetComponentReservationMgr();
			if (pCRM)
			{
				for (u32 componentIndex = 0; componentIndex < pCRM->GetNumReserveComponents(); componentIndex ++)
				{
					CComponentReservation* componentReservation = pCRM->FindComponentReservationByArrayIndex(componentIndex);
					if (componentReservation)
					{
						CPed* pPed = componentReservation->GetPedUsingComponent();
						if (pPed && pPed != GetPed() && pPed->GetIsEnteringVehicle())
						{
							GetGrabHelper()->SetFlag(CGrabHelper::TARGET_ABORT_GRABBING);

							break;
						}
					}
				}
			}
		}
	}

	if (GetGrabHelper() && m_eState != ROLLING_FALL)
		GetGrabHelper()->ControlBehaviour(pPed);

	// try using success and failure messages to end task
	if(m_bBalanceFailed)
	{
		// HDD testing
		// kicks off pedal if balance fails (eg character falls) while still holding onto something
		if(GetGrabHelper() && GetGrabHelper()->IsHoldingOn())
		{
			if(!m_bPedalling)
			{
				AddTuningSet(&sm_Tunables.m_LostBalanceAndGrabbing);

				m_bPedalling = true;
			}
		}
		else if(m_bPedalling)
		{
			ART::MessageParams msgFlail;
			msgFlail.addBool(NMSTR_PARAM(NM_START), false);
			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_PEDAL_MSG), &msgFlail);

			m_bPedalling = false;
		}
	}


	static float linVelMin = 5.0f;
	static float slopeLim = 0.96f;
	static float velLim = 4.0f;
	float slopeDot = pPed->GetGroundNormal().z;
	float velMag = Abs(pPed->GetVelocity().Mag() - VEC3V_TO_VECTOR3(pPed->GetCollider()->GetReferenceFrameVelocity()).Mag());

	///////////////////////////////////////// BEHAVIOUR STATE MACHINE //////////////////////////////////////////
	switch(m_eState)
	{
	///////////////
	case STANDING:
	///////////////
#if !__FINAL
		m_strTaskName = "NMBalance:STANDING";
#endif // !__FINAL

		//Moved this up from staggering as some peds (like those against walls) were taking too long to get to the staggering state
		if(pPed->GetPedAudioEntity())
			pPed->GetPedAudioEntity()->SetIsStaggering();
		// Depending on the severity of the shot, the ped may be knocked clean off their feet. Switch
		// to the appropriate state based on the NM balancer feedback.
		if(smart_cast<CTaskNMControl*>(pNmControlTask)->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE))
		{
			ClearFlag(CTaskNMBehaviour::DO_CLIP_POSE);
			m_eState = FALLING;
		}
		//else if(vPedComVel.Mag2() >= fSmallSpeed2)
		//{
			m_eState = STAGGERING;
		//}
		break;

	/////////////////
	case STAGGERING:
	/////////////////
#if !__FINAL
		m_strTaskName = "NMBalance:STAGGERING";
#endif // !__FINAL

#if __BANK
		if(CTaskNMControl::ms_bTeeterEnabled)
		{
#endif // __BANK
			// Probe for a significant drop in the direction of travel.
			if(ProbeForSignificantDrop(pPed, m_vEdgeLeft, m_vEdgeRight, OnMovingGround(pPed), m_FirstTeeterProbeDetectedDrop, m_probeResult))
			{
				m_eState = TEETERING;
			}
#if __BANK
		}
#endif // __BANK

		// Has the ped lost their balance?
		if(smart_cast<CTaskNMControl*>(pNmControlTask)->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE))
		{
			ClearFlag(CTaskNMBehaviour::DO_CLIP_POSE);
			m_eState = FALLING;
		}

		// Try to detect that the ped is balancing on the spot to help early exit from NM back to clip.
		//if(vPedComVel.Mag2() < fSmallSpeed2)
		//{
		//	m_eState = STANDING;
		//}
		break;

	/////////////////
	case TEETERING:
	/////////////////
#if !__FINAL
		m_strTaskName = "NMBalance:TEETERING";
#endif // !__FINAL

#if __BANK
		if(CTaskNMControl::ms_bTeeterEnabled)
		{
#endif // __BANK
			if (!m_bTeeterInitd)
			{
				AddTuningSet(&sm_Tunables.m_Teeter);
				CNmMessageList list;
				ApplyTuningSetsToList(list);
				ART::MessageParams msgTeeter = list.GetMessage(NMSTR_MSG(NM_TEETER_MSG));
				msgTeeter.addVector3(NMSTR_PARAM(NM_TEETER_EDGE_LEFT), m_vEdgeLeft.x, m_vEdgeLeft.y, m_vEdgeLeft.z);
				msgTeeter.addVector3(NMSTR_PARAM(NM_TEETER_EDGE_RIGHT), m_vEdgeRight.x, m_vEdgeRight.y, m_vEdgeRight.z);
				pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_TEETER_MSG), &msgTeeter);
				m_bTeeterInitd = true;
			}

#if __BANK
			if(CNmDebug::ms_bDrawTeeterEdgeDetection)
			{
				grcDebugDraw::Line(m_vEdgeLeft, m_vEdgeRight, Color_yellow);
			}
#endif // __BANK

			// Continue to probe if on a moving vehicle
			if (OnMovingGround(pPed) && ProbeForSignificantDrop(pPed, m_vEdgeLeft, m_vEdgeRight, OnMovingGround(pPed), m_FirstTeeterProbeDetectedDrop, m_probeResult))
			{
				ART::MessageParams msgTeeter;
				msgTeeter.addVector3(NMSTR_PARAM(NM_TEETER_EDGE_LEFT), m_vEdgeLeft.x, m_vEdgeLeft.y, m_vEdgeLeft.z);
				msgTeeter.addVector3(NMSTR_PARAM(NM_TEETER_EDGE_RIGHT), m_vEdgeRight.x, m_vEdgeRight.y, m_vEdgeRight.z);
				pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_TEETER_MSG), &msgTeeter);
			}
#if __BANK
		}
#endif // __BANK

		// Has the ped lost their balance?
		if(smart_cast<CTaskNMControl*>(pNmControlTask)->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE))
		{
			ClearFlag(CTaskNMBehaviour::DO_CLIP_POSE);

			m_eState = FALLING;
		}
		break;


	//////////////
	case FALLING:
	//////////////
#if !__FINAL
		m_strTaskName = "NMBalance:FALLING";
#endif // !__FINAL

		if(pPed->GetPedAudioEntity())
			pPed->GetPedAudioEntity()->SetIsStaggerFalling();

		// Switch to a roll down stairs if on a slope or if there is a significant relative velocity (to ground object)
		if (slopeDot < slopeLim || velMag > velLim)
		{
			m_eState = ROLLING_FALL;
		}

		m_bBalanceFailed = true;
		//m_eState = STAGGERING;
		break;


		//////////////
	case ROLLING_FALL:
		//////////////
#if !__FINAL
		m_strTaskName = "NMBalance:ROLLING_FALL";
#endif // !__FINAL

		if (!m_bRollingFallInitd)
		{
			AddTuningSet(&sm_Tunables.m_RollingFall);

			m_rollStartTime = fwTimer::GetTimeInMilliseconds();
			m_bRollingFallInitd = true;
		}

		// If rollingDownStairs for a while, switch to catchfall with added resistance
		if (!m_bCatchFallInitd && fwTimer::GetTimeInMilliseconds() > m_rollStartTime + sm_Tunables.m_timeToCatchfallMS &&
			velMag < linVelMin)
		{
			AddTuningSet(&sm_Tunables.m_CatchFall);

			m_bCatchFallInitd = true;
		}

		break;

	////////////////
	case ON_GROUND:
	////////////////
#if !__FINAL
		m_strTaskName = "NMBalance:ON_GROUND";
#endif // !__FINAL

		m_bBalanceFailed = true;
		//m_eState = STAGGERING;
		break;


	/////////
	default:
	/////////
		taskAssertf(false, "Unknown state in CTaskNMBalance.");
		break;
	}
	/////////////////////////////////////////// END OF STATE MACHINE ///////////////////////////////////////////

	CTaskNMBehaviour::ControlBehaviour(pPed);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBalance::FinishConditions(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (GetGrabHelper() && m_eState != ROLLING_FALL)
		GetGrabHelper()->FinishConditions(pPed);

	int nFlags = FLAG_QUIT_IN_WATER;
	if(m_bBalanceFailed)
		nFlags |= FLAG_VEL_CHECK;

	static float fallingSpeed = -4.0f;
	if (pPed->GetVelocity().z < fallingSpeed)
	{
		m_bHasFailed = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_HIGH_FALL;
	}

	// for the player we want to use a vel check even if the balance hasn't failed (or succeeded)
	// after a certain amount of time, so you can't get stuck in a pose where you're unable to balance
	// (wedgied on a car door for example)
	if(pPed->IsPlayer() && fwTimer::GetTimeInMilliseconds() > m_nStartTime + CTaskNMBrace::snBracePlayerTimeoutBeforeVelCheck)
		nFlags |= FLAG_VEL_CHECK;

	bool bResult = ProcessFinishConditionsBase(pPed, MONITOR_FALL, nFlags);

	return bResult;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMBalance::ProbeForSignificantDrop(CPed* pPed, Vector3& vEdgeLeft, Vector3& vEdgeRight, bool onMovingGround, bool &firstTeeterProbeDetectedDrop, WorldProbe::CShapeTestResults &probeResult)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	phCollider* pCollider = pPed->GetCollider();
	Assert(pCollider);
	Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	// This is a unit vector in the direction the ped is moving.
	Vector3 vPedDirNormal;
	pPed->GetRagdollInst()->GetCOMVel(vPedDirNormal);
	if (onMovingGround)
		vPedDirNormal -= VEC3V_TO_VECTOR3(pCollider->GetReferenceFrameVelocity());
	vPedDirNormal.z = 0.0f;
	float fSpeed = vPedDirNormal.Mag();
	vPedDirNormal.NormalizeSafe();
	// Find a unit vector which is orthogonal to the direction of motion (and to the left).
	Vector3 vPedDirOrthoL(-vPedDirNormal.y, vPedDirNormal.x, 0.0f);

	static dev_float sfCriticalDepth = 1.5f; // Critical height difference from pelvis to ground to consider a ledge.
	static dev_float sfEdgeHorDistThreshold1 = 0.50f; // The horizontal distance from the ped's centre of probe1.
	static dev_float sfLengthOfEdge = 1.5f;
	const float fPelvisToGround = 0.5f;

	// Scale the thresholds by the speed of the ped
	float fEdgeHorDistThreshold1 = sfEdgeHorDistThreshold1; // The horizontal distance from the ped's centre of probe1.
	static float thredholdScaleMaxSpeed = 1.5f;
	static float thredholdScaleMin = 0.3f;
	if (fSpeed < thredholdScaleMaxSpeed)
	{
		float thresholdScale = Max(fSpeed / thredholdScaleMaxSpeed, thredholdScaleMin);
		fEdgeHorDistThreshold1 *= thresholdScale;
	}

	// Send off a asynch probes every few frames 
	if (probeResult.GetResultsReady() && firstTeeterProbeDetectedDrop)
	{
		if (probeResult.GetNumHits() > 0)
		{
			static float f4 = 0.33f;
			Vector3 vEdgeCentre = probeResult[0].GetHitPosition();
			vEdgeCentre.z += f4;
			Vector3 vPedDirOrthoL2(-probeResult[0].GetHitNormal().y, probeResult[0].GetHitNormal().x, 0.0f);
			vEdgeLeft.Add(vEdgeCentre, vPedDirOrthoL2*(sfLengthOfEdge/2.0f));
			vEdgeRight.Add(vEdgeCentre, vPedDirOrthoL2*-(sfLengthOfEdge/2.0f));
		}
		else
		{
			Vector3 vProbe1Pos;	vProbe1Pos.Add(vPedPos, vPedDirNormal*fEdgeHorDistThreshold1/2.0f);
			vEdgeLeft.Add(vProbe1Pos, vPedDirOrthoL*(sfLengthOfEdge/2.0f));
			vEdgeRight.Add(vProbe1Pos, vPedDirOrthoL*-(sfLengthOfEdge/2.0f));
		}
#if __BANK
		if(CNmDebug::ms_bDrawTeeterEdgeDetection)
		{
			grcDebugDraw::Line(vEdgeLeft, vEdgeRight, Color_white);
		}
#endif // __BANK

		probeResult.Reset();
		firstTeeterProbeDetectedDrop = false;

		return true;
	}
	else if (probeResult.GetResultsReady())
	{
		if(probeResult.GetNumHits() > 0)
		{
#if DEBUG_DRAW
			if(CNmDebug::ms_bDrawTeeterEdgeDetection)
			{
				grcDebugDraw::Sphere(probeResult[0].GetHitPosition(), 0.05f, Color_red, true);
			}
#endif // DEBUG_DRAW
			firstTeeterProbeDetectedDrop = false;
			probeResult.Reset();
			return false;
		}

		probeResult.Reset();
		firstTeeterProbeDetectedDrop = true;

		static float f1 = 0.8f;
		static float f2 = 1.0f;
		static float f3 = 0.2f;
		Vector3 vProbe4End = vPedPos - vPedDirNormal*f3;
		vProbe4End.z -= (fPelvisToGround + f1);
		Vector3 vProbe4Start = vProbe4End + vPedDirNormal*(fEdgeHorDistThreshold1+f2);

#if DEBUG_DRAW
		if(CNmDebug::ms_bDrawTeeterEdgeDetection)
		{
			grcDebugDraw::Line(vProbe4Start, vProbe4End, Color_red);
			grcDebugDraw::Sphere(vProbe4Start, 0.03f, Color_red, true);
			grcDebugDraw::Sphere(vProbe4End, 0.03f, Color_red, true);
		}
#endif // DEBUG_DRAW

		// Probe towards and under the feet to get better edge data
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetStartAndEnd(vProbe4Start, vProbe4End);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE);
		probeDesc.SetExcludeInstance(pPed->GetCurrentPhysicsInst());

		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}
	else if (!probeResult.GetWaitingOnResults() && (onMovingGround || (fwTimer::GetFrameCount() % NUM_FRAMES_PER_TEETER_PROBE == 0)))
	{
		firstTeeterProbeDetectedDrop = false;

		// Compute quantities derived from the above parameters.
		float fTheta1 = tan(fEdgeHorDistThreshold1/fPelvisToGround);
		float fProbeHorDist1 = sfCriticalDepth*atan(fTheta1);

		// Probe 1:
		Vector3 vProbeStart = vPedPos;
		Vector3 vProbe1End;
		vProbe1End.Add(vPedDirNormal*fProbeHorDist1, vProbeStart);
		vProbe1End.z -= sfCriticalDepth;

#if DEBUG_DRAW
		if(CNmDebug::ms_bDrawTeeterEdgeDetection)
		{
			grcDebugDraw::Sphere(vProbeStart, 0.03f, Color_blue, true);
			grcDebugDraw::Sphere(vProbe1End, 0.03f, Color_blue, true);
			grcDebugDraw::Line(vProbeStart, vProbe1End, Color_yellow);
		}
#endif // DEBUG_DRAW

		// Probe along the horizontal lines defined above looking for scenery.
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&probeResult);
		probeDesc.SetStartAndEnd(vProbeStart, vProbe1End);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE);
		probeDesc.SetExcludeInstance(pPed->GetCurrentPhysicsInst());

		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBalance::ProbeForStairsOrSlope(CPed* pPed, bool &bOnStairs, bool &bSteepSlope)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Check for stairs or a steep slope
	Matrix34 ragdollCompMatrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());
	pPed->GetRagdollComponentMatrix(ragdollCompMatrix, RAGDOLL_SPINE3);
	WorldProbe::CShapeTestFixedResults<> probeResult;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(ragdollCompMatrix.d, ragdollCompMatrix.d - Vector3(0.0f, 0.0f, 2.5f));
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetExcludeEntity(NULL);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		if(PGTAMATERIALMGR->GetPolyFlagStairs(probeResult[0].GetHitMaterialId()))
			bOnStairs = true;
		else
		{
			float angle = (180.0f / PI * AcosfSafe(probeResult[0].GetHitNormal().z));
			static float steep = 35.0f;
			if (angle >= steep)
				bSteepSlope = true;
		}
	}
}

#if !__FINAL
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMBalance::Debug() const
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_pGrabHelper)
		m_pGrabHelper->DrawDebug();
}
#endif
