// Filename   :	TaskNMShot.cpp
// Description:	Natural Motion shot class (FSM version)

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "crskeleton/Skeleton.h"
#include "fragment/Cache.h"
#include "fragment/Instance.h"
#include "fragment/Type.h"
#include "fragment/TypeChild.h"
#include "fragmentnm/messageparams.h"
#include "fragmentnm/manager.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/shapetest.h"
#include "phbound/boundcomposite.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "fwanimation/pointcloud.h"
#include "grcore/debugdraw.h"
#include "fwmaths/Angle.h"

// Game headers

#include "Animation/FacialData.h"
#include "animation/AnimBones.h"
#include "audio/speechaudioentity.h"
#include "camera/CamInterface.h"
#include "Event/EventDamage.h"
#include "modelinfo/PedModelInfo.h"
#include "Network/NetworkInterface.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPlacement.h"
#include "PedGroup/PedGroup.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Physics/RagdollConstraints.h"
#include "scene/world/GameWorld.h"
#include "Task/Animation/TaskReachArm.h"
#include "Task/Combat/TaskDamageDeath.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskAnimatedFallback.h"
#include "Task/Movement/Jumping/TaskInAir.h"
#include "Task/Physics/TaskNMBalance.h"
#include "Task/Physics/TaskNMShot.h"
#include "vehicles/vehicle.h"
#include "Vfx/Misc/Fire.h"
#include "Weapons/Weapon.h"
#include "Renderer/Water.h"
#include "Task/Physics/NmDebug.h"
#include "fwmaths/random.h"


AI_OPTIMISATIONS()

#define JUST_STARTED_THRESHOLD 50

#if ART_ENABLE_BSPY
extern const char* parser_RagdollComponent_Strings[];
#endif

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMShot::Tunables CTaskNMShot::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMShot, 0xc9cd39f1);

bool CTaskNMShot::Tunables::HitPointRandomisation::GetRandomHitPoint(Vector3& vRandomHitPoint, const CPed* pPed, RagdollComponent eComponent, bool bBiasSideLeft) const
{
	if (m_Enable && ShouldRandomiseHitPoint(eComponent))
	{
		Vector3 topExtents[2];
		Vector3 bottomExtents[2];

		float biasSideLeft = 1.0f;
		float biasSideRight = 1.0f;
		if (bBiasSideLeft)
		{
			biasSideLeft = m_BiasSide;
		}
		else
		{
			biasSideRight = m_BiasSide;
		}

		Matrix34 mat(M34_IDENTITY);
		pPed->GetRagdollComponentMatrix(mat,m_TopComponent);

		topExtents[0] = mat.d - (mat.b*m_TopSpread*biasSideLeft);
		topExtents[1] = mat.d + (mat.b*m_TopSpread*biasSideRight);

		pPed->GetRagdollComponentMatrix(mat, m_BottomComponent);

		bottomExtents[0] = mat.d - (mat.b*m_BottomSpread*biasSideLeft);
		bottomExtents[1] = mat.d + (mat.b*m_BottomSpread*biasSideRight);

#if __BANK
		TUNE_GROUP_BOOL(NM_SHOT, bRenderShotRandomisation, false);
		TUNE_GROUP_FLOAT(NM_SHOT, fShotRandomisationDebugSphereRadius, 0.05f,0.001f, 0.5f, 0.001f);
		TUNE_GROUP_FLOAT(NM_SHOT, fShotRandomisationDebugSphereForward, -0.15f,-0.5f, 0.5f, 0.001f);
		TUNE_GROUP_INT(NM_SHOT, iShotRandomisationDebugSphereLifeTime, 5,1, 100,1);
		Matrix34 matSpine0;
		pPed->GetRagdollComponentMatrix(matSpine0, RAGDOLL_SPINE0);
		Vector3 forward = matSpine0.c*fShotRandomisationDebugSphereForward;
		if (bRenderShotRandomisation)
		{
			// render the extents			
			grcDebugDraw::Quad(VECTOR3_TO_VEC3V(topExtents[0]+forward), VECTOR3_TO_VEC3V(topExtents[1]+forward), 
				VECTOR3_TO_VEC3V(bottomExtents[0]+forward), VECTOR3_TO_VEC3V(bottomExtents[1]+forward), Color_yellow, true, true, 5);
		}
#endif //__BANK

		topExtents[0] = Lerp(fwRandom::GetRandomNumberInRange(0.0f, 1.0f), topExtents[0], topExtents[1]);
		bottomExtents[0] = Lerp(fwRandom::GetRandomNumberInRange(0.0f, 1.0f), bottomExtents[0], bottomExtents[1]);

		topExtents[0] = Lerp(fwRandom::GetRandomNumberInRange(0.0f, 1.0f), topExtents[0], bottomExtents[0]);

		vRandomHitPoint = Lerp(m_Blend, vRandomHitPoint, topExtents[0]);

#if __BANK
		if (bRenderShotRandomisation)
		{
			// render the final target
			grcDebugDraw::Sphere(vRandomHitPoint+forward, fShotRandomisationDebugSphereRadius, Color_purple, true, 5);
		}
#endif //__BANK

		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////
// CClonedNMShotInfo
//////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClonedNMShotInfo::CClonedNMShotInfo(CEntity* pShotBy, u32 nWeaponHash, int nComponent, const Vector3& vecHitPositionLocalSpace, 
									 const Vector3& vecHitImpulseWorldSpace, const Vector3& vecHitPolyNormalWorldSpace)
									 ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
									 : m_nWeaponHash(nWeaponHash)
									 , m_nComponent((u8)nComponent)
									 , m_vecOriginalHitPositionLocalSpace(vecHitPositionLocalSpace)
									 , m_vecOriginalHitPolyNormalWorldSpace(vecHitPolyNormalWorldSpace)
									 , m_vecOriginalHitImpulseWorldSpace(vecHitImpulseWorldSpace)
									 , m_bHasShotByEntity(false)
{

	Assertf(nComponent >=0 && nComponent < ((1<<SIZEOF_COMPONENT)-1), "CClonedNMShotInfo: nComponent (%d) out of synced range", nComponent);

	if (pShotBy && pShotBy->GetIsDynamic() && static_cast<CDynamicEntity*>(pShotBy)->GetNetworkObject())
	{
		m_shotByEntity = static_cast<CDynamicEntity*>(pShotBy)->GetNetworkObject()->GetObjectID();
		m_bHasShotByEntity = true;
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClonedNMShotInfo::CClonedNMShotInfo()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
:  m_nWeaponHash(0)
, m_nComponent(0)
, m_vecOriginalHitPositionLocalSpace(Vector3(0.0f, 0.0f, 0.0f))
, m_vecOriginalHitPolyNormalWorldSpace(Vector3(0.0f, 0.0f, 0.0f))
, m_vecOriginalHitImpulseWorldSpace(Vector3(0.0f, 0.0f, 0.0f))
, m_bHasShotByEntity(false)
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskFSMClone *CClonedNMShotInfo::CreateCloneFSMTask()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	CEntity *pShotByEntity = NULL;

	if (m_bHasShotByEntity)
	{
		netObject* pObj = NetworkInterface::GetNetworkObject(m_shotByEntity);

		if (pObj)
		{
			pShotByEntity = pObj->GetEntity();
		}
	}

	return rage_new CTaskNMShot(NULL, 1000, 10000, pShotByEntity, m_nWeaponHash, (int)m_nComponent, m_vecOriginalHitPositionLocalSpace, m_vecOriginalHitImpulseWorldSpace, m_vecOriginalHitPolyNormalWorldSpace, 0, false, false, true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMShot ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMShot::CTaskNMShot(CPed* pPed, u32 nMinTime, u32 nMaxTime, CEntity* pShotBy, u32 nWeaponHash, int nComponent,
						 const Vector3& vecHitPos, const Vector3& vecHitImpulse, const Vector3& vecHitPolyNormal, s32 hitCountThisPerformance, 
						 bool bShotgunMessageSent, bool bHeldWeapon, bool hitPosInLocalSpace, u32 lastStandStartTimeMS, 
						 u32 lastConfigureBulletsCall, bool bLastStandActive, bool bKnockDownStarted)
						 ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
						 : CTaskNMBehaviour(nMinTime, nMaxTime),
						 m_eState(State_STANDING),
						 m_threatenedHelper(),
						 m_pShotBy(pShotBy),
						 m_bUpright(false),
						 m_nComponent(nComponent),
						 m_nWeaponHash(nWeaponHash),
						 m_vecOriginalHitPositionLocalSpace(vecHitPos),       // World space from weapon system, local space if cloned
						 m_vecOriginalHitPolyNormalWorldSpace(vecHitPolyNormal),  // Always world space
						 m_vecOriginalHitImpulseWorldSpace(vecHitImpulse),   // Always world space
						 m_nTimeToStopBlindFire(0),
						 m_nTimeTillBlindFireNextShot(0),
						 m_bUsingBalance(false),
						 m_bBalanceFailed(false),
						 m_bKillShotUsed(false),
						 m_bUseHeadLook(true),
						 m_bUsePointGun(false),
						 m_LastConfigureBulletsCall(lastConfigureBulletsCall),
						 m_BiasSide(false),
						 m_BiasSideTime(0),
						 m_ElectricDamage(false),
						 m_StaggerFallStarted(false),
						 m_ThrownOutImpulseCount(0),
						 m_bSprinting(false),
						 m_CountdownToCatchfall(0),
						 m_nLookAtArmWoundTime(0),
						 m_LastStandStartTimeMS(lastStandStartTimeMS),
						 m_healthRatio(1.0f),
						 m_bLastStandActive(bLastStandActive),
						 m_bKnockDownStarted(bKnockDownStarted),
						 m_hitCountThisPerformance((s16)hitCountThisPerformance),
						 m_RapidFiring(false),
						 m_RapidFiringBoostFrame(false),
						 m_stepInInSupportApplied(false),
						 m_bFallingToKnees(false),
						 m_bMinTimeExtended(false),
						 m_ShotgunMessageSent(bShotgunMessageSent),
						 m_HeldWeapon(bHeldWeapon),
						 m_SettingShotMessageForShotgun(false),
						 m_eType(SHOT_DEFAULT),
						 m_ReachingWithLeft(false),
						 m_ReachingWithRight(false),
						 m_bTeeterInitd(false),
						 m_DoLegShotFall(false),
						 m_FallingOverWall(false),
						 m_probeResult(1),
						 m_vEdgeLeft(Vector3::ZeroType),
						 m_vEdgeRight(Vector3::ZeroType),
						 m_FirstTeeterProbeDetectedDrop(false),
						 m_DisableReachForWoundTimeMS(0),
						 m_WasCrouchedOrInLowCover(false)
#if __DEV
						 , m_selected(false)
#endif
{
#if !__FINAL
	m_strTaskAndStateName = "NMShot";
#endif // !__FINAL

	m_probeResult.Reset();

	m_ShotsPerBurst = (u8)fwRandom::GetRandomNumberInRange(sm_Tunables.m_Impulses.m_RapidFireBoostShotMinRandom, sm_Tunables.m_Impulses.m_RapidFireBoostShotMaxRandom);

	// Randomly pick an initial side
	m_BiasSide = fwRandom::GetRandomTrueFalse();

#if __ASSERT
	Assertf(m_nWeaponHash != 0, "CTaskNMShot created without a valid weapon hash (%s)", (pPed && pPed->GetNetworkObject()) ? pPed->GetNetworkObject()->GetLogName() : "non-networked ped");

	if (m_nWeaponHash != 0)
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
		Assertf(pWeaponInfo, "CTaskNMShot created without a valid weapon info - weapon hash = 0x%x (%s)", m_nWeaponHash, (pPed && pPed->GetNetworkObject()) ? pPed->GetNetworkObject()->GetLogName() : "non-networked ped");
	}
#endif

	if (!hitPosInLocalSpace)
	{
		if (pPed != NULL)
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
			if (AssertVerify(pWeaponInfo != NULL))
			{
				Tunables::HitPointRandomisation hitRandomisation = IsAutomatic(pWeaponInfo->GetNmShotTuningSet()) ? sm_Tunables.m_HitRandomisationAutomatic : sm_Tunables.m_HitRandomisation;
				
				// HACK! Always bias the left side in MP. When a ped is being shot while aiming at the shooter and they are hit in their right side then tend to always end up
				// tripping and falling over immediately because of the way the aiming pose positions the feet. Forcing the initial shot to always bias the left side forces
				// the aiming ped to open up their body
				if (pPed->IsPlayer() && pPed->GetNetworkObject() != NULL && static_cast<CNetObjPlayer*>(pPed->GetNetworkObject())->HasValidTarget() && m_pShotBy != NULL)
				{
					Vec3V vAimDirection = NormalizeSafe(Subtract(VECTOR3_TO_VEC3V(pPed->GetWeaponManager()->GetWeaponAimPosition()), pPed->GetTransform().GetPosition()), Vec3V(V_X_AXIS_WZERO));
					Vec3V vToShooterDirection = NormalizeSafe(Subtract(m_pShotBy->GetTransform().GetPosition(), pPed->GetTransform().GetPosition()), Vec3V(V_X_AXIS_WZERO));
					ScalarV fDotProduct = Dot(vAimDirection, vToShooterDirection);
					static float fAimingTowardsThreshold = 0.5f;
					if (fDotProduct.Getf() > fAimingTowardsThreshold)
					{
						m_BiasSide = true;
						hitRandomisation.m_BiasSide = 0.0f;
					}
				}
				
				hitRandomisation.GetRandomHitPoint(m_vecOriginalHitPositionLocalSpace, pPed, static_cast<RagdollComponent>(nComponent), m_BiasSide);
			}
		}

		Matrix34 ragdollComponentMatrix;
		if(taskVerifyf(pPed && pPed->GetRagdollComponentMatrix(ragdollComponentMatrix, m_nComponent), "CTaskNMShot::CTaskNMShot: Could not switch hit position to local space"))
		{
			Assert(ragdollComponentMatrix.IsOrthonormal());
			ragdollComponentMatrix.UnTransform(m_vecOriginalHitPositionLocalSpace);
		}
	}

	m_vecOriginalHitPositionBoneSpace.ZeroComponents();

	// CheckInputNormal
	CheckInputNormalAndHitPosition();

	ResetStartTime();

	if (!m_HeldWeapon)
		m_HeldWeapon = pPed && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject() ? true : false;

	// Need to check this on construction as the values won't be valid once the task starts
	if (pPed)
	{
		if (pPed->GetIsCrouching())
		{
			m_WasCrouchedOrInLowCover = true;
		}
		else if (pPed->GetIsInCover() || pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_COVER) != NULL)
		{
			const CCoverPoint* pCoverPoint = pPed->GetCoverPoint();
			if (pCoverPoint != NULL && pCoverPoint->GetHeight() != CCoverPoint::COVHEIGHT_TOOHIGH)
			{
				m_WasCrouchedOrInLowCover = true;
			}
		}
	}
	SetInternalTaskType(CTaskTypes::TASK_NM_SHOT);
	// Reject any non-high lod NM agents 
	if (!IsHighLODNMAgent(pPed))
		return;

	// both fred and wilma have 21 components
	Assertf(nComponent >= 0 && nComponent < 21, "invalid component index %d", nComponent);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskNMShot::~CTaskNMShot()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
}

//////////////////////////////////////////////////////////////////////////
CTaskNMShot::CTaskNMShot(const CTaskNMShot& otherTask)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
						 : CTaskNMBehaviour(otherTask),
						 m_eState(otherTask.m_eState),
						 m_pShotBy(otherTask.m_pShotBy),
						 m_bUpright(otherTask.m_bUpright),
						 m_nComponent(otherTask.m_nComponent),
						 m_nWeaponHash(otherTask.m_nWeaponHash),
						 m_vecOriginalHitPositionLocalSpace(otherTask.m_vecOriginalHitPositionLocalSpace),       // World space from weapon system, local space if cloned
						 m_vecOriginalHitPolyNormalWorldSpace(otherTask.m_vecOriginalHitPolyNormalWorldSpace),  // Always world space
						 m_vecOriginalHitImpulseWorldSpace(otherTask.m_vecOriginalHitImpulseWorldSpace),   // Always world space
						 m_vecOriginalHitPositionBoneSpace(otherTask.m_vecOriginalHitPositionBoneSpace),
						 m_nTimeToStopBlindFire(otherTask.m_nTimeToStopBlindFire),
						 m_nTimeTillBlindFireNextShot(otherTask.m_nTimeTillBlindFireNextShot),
						 m_bUsingBalance(otherTask.m_bUsingBalance),
						 m_bBalanceFailed(otherTask.m_bBalanceFailed),
						 m_bKillShotUsed(otherTask.m_bKillShotUsed),
						 m_bUseHeadLook(otherTask.m_bUseHeadLook),
						 m_bUsePointGun(otherTask.m_bUsePointGun),
						 m_LastConfigureBulletsCall(otherTask.m_LastConfigureBulletsCall),
						 m_BiasSide(otherTask.m_BiasSide),
						 m_BiasSideTime(otherTask.m_BiasSideTime),
						 m_ElectricDamage(otherTask.m_ElectricDamage),
						 m_StaggerFallStarted(otherTask.m_StaggerFallStarted),
						 m_ThrownOutImpulseCount(otherTask.m_ThrownOutImpulseCount),
						 m_bSprinting(otherTask.m_bSprinting),
						 m_CountdownToCatchfall(otherTask.m_CountdownToCatchfall),
						 m_nLookAtArmWoundTime(otherTask.m_nLookAtArmWoundTime),
						 m_LastStandStartTimeMS(otherTask.m_LastStandStartTimeMS),
						 m_healthRatio(otherTask.m_healthRatio),
						 m_bLastStandActive(otherTask.m_bLastStandActive),
						 m_bKnockDownStarted(otherTask.m_bKnockDownStarted),
						 m_hitCountThisPerformance(otherTask.m_hitCountThisPerformance),
						 m_RapidFiring(otherTask.m_RapidFiring),
						 m_RapidFiringBoostFrame(otherTask.m_RapidFiringBoostFrame),
						 m_stepInInSupportApplied(otherTask.m_stepInInSupportApplied),
						 m_bFallingToKnees(otherTask.m_bFallingToKnees),
						 m_bMinTimeExtended(otherTask.m_bMinTimeExtended),
						 m_ShotgunMessageSent(otherTask.m_ShotgunMessageSent),
						 m_ShotsPerBurst(otherTask.m_ShotsPerBurst),
						 m_HeldWeapon(otherTask.m_HeldWeapon),
						 m_SettingShotMessageForShotgun(otherTask.m_SettingShotMessageForShotgun),
						 m_eType(otherTask.m_eType),
						 m_ReachingWithLeft(otherTask.m_ReachingWithLeft),
						 m_ReachingWithRight(otherTask.m_ReachingWithRight),
						 m_bTeeterInitd(otherTask.m_bTeeterInitd),
						 m_DoLegShotFall(otherTask.m_DoLegShotFall),
						 m_FallingOverWall(otherTask.m_FallingOverWall),
						 m_probeResult(1),
						 m_vEdgeLeft(otherTask.m_vEdgeLeft),
						 m_vEdgeRight(otherTask.m_vEdgeRight),
						 m_FirstTeeterProbeDetectedDrop(otherTask.m_FirstTeeterProbeDetectedDrop),
						 m_DisableReachForWoundTimeMS(otherTask.m_DisableReachForWoundTimeMS),
						 m_WasCrouchedOrInLowCover(otherTask.m_WasCrouchedOrInLowCover)
#if __DEV
						 , m_selected(false)
#endif
{
#if !__FINAL
	m_strTaskAndStateName = "NMShot";
#endif // !__FINAL

	m_threatenedHelper = otherTask.m_threatenedHelper;

	m_probeResult.Reset();

	// CheckInputNormal
	CheckInputNormalAndHitPosition();
	SetInternalTaskType(CTaskTypes::TASK_NM_SHOT);
}


#if DEBUG_DRAW
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::DebugVisualise(const CPed* pPed) const
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{ 
	Vector3 vecWorldHitNormal = m_vecOriginalHitImpulseWorldSpace;
	vecWorldHitNormal.NormalizeFast();

	// Draw bullet impact information:
	static dev_float sfLengthOfImpactTrack = 2.0f;

	Vector3 vecWorldHitPos;
	Matrix34 ragdollComponentMatrix;
	if(pPed && pPed->GetRagdollComponentMatrix(ragdollComponentMatrix, m_nComponent))
	{
		Assert(ragdollComponentMatrix.IsOrthonormal());
		ragdollComponentMatrix.Transform(m_vecOriginalHitPositionLocalSpace, vecWorldHitPos);
	}

	grcDebugDraw::Sphere(vecWorldHitPos, 0.05f, Color_green);
	grcDebugDraw::Line(vecWorldHitPos, vecWorldHitPos + 
		vecWorldHitNormal * sfLengthOfImpactTrack, Color_yellow);
}
#endif // DEBUG_DRAW

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTaskInfo* CTaskNMShot::CreateQueriableState() const
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	return rage_new	CClonedNMShotInfo(m_pShotBy, m_nWeaponHash, m_nComponent, m_vecOriginalHitPositionLocalSpace, m_vecOriginalHitImpulseWorldSpace, m_vecOriginalHitPolyNormalWorldSpace);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::CheckInputNormalAndHitPosition()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Check the bullet impulse vector
	float originalImpulseMag = m_vecOriginalHitImpulseWorldSpace.Mag();
	if (originalImpulseMag != originalImpulseMag || originalImpulseMag*0.0f != 0.0f)
	{
		Warningf("CTaskNMShot::CheckInputNormal - non-finite impulse");
		m_vecOriginalHitImpulseWorldSpace = Vector3(1.0f,0.0f,0.0f);
	}
	else if (originalImpulseMag <= 0.01f)
	{
		m_vecOriginalHitImpulseWorldSpace = Vector3(1.0f,0.0f,0.0f);
	}

	// Check the hit poly normal
	float originalNormalMag = m_vecOriginalHitPolyNormalWorldSpace.Mag();
	if (originalNormalMag != originalNormalMag || originalNormalMag*0.0f != 0.0f)
	{
		Warningf("CTaskNMShot::CheckInputNormal - non-finite normal");
		m_vecOriginalHitPolyNormalWorldSpace = Vector3(1.0f,0.0f,0.0f);
	}
	else if (originalNormalMag <= 0.9f || originalNormalMag >= 1.1f)
	{
		Warningf("CTaskNMShot::CheckInputNormal - non-unit normal.  Normal is %f %f %f", 
			m_vecOriginalHitPolyNormalWorldSpace.x, 
			m_vecOriginalHitPolyNormalWorldSpace.y, 
			m_vecOriginalHitPolyNormalWorldSpace.z);
		m_vecOriginalHitPolyNormalWorldSpace = Vector3(1.0f,0.0f,0.0f);
	}

	// Clamp the local space position
	Assertf(m_vecOriginalHitPositionLocalSpace.Mag2() < LOCAL_POS_MAG_LIMIT*LOCAL_POS_MAG_LIMIT, "m_vecOriginalHitPositionLocalSpace: %.3f, %.3f %.3f: %.3f",
		VEC3V_ARGS(VECTOR3_TO_VEC3V(m_vecOriginalHitPositionLocalSpace)), m_vecOriginalHitPositionLocalSpace.Mag());
	m_vecOriginalHitPositionLocalSpace.ClampMag(0.0f, LOCAL_POS_MAG_LIMIT);
}


#if __BANK
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* CTaskNMShot::GetShotTypeName() const 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	switch (m_eType)
	{
	case SHOT_DEFAULT:
		return "SHOT_DEFAULT";
	case SHOT_NORMAL:
		return "SHOT_NORMAL";
	case SHOT_SHOTGUN:
		return "SHOT_SHOTGUN";
	case SHOT_UNDERWATER:
		return "SHOT_UNDERWATER";
	case SHOT_DANGLING:
		return "SHOT_DANGLING";
	case SHOT_AGAINST_WALL:
		return "SHOT_AGAINST_WALL";
	default:
		AssertMsg(0, "CTaskNMShot::GetShotTypeName() - state not handled");
		return "SHOT_DEFAULT";
	}
}
#endif //__BANK

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFailure(pPed, pFeedbackInterface);

	m_threatenedHelper.BehaviourFailure(pFeedbackInterface);

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		if(!m_bBalanceFailed)// && !pPed->IsPlayer())
		{
			// If the balance has failed, see if we were hit, if we were generate an audio line
			// to say we were knocked over
			if( m_pShotBy && m_pShotBy->GetIsTypePed() )
			{
				CPed* pPedHitBy = static_cast<CPed*>(m_pShotBy.Get());
				if( !pPedHitBy->IsInjured() && pPedHitBy->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE) )
				{
					pPedHitBy->NewSay("MELEE_KNOCK_DOWN");
				}
			}
			pPed->SetPedResetFlag( CPED_RESET_FLAG_KnockedToTheFloorByPlayer, true );

			Matrix34 ragdollCompMatrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());
			pPed->GetRagdollComponentMatrix(ragdollCompMatrix, RAGDOLL_SPINE3);

			// If ped fell on stairs, try and make them roll down the slope.
			WorldProbe::CShapeTestFixedResults<> probeResult;
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(ragdollCompMatrix.d, ragdollCompMatrix.d - Vector3(0.0f, 0.0f, 2.5f));
			probeDesc.SetResultsStructure(&probeResult);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
			probeDesc.SetExcludeEntity(NULL);
			if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
			{
				if(PGTAMATERIALMGR->GetPolyFlagStairs(probeResult[0].GetHitMaterialId()))
				{
					ART::MessageParams msg;
					msg.addBool(NMSTR_PARAM(NM_START), true);
					pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_ROLLDOWN_STAIRS_MSG), &msg);
				}
			}
			m_nSettledStartTime = 0; // reset the settled time, as it was previously tracking how long we'd been standing balanced for
		}
		m_bBalanceFailed = true;
	}
	else if (CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_FALLOVER_WALL_FB))
	{
		m_FallingOverWall = false;

		// Reset the non-falling set falling reaction
		if (m_healthRatio>0.7)
		{
			sm_Tunables.m_ParamSets.m_SetFallingReactionHealthy.Post(*pPed, this);
		}
		else
		{
			sm_Tunables.m_ParamSets.m_SetFallingReactionInjured.Post(*pPed, this);
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourSuccess(pPed, pFeedbackInterface);

	Assert(pPed->GetRagdollConstraintData());

	// If this is a balance success message...
	if((!NetworkInterface::IsGameInProgress() || CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB)) && fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMinTime && !pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled() && !m_FallingOverWall && !m_bFallingToKnees)
	{
		// If we're dead but balanced then start the fall-to-knees behavior and kill the stay-upright behavior!
		// Ignore success messages if the ped is dead - we'll let stillness checks handle that
		if(pPed->GetIsDeadOrDying() || pPed->IsInjured() || IsCloneDying(pPed))
		{
			// The shotgun currently ignores balancer failure on the NM side as a trick to give more control over the legs when knocked off your feet.
			// Since we won't get the typical behaviour success/failure messages, we'll borrow the logic in this part of code to let the game know that we're
			// done with the shot behaviour and ready to relax if a corpse.
			if (m_eType == SHOT_SHOTGUN)
			{
				m_bBalanceFailed = true;
				m_bHasSucceeded = true;
				m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
				m_nSuggestedBlendOption = BLEND_FROM_NM_STANDING;
			}
			else
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_FallToKnees);
				ART::MessageParams msg;
				msg.addBool(NMSTR_PARAM(NM_START), false);
				msg.addBool(NMSTR_PARAM(NM_STAYUPRIGHT_USE_FORCES), false);
				msg.addBool(NMSTR_PARAM(NM_STAYUPRIGHT_USE_TORQUES), false);
				pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_STAYUPRIGHT_MSG), &msg);
				m_bFallingToKnees = true;

				if(pPed && pPed->GetSpeechAudioEntity() && !pPed->GetSpeechAudioEntity()->IsPainPlaying())
				{
					audDamageStats damageStats;
					damageStats.DamageReason = AUD_DAMAGE_REASON_DYING_MOAN;
					pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
				}
			}
		}
		else
		{
			if (!m_bBalanceFailed)
			{
				if (m_nSettledStartTime==0)
				{
					m_nSettledStartTime = fwTimer::GetTimeInMilliseconds();
				}

				if ((fwTimer::GetTimeInMilliseconds() - m_nSettledStartTime) >= sm_Tunables.m_BlendOutDelayStanding)
				{
					m_bHasSucceeded = true;
					m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
					m_nSuggestedBlendOption = BLEND_FROM_NM_STANDING;
				}
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::BehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// HD - this is fired when the shot behaviour switches from looking at the wound -> looking at the shooter
	//		which seems to be the logical time to begin pointing/firing back

	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourEvent(pPed, pFeedbackInterface);

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_SHOT_FB) && !m_threatenedHelper.HasStarted())
	{
		if(m_pShotBy && 
			m_pShotBy->GetIsTypePed() && 
			pPed->GetPedIntelligence()->IsThreatenedBy(*(CPed*)m_pShotBy.Get()))
		{
			CThreatenedHelper::Parameters timingParams;
			timingParams.m_nMinTimeBeforeGunThreaten = static_cast<u16>(sm_Tunables.m_iShotMinTimeBeforeGunThreaten);
			timingParams.m_nMaxTimeBeforeGunThreaten = static_cast<u16>(sm_Tunables.m_iShotMaxTimeBeforeGunThreaten);
			timingParams.m_nMinTimeBeforeFireGun = static_cast<u16>(sm_Tunables.m_iShotMinTimeBetweenFireGun);
			timingParams.m_nMaxTimeBeforeFireGun = static_cast<u16>(sm_Tunables.m_iShotMaxTimeBetweenFireGun);

			if (!pPed->IsPlayer() && m_threatenedHelper.PedCanThreaten(pPed))
			{
				// TBD - distinguish between whether the ped 'dislikes' the driver or really hates
				// his guts enough to empty a clip of ammo into his windscreen
				// ...for now, lets just shoot at him
				m_threatenedHelper.Start(CThreatenedHelper::THREATENED_GUN_THREATEN_AND_FIRE, timingParams);
			}
		}
	}
	else if (CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface,NM_SHOT_REACH_FOR_WOUND_FB))
	{

		m_ReachingWithLeft = pFeedbackInterface->m_args[0].m_bool;
		m_ReachingWithRight = pFeedbackInterface->m_args[1].m_bool;
	}
	else if (CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_FALLOVER_WALL_STATE_FB))
	{
		if (!m_FallingOverWall)
		{
			const CCollisionRecord* pCollisionRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecord();
			if (pCollisionRecord != NULL && pCollisionRecord->m_pRegdCollisionEntity != NULL && pCollisionRecord->m_pRegdCollisionEntity->GetType() == ENTITY_TYPE_VEHICLE)
			{
				// Set appropriate SetFallingReaction params
				sm_Tunables.m_ParamSets.m_SetFallingReactionFallOverVehicle.Post(*pPed, this);
			}
			else
			{
				// Set appropriate SetFallingReaction params
				sm_Tunables.m_ParamSets.m_SetFallingReactionFallOverWall.Post(*pPed, this);
			}
		}

		// Only consider us to be 'falling-over-wall' if actively rolling over a wall
		// This state is used to block transitioning out of the CTaskNMShot task so needs to be set carefully
		// This will get set to false if the fall-over-wall behavior fails or if a timer has passed (maxTimeInFowMS)
		if (pFeedbackInterface->m_args[0].kInt == 2) // ART::NmRsCBUFallOverWall::fow_RollingOverWall
		{
			m_FallingOverWall = true;
		}
	}
	else if (CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_POINTGUN_FB))
	{
		
		if (pFeedbackInterface->m_args[0].m_int==0) // point gun hand state
		{
			if (pFeedbackInterface->m_args[1].m_int==4) // left hand is supporting the weapon
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PointGunLeftHandSupporting, true);
			}
			else
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PointGunLeftHandSupporting, false);
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::BehaviourFinish(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFinish(pPed, pFeedbackInterface);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::GetWorldHitPosition(CPed *pPed, Vector3 &worldHitPos) const
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	Matrix34 ragdollComponentMatrix;
	if(pPed && pPed->GetRagdollComponentMatrix(ragdollComponentMatrix, m_nComponent))
	{
		Assert(ragdollComponentMatrix.IsOrthonormal());
		ragdollComponentMatrix.Transform(m_vecOriginalHitPositionLocalSpace, worldHitPos);
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::UpdateShot(CPed* pThisPed, CEntity* pShotBy, u32 nWeaponHash, int nComponent, const Vector3& vecHitPosn,
							 const Vector3& vecHitImpulse, const Vector3& vecHitPolyNormal)
							 ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Reject any non-high lod NM agents 
	if (!IsHighLODNMAgent(pThisPed))
		return;

	// Note how much time has passed
	u32 timePassed = fwTimer::GetTimeInMilliseconds() - m_nStartTime;

	// Throw out multiple first-frame hits from non-shotguns from the same source
	if (timePassed <= JUST_STARTED_THRESHOLD)
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(nWeaponHash);
		Assert(pWeaponInfo);
		if(pWeaponInfo->GetGroup() != WEAPONGROUP_SHOTGUN && nWeaponHash == m_nWeaponHash && pShotBy == m_pShotBy)
		{
			return;
		}
	}

	Vector3 vecHitPos = vecHitPosn;

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
	atHashString shotTuningSet((u32)0);
	if (AssertVerify(pWeaponInfo != NULL))
	{
		shotTuningSet = pWeaponInfo->GetNmShotTuningSet();
	}
	else
	{
		return;
	}

	if (pThisPed)
	{
		const Tunables::HitPointRandomisation& hitRandomisation = IsAutomatic(shotTuningSet) ? sm_Tunables.m_HitRandomisationAutomatic : sm_Tunables.m_HitRandomisation;

		// Switch sides after a certain amount of time has elapsed
		if (m_BiasSideTime < fwTimer::GetTimeInMilliseconds())
		{
			m_BiasSide = !m_BiasSide;
			m_BiasSideTime = fwTimer::GetTimeInMilliseconds() + hitRandomisation.m_BiasSideTime;
		}

		hitRandomisation.GetRandomHitPoint(vecHitPos, pThisPed, static_cast<RagdollComponent>(nComponent), m_BiasSide);
	}


	Matrix34 ragdollComponentMatrix;
	Vector3 localHitPos = vecHitPos;
	if(pThisPed && pThisPed->GetRagdollComponentMatrix(ragdollComponentMatrix, nComponent))
	{
		Assert(ragdollComponentMatrix.IsOrthonormal());
		ragdollComponentMatrix.UnTransform(localHitPos);
	}

	// Check state
	CheckState(pThisPed);

	// Save off these values to preserve the original hit information (this is necessary for when multiple bullets
	// come in for a single frame, because the original ShotTask call data would get overwritten by these updates
	// before it has a chance to be applied)
	RegdEnt save_pShotBy(m_pShotBy);
	int save_nComponent = m_nComponent;
	u32 save_nWeaponHash = m_nWeaponHash;
	Vector3 save_vecHitPos = m_vecOriginalHitPositionLocalSpace;    
	Vector3 save_vecHitImpulse = m_vecOriginalHitImpulseWorldSpace;  
	Vector3 save_vecHitPolyNormal = m_vecOriginalHitPolyNormalWorldSpace;  

	if(pShotBy != m_pShotBy)
	{
		m_pShotBy = pShotBy;
	}
	m_nWeaponHash = nWeaponHash;
	m_nComponent = nComponent;
	m_vecOriginalHitPositionLocalSpace = localHitPos;
	m_vecOriginalHitImpulseWorldSpace = vecHitImpulse;
	m_vecOriginalHitPolyNormalWorldSpace = vecHitPolyNormal;
	CheckInputNormalAndHitPosition();

	ChooseParameterSet();

	// If the task just started, just add the bullet impulse instead of calling the whole shot message
	if (timePassed > JUST_STARTED_THRESHOLD)
	{	
		CNmMessageList list;

		SetShotMessage(pThisPed, false, list);

		// Player's can get stuck in ragdoll if repeatedly getting shot
		// Only reset the start time (essentially extending the task) if not the player or the player is dead as we want the player to regain control quickly
		if (!pThisPed->IsPlayer() || pThisPed->IsInjured() || IsCloneDying(pThisPed))
		{
			ResetStartTime();
		}

		m_bHasSucceeded = false;
		m_bHasFailed = false;
	}
	else
	{
		// Ignore minigun bullets prior to the behavior start
		if (pWeaponInfo && pWeaponInfo->GetTimeBetweenShots() < 0.04f)
			return;

		// Keep track of the number of hits
		m_hitCountThisPerformance++;

		// Analyze the situation
		GatherSituationalData(pThisPed);

		// Configure bullets if not done very recently
		if ((fwTimer::GetTimeInMilliseconds() - m_LastConfigureBulletsCall) > JUST_STARTED_THRESHOLD)
		{
			static atHashString s_configureBulletsHash("configureBullets",0xE5828EA9);

			// Send the configure bullets message for the appropriate weapon
			Assert(pWeaponInfo);

			atHashString shotTuningSet = pWeaponInfo->GetNmShotTuningSet();
			taskAssertf(shotTuningSet.GetHash(), "No nm shot tuning set specified for weapon %s!", pWeaponInfo->GetName());

			CNmTuningSet* pWeaponSet = sm_Tunables.m_WeaponSets.Get(shotTuningSet);
			taskAssertf(pWeaponSet, "Nm shot tuning set does not exist! Weapon:%s, Set:%s", pWeaponInfo->GetName(), shotTuningSet.TryGetCStr() ? shotTuningSet.GetCStr() : "UNKNOWN");

			if(pWeaponSet)
			{
				pWeaponSet->Post(*pThisPed, this, s_configureBulletsHash);
			}
		}
		HandleConfigureBullets(pThisPed);

		Vector3 objectBehindVictimPos, objectBehindVictimNormal;
		bool wallInImpulsePath = CheckForObjectInPath(pThisPed, objectBehindVictimPos, objectBehindVictimNormal);

		///// APPLY BULLET IMPULSE PARAMETERS MESSAGE:

		HandleApplyBulletImpulse(pThisPed, false, false, wallInImpulsePath);

		// Restore shot values for a possible original shot getting called this frame
		m_pShotBy = save_pShotBy;
		m_nComponent = save_nComponent;
		m_nWeaponHash = save_nWeaponHash;
		m_vecOriginalHitPositionLocalSpace = save_vecHitPos;    
		m_vecOriginalHitImpulseWorldSpace = save_vecHitImpulse;  
		m_vecOriginalHitPolyNormalWorldSpace = save_vecHitPolyNormal;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMShot::HandleRagdollImpact(float fMag, const CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId)
//////////////////////////////////////////////////////////////////////////
{
	// Don't break out of stun gun reactions in favour of vehicle hits
	if (pEntity != NULL && pEntity->GetIsTypeVehicle())
	{
		static const float s_fMinVelocity2 = square(2.0f);
		if (m_ElectricDamage)
		{
			return true;
		}
		else if (static_cast<const CVehicle*>(pEntity)->GetVelocity().Mag2() < s_fMinVelocity2)
		{
			return true;
		}
	}

	return CTaskNMBehaviour::HandleRagdollImpact(fMag, pEntity, vPedNormal, nComponent, nMaterialId);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::ChooseParameterSet()
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_eType = SHOT_NORMAL;
	m_ElectricDamage = false;

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
	Assert(pWeaponInfo);

	if (pWeaponInfo)
	{
		if(pWeaponInfo->GetDamageType() == DAMAGE_TYPE_ELECTRIC)
		{
			m_ElectricDamage = true;
		}
		// shotgun ?
		else if (pWeaponInfo->GetGroup() == WEAPONGROUP_SHOTGUN)
		{
			m_eType = SHOT_SHOTGUN;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::CheckState(CPed *pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// check for water contact
	if (pPed->GetIsInWater())
	{
		m_eState = State_SUBMERGED;

		if (pPed->m_Buoyancy.GetStatus() == PARTIALLY_IN_WATER)
			m_eState = State_PARTIALLY_SUBMERGED;		

		// Turn the weight belt on
		//pPed->m_Buoyancy.SetPedWeightBeltActive(true);

		//static float wb = 1.0f;
		//FRAGNMASSETMGR->SetWeightBeltMultiplier(pPed->GetRagdollInst()->GetARTAssetID(), wb);
		//FRAGNMASSETMGR->SetDragMultiplier(pPed->GetRagdollInst()->GetARTAssetID(), wb);
	}
	else
	{
		m_eState = State_STANDING;

		// Turn the weight belt on
		//pPed->m_Buoyancy.SetPedWeightBeltActive(false);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::GetLocalNormal(CPed* pPed, Vector3 &localHitNormal)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Transform hitPoint information vectors from world space to the space of the hit component. 
	Assert(pPed);
	if(pPed)
	{
		Matrix34 ragdollComponentMatrix;
		if(pPed->GetRagdollComponentMatrix(ragdollComponentMatrix, m_nComponent))
		{
			Assert(ragdollComponentMatrix.IsOrthonormal());

			// Convert the normal at the point of impact to component local space.
			ragdollComponentMatrix.UnTransform3x3(m_vecOriginalHitPolyNormalWorldSpace, localHitNormal);
			localHitNormal.Normalize();

			// Store wound data in the ped
			CPed::WoundData wound;
			wound.valid = true;
			wound.component = m_nComponent;
			wound.localHitLocation = m_vecOriginalHitPositionLocalSpace;
			wound.localHitNormal = localHitNormal;
			wound.impulse = m_vecOriginalHitImpulseWorldSpace.Mag();
			pPed->AddWoundData(wound, m_pShotBy);
		}
		else
			AssertMsg(0, "Matrix not found - 1");
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::SetShotMessage(CPed* pPed, bool bStartBehaviour, CNmMessageList& list)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// make sure the weapon info is valid
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
	Assert(pWeaponInfo);
	if (!pWeaponInfo)
	{
		return;
	}

	// Keep track of the number of hits
	m_hitCountThisPerformance++;

	if (m_eType == SHOT_SHOTGUN)
	{
		if (m_ShotgunMessageSent)
			m_hitCountThisPerformance = 1;
		m_SettingShotMessageForShotgun = true;
		m_ShotgunMessageSent = true; // Set this so that new shotgun pellets during this performance won't recall the whole message
	}

	ChooseParameterSet();

	// Play a pain facial clip (except for electric damage, the parent task will handle that)
	CFacialDataComponent* pFacialData = pPed->GetFacialData();
	if(!m_ElectricDamage && pFacialData)
	{
		// Do not play pain facial anim if it's the local player and they are dying/dead, as we will play a dying anim instead.
		if (!(pPed->IsLocalPlayer() && pPed->GetIsDeadOrDying()))
		{
			pFacialData->PlayPainFacialAnim(pPed);
		}
	}

	Assert(pPed->GetRagdollConstraintData());

	m_nTimeToStopBlindFire = 0;
	m_nTimeTillBlindFireNextShot = 0;

	m_bUsingBalance = false;

#if __DEV
	// Send character health data to RAG
	int numActiveAgents = FRAGNMASSETMGR->GetAgentCapacity(0) - FRAGNMASSETMGR->GetAgentCount(0);
	CPed* pFocusPed = NULL;
	for(int i = 0; i < CDebugScene::FOCUS_ENTITIES_MAX && !pFocusPed; ++i)
	{
		CEntity* pEnt = CDebugScene::FocusEntities_Get(i);
		if( pEnt && pEnt->GetIsTypePed() )
			pFocusPed = static_cast<CPed*>(pEnt);
	}
	m_selected = (pFocusPed && pFocusPed == pPed) || numActiveAgents == 1 ? true : false;
	if (m_selected)
	{
		sm_CharacterHealth = 1.0f;
		sm_CharacterStrength = 1.0f;
		sm_StayUprightMagnitude = 0.0f;
		sm_RigidBodyImpulseRatio = 0.0f;
		sm_ShotRelaxAmount = 0.0f;
	}

	// Show the state of shot relax
	if (m_selected)
	{
			sm_ShotRelaxAmount = 0.0f;
	}
#endif

	// use root velocity rather than plain pPed->GetMoveSpeed()
	// because for the ragdoll this seems to be returning zero at the start
	Vector3 vecRootVelocity = pPed->GetLocalSpeed(Vector3(0.0f,0.0f,0.0f), false, 0);
	// Not considered sprinting any longer if root velocity drops below certain threshold or we're no longer balancing
	static dev_float s_fSprintingVelocityThreshold2 = square(1.5f);

	// Motion data will only be valid when first starting the behavior - after we've established that the
	// ped was trying to sprint we determine below whether the ped should still be considered sprinting based
	// on their root velocity and balance state
	if (bStartBehaviour)
	{
		m_bSprinting = !pPed->GetMotionData()->GetIsLessThanRun() || (vecRootVelocity.XYMag2() > s_fSprintingVelocityThreshold2);
	}

#if ART_ENABLE_BSPY
	SetbSpyScratchpadString(atVarString("Initial sprinting state %d (%f)", m_bSprinting, pPed->GetMotionData()->GetCurrentMbrY()).c_str());
#endif

	if (m_bSprinting && (vecRootVelocity.XYMag2() < s_fSprintingVelocityThreshold2 || m_bBalanceFailed))
	{
		m_bSprinting = false;
	}

#if ART_ENABLE_BSPY
	SetbSpyScratchpadString(atVarString("Calculated sprinting state %d (%f/%f)", m_bSprinting, vecRootVelocity.XYMag(), sqrt(s_fSprintingVelocityThreshold2)).c_str());
#endif

	const bool isMelee = pWeaponInfo->GetIsMelee();

	// Analyze the situation
	GatherSituationalData(pPed);

	AddTuningSet(&sm_Tunables.m_ParamSets.m_Base);

	// Underwater reaction
	if (m_eState == State_PARTIALLY_SUBMERGED || m_eState == State_SUBMERGED)
	{
		HandleUnderwaterReaction(pPed, bStartBehaviour, list);
	}
	// Running Reaction
	else if(m_bSprinting)
	{
		HandleRunningReaction(pPed, bStartBehaviour, list);
	}
	// Melee Reaction
	else if (isMelee)
	{
		HandleMeleeReaction(pPed, bStartBehaviour, list);
	}
	// Reaction when ankles are bound
	else if (pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled())
	{
		HandleBoundAnklesReaction(pPed, bStartBehaviour, list);
	}
	// Shotgun reaction
	else
	{
		// Send the appropriate set for the equipped weapon
		// (the paths above (except for running) deliberately ignore weapon hit type at the moment)
		atHashString shotTuningSet = pWeaponInfo->GetNmShotTuningSet();
		taskAssertf(shotTuningSet.GetHash(), "No nm shot tuning set specified for weapon %s!", pWeaponInfo->GetName());

		CNmTuningSet* pWeaponSet = sm_Tunables.m_WeaponSets.Get(shotTuningSet);
		taskAssertf(pWeaponSet, "Nm shot tuning set does not exist! Weapon:%s, Set:%s", pWeaponInfo->GetName(), shotTuningSet.TryGetCStr() ? shotTuningSet.GetCStr() : "UNKNOWN");

		if(pWeaponSet)
		{
			AddTuningSet(pWeaponSet);
		}

		if (m_eType == SHOT_SHOTGUN)
		{
			// special handling for shotgun hits
			HandleShotgunReaction(pPed, bStartBehaviour, list);
		}
		else
		{
			// General shot reaction
			HandleNormalShotReaction(pPed, bStartBehaviour, list);
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::StartBehaviour(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
#if !__FINAL
	// On restarting the task, reset the task name display for debugging the state.
	m_strTaskAndStateName = "NMShot";
#endif // !__FINAL

	// check for an invalid weapon info and quit (we need a valid one or the task will crash)
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
	if (!pWeaponInfo)
	{
		m_bHasFailed = true;
		return;
	}

	// Check state
	CheckState(pPed);

	// reset the left hand supporting config flag.
	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PointGunLeftHandSupporting, false);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	if(pPed->GetRagdollInst()->GetNMAgentID() > -1)
	{
		// has the ped got a gun in right/left hand?
		bool bWeaponInLeftHand = false;
		bool bWeaponInRightHand = false;

		if(pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject())
		{
			bWeaponInRightHand = true;

			if(pPed->GetNMTwoHandedWeaponBothHandsConstrained()) 
			{
				bWeaponInLeftHand = true;
			}
		}

		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
		if(pWeaponInfo && pWeaponInfo->GetIsMelee())
		{
			// want to use a specific clip pose for melee
			const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(CLIP_SET_STD_PED, CLIP_STD_NM_MELEE);
			pPed->GetRagdollInst()->SetActivePoseFromClip(pClip);
			pPed->GetRagdollInst()->ConfigureCharacter(pPed->GetRagdollInst()->GetNMAgentID(), true, !bWeaponInLeftHand, !bWeaponInRightHand, 0.0f, 0.0f);
		}
	}

	CNmMessageList list;

	SetShotMessage(pPed, true, list);
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMShot::CleanUp()
{
	StartAnimatedReachForWound();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::ControlBehaviour(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	bool isArmoured = IsArmoured(pPed);

	m_threatenedHelper.ControlBehaviour(pPed);

	// Allow the behavior to end if enough time has passed doing the fall over wall without failure (or switching to a new behavior).
	// This is needed as a failsafe since the FOW success message rifes too quickly for our aggressive blendouts, so we don't check for FOW success.
	if (m_FallingOverWall)
	{
		u32 timePassedFOW = fwTimer::GetTimeInMilliseconds() - m_nStartTime;
		static u32 maxTimeInFowMS = 4000; 
		if (timePassedFOW > maxTimeInFowMS)
		{
			m_FallingOverWall = false;

			// Reset the non-falling set falling reaction
			if (m_healthRatio>0.7)
			{
				sm_Tunables.m_ParamSets.m_SetFallingReactionHealthy.Post(*pPed, this);
			}
			else
			{
				sm_Tunables.m_ParamSets.m_SetFallingReactionInjured.Post(*pPed, this);
			}
		}

		if (GetControlTask())
			GetControlTask()->SetFlag(CTaskNMControl::BLOCK_DRIVE_TO_GETUP);
	}
	else
	{
		if (!m_ElectricDamage)
		{
			if (GetControlTask())
				GetControlTask()->ClearFlag(CTaskNMControl::BLOCK_DRIVE_TO_GETUP);
		}
	}

	if (m_ElectricDamage)
	{
		// Ensure the ped plays their electrocuted facial anim
		CFacialDataComponent* pFacialData = pPed->GetFacialData();
		if (pFacialData)
		{
			pFacialData->RequestFacialIdleClip(CFacialDataComponent::FICT_Electrocution);
		}
	}

	if (m_eState != State_TEETERING && !m_bTeeterInitd && !m_FallingOverWall && !(m_eState == State_PARTIALLY_SUBMERGED || m_eState == State_SUBMERGED))
	{
		// Probe for a significant drop in the direction of travel.
		if(CTaskNMBalance::ProbeForSignificantDrop(pPed, m_vEdgeLeft, m_vEdgeRight, OnMovingGround(pPed), m_FirstTeeterProbeDetectedDrop, m_probeResult))
		{
			m_FirstTeeterProbeDetectedDrop = false;
			m_eState = State_TEETERING;
		}
	}

	// Get the health ratio
	CalculateHealth(pPed);

	// Enable "stepIfInSupport" after the first few frames of a shot reaction to compensate for feet that are slightly off the ground on 
	// activation due to animation.
	u32 s_timeToStepIfInSupport = 200;
	if (!m_stepInInSupportApplied && (fwTimer::GetTimeInMilliseconds() - m_nStartTime) > s_timeToStepIfInSupport)
	{
		ART::MessageParams msg;
		msg.addBool(NMSTR_PARAM(NM_CONFIGURE_BALANCE_STEP_IF_IN_SUPPORT), true);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_CONFIGURE_BALANCE_MSG), &msg);
		m_stepInInSupportApplied = true;
	}

	// Call staggerfall if the "Last Stand" mode has ended
	if (m_bLastStandActive)
	{
		Assert(!m_bKnockDownStarted);
		u32 lastStandMaxTimeBetweenHitsMS = (u32) (CTaskNMBehaviour::sm_LastStandMaxTimeBetweenHits * 1000.0f);
		float timeSinceLastStandStart = ((float)fwTimer::GetTimeInMilliseconds() - m_LastStandStartTimeMS) / 1000.0f;
		float lastStandMaxTotalTime = isArmoured ? sm_Tunables.m_LastStandMaxArmouredTotalTime : sm_Tunables.m_LastStandMaxTotalTime;
		m_bLastStandActive = (fwTimer::GetTimeInMilliseconds() - m_LastConfigureBulletsCall) < lastStandMaxTimeBetweenHitsMS && 
			timeSinceLastStandStart < lastStandMaxTotalTime;
		if (!m_bLastStandActive)
		{
			// Turn off last stand
			ART::MessageParams msg;
			msg.addBool(NMSTR_PARAM(NM_START), false);
			msg.addBool(NMSTR_PARAM(NM_STAYUPRIGHT_LAST_STAND_MODE), false);
			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_STAYUPRIGHT_MSG), &msg);

			if (m_bFallingToKnees || (m_healthRatio == 0.0f && g_DrawRand.GetRanged(0.0f, 1.0f) <= sm_Tunables.m_ChanceOfFallToKneesAfterLastStand))
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_FallToKnees);
				m_bFallingToKnees = true;
				if(pPed && pPed->GetSpeechAudioEntity() && !pPed->GetSpeechAudioEntity()->IsPainPlaying())
				{
					audDamageStats damageStats;
					damageStats.DamageReason = AUD_DAMAGE_REASON_DYING_MOAN;
					pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
				}
			}
			else
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_StaggerFall);
			}

			m_bKnockDownStarted = true;

			// Special handling for knockdown rubber bullet shots
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
			if (pWeaponInfo && (m_ElectricDamage || pWeaponInfo->GetDamageType()==DAMAGE_TYPE_BULLET_RUBBER))
			{
				// MP wants tazed or knockdown rubber bullet victims to always go down quickly, so add in a shot fall to knees to force going down
				AddTuningSet(&sm_Tunables.m_ParamSets.m_RubberBulletKnockdown);
				if (GetControlTask())
					GetControlTask()->SetFlag(CTaskNMControl::BLOCK_DRIVE_TO_GETUP);
			}
		}
	}

	if (m_CountdownToCatchfall > 1 && --m_CountdownToCatchfall == 1)
	{
		// Need to call catchfall explicitly because we're screwing with the balancer to the degree that shot won't call catchfall naturally
		AddTuningSet(&sm_Tunables.m_ParamSets.m_CatchFall);
	}

	if (m_bBalanceFailed && m_DoLegShotFall)
	{
		// Need to call catchfall explicitly because we're screwing with the balancer to the degree that shot won't call catchfall naturally
		AddTuningSet(&sm_Tunables.m_ParamSets.m_CatchFall);
		// mask to just the left arm because we're doing a reach for wound on the leg with the right.
		// Just want the ped to try and protect his head
		ART::MessageParams msg;
		msg.addString(NMSTR_PARAM(NM_CATCHFALL_MASK), "ul");
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_CATCHFALL_MSG), &msg);

		m_DoLegShotFall = false;
	}

	// Extend the time on ground if the balancer has failed and the last hit was electric / rubber bullet 
	if (m_bBalanceFailed && !m_bMinTimeExtended)
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
		if (pWeaponInfo && (m_ElectricDamage || pWeaponInfo->GetDamageType()==DAMAGE_TYPE_BULLET_RUBBER))
		{
			s32 shotReactionTime = (s32)(pWeaponInfo->GetDamageTime()*1000.0f);
			int timePassed = (int) (fwTimer::GetTimeInMilliseconds() - m_nStartTime);

			if (sm_Tunables.m_ReduceDownedTimeByPerformanceTime)
			{
				shotReactionTime -= timePassed;
			}
			shotReactionTime = Max(shotReactionTime, sm_Tunables.m_MinimumDownedTime );

			BumpPerformanceTime(shotReactionTime);
			m_bMinTimeExtended = true;
		}
	}

#if __DEV
	if (m_selected)
	{
			sm_ShotRelaxAmount = 0.0f;
	}
#endif

	// Cache a reference to the clipPose helper class and the NM control task.
	CClipPoseHelper& refClipPoseHelper = GetClipPoseHelper();
	CTaskNMControl *pNmControlTask = GetControlTask();

	// Check for updates to NM's last impulse multiplier
	if (pPed->GetRagdollInst()->GetNMAgentID() >= 0)
	{
		float NMimpulseModifier = pPed->GetRagdollInst()->GetNMImpulseModifierUpdate();
		if (NMimpulseModifier != -1.0f)
		{
			pPed->GetRagdollInst()->SetPendingImpulseModifier(NMimpulseModifier);
			pPed->GetRagdollInst()->ResetNMImpulseModifierUpdate();
		}
	}

	///////////////////////////////////////// BEHAVIOUR STATE MACHINE //////////////////////////////////////////
	switch(m_eState)
	{
		///////////////
	case State_STANDING:
		///////////////
#if !__FINAL
		m_strTaskAndStateName = "NMShot:State_STANDING";
#endif // !__FINAL

		// Depending on the severity of the shot, the ped may be knocked clean off their feet. Switch
		// to the appropriate state based on the NM balancer feedback.
		if(smart_cast<CTaskNMControl*>(pNmControlTask)->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE))
		{
			if (refClipPoseHelper.GetAnimPoseType() != CTaskNMBehaviour::ANIM_POSE_HANDS_CUFFED)
			{
				refClipPoseHelper.Disable(pPed);
				ClearFlag(CTaskNMBehaviour::DO_CLIP_POSE);
			}
			m_eState = State_FALLING;
		}
		else
		{
			m_eState = State_STAGGERING;
		}
		break;

		/////////////////
	case State_STAGGERING:
		/////////////////
#if !__FINAL
		m_strTaskAndStateName = "NMShot:State_STAGGERING";
#endif // !__FINAL


		// Has the ped lost their balance?
		if(sm_Tunables.m_bUseClipPoseHelper && smart_cast<CTaskNMControl*>(pNmControlTask)->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE))
		{
			if (refClipPoseHelper.GetAnimPoseType() != CTaskNMBehaviour::ANIM_POSE_HANDS_CUFFED)
			{
				refClipPoseHelper.Disable(pPed);
				ClearFlag(CTaskNMBehaviour::DO_CLIP_POSE);
			}
			m_eState = State_FALLING;
		}
		break;

		//////////////
	case State_FALLING:
		//////////////
#if !__FINAL
		m_strTaskAndStateName = "NMShot:State_FALLING";
#endif // !__FINAL

		//m_eState = State_STAGGERING;
		break;

		////////////////
	case State_PARTIALLY_SUBMERGED:
		////////////////
#if !__FINAL
		m_strTaskAndStateName = "NMShot:State_PARTIALLY_SUBMERGED";
#endif // !__FINAL
		break;

		////////////////
	case State_SUBMERGED:
		////////////////
#if !__FINAL
		m_strTaskAndStateName = "NMShot:State_SUBMERGED";
#endif // !__FINAL
		break;

		////////////////
	case State_TEETERING:
		////////////////
#if !__FINAL
		m_strTaskAndStateName = "NMShot:State_TEETERING";
#endif // !__FINAL

		if (!m_bTeeterInitd)
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_Teeter);
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
		break;
 
		////////////////
	case State_ON_GROUND:
		////////////////
#if !__FINAL
		m_strTaskAndStateName = "NMShot:State_ON_GROUND";
#endif // !__FINAL


		//m_eState = State_STAGGERING;
		break;


		/////////
	default:
		/////////
		taskAssertf(false, "Unknown state in CTaskNMShot.");
		break;
	}
	/////////////////////////////////////////// END OF STATE MACHINE ///////////////////////////////////////////

	CNmMessageList list;

	ApplyTuningSetsToList(list);
	
	// Update point gun
	if (m_bUsePointGun)
	{
		HandlePointGun(pPed, false, list);
	}

	// Update head look
	if (m_bUseHeadLook)
	{
		HandleHeadLook(pPed, list);
	}

	list.Post(pPed->GetRagdollInst());

	// TODO RA - Rewrite / organise this stuff to fit in with the internal state machine. *****************************
	if(m_pShotBy)
	{
		Vector3 vecLookAtPos = VEC3V_TO_VECTOR3(m_pShotBy->GetTransform().GetPosition());
		if(m_pShotBy->GetIsTypePed())
		{
			vecLookAtPos = ((CPed *)m_pShotBy.Get())->GetBonePositionCached(BONETAG_HEAD);
		}

		if (m_threatenedHelper.HasStarted())
		{
			Vector3 vecShootAtPos = VEC3V_TO_VECTOR3(m_pShotBy->GetTransform().GetPosition());
			if(m_pShotBy->GetIsTypePed())
			{
				vecShootAtPos = ((CPed *)m_pShotBy.Get())->GetBonePositionCached(m_threatenedHelper.getChosenWeaponTargetBoneTag());
			}

			Vector3 worldPosTarget(vecShootAtPos); // used by weapon firing code later
			vecShootAtPos = VEC3V_TO_VECTOR3(m_pShotBy->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vecShootAtPos)));
			m_threatenedHelper.Update(pPed, vecShootAtPos, worldPosTarget, m_pShotBy->GetCurrentPhysicsInst()->GetLevelIndex());
		}
	}

	if(m_DisableReachForWoundTimeMS>0 && fwTimer::GetTimeInMilliseconds()>m_DisableReachForWoundTimeMS)
	{
		ART::MessageParams msg;
		msg.addBool(NMSTR_PARAM(NM_SHOT_REACH_FOR_WOUND), false);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_SHOT_MSG), &msg);
	}

	// should we blind fire like a crazy person?
	if (ShouldBlindFireWeapon(pPed))
	{
		if (fwTimer::GetTimeInMilliseconds() >= m_nTimeTillBlindFireNextShot)
		{
			CObject* weaponObj = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
			if (weaponObj)
			{
				CWeapon* pedWeapon = weaponObj->GetWeapon();

				if (pedWeapon->GetWeaponInfo()->GetIsGun() && 
					pedWeapon->GetState() == CWeapon::STATE_READY)
				{
					Vector3 vecStart, vecEnd;
					Matrix34 m = MAT34V_TO_MATRIX34(weaponObj->GetMatrix());
					pedWeapon->CalcFireVecFromGunOrientation(pPed, m, vecStart, vecEnd);

					if(NetworkInterface::IsGameInProgress())
					{
						if(!ShouldBlindFireWeaponNetworkCheck(pPed, vecStart, vecEnd))
						{
							// firing will potentially hit a player, so we don't as it will be out of sync across the network...
							return;
						}
					}
	
					pedWeapon->Fire(CWeapon::sFireParams(pPed, m, &vecStart, &vecEnd));
					// Use fire intervals defined in the weapon info
					const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pedWeapon->GetWeaponHash());
					Assert(pWeaponInfo);

					ART::MessageParams msgFire;
					msgFire.addBool(NMSTR_PARAM(NM_START), true);
					msgFire.addFloat(NMSTR_PARAM(NM_FIREWEAPON_WEAPON_STRENGTH), pWeaponInfo->GetForceHitPed()*sm_Tunables.m_fFireWeaponStrengthForceMultiplier );
					msgFire.addVector3(NMSTR_PARAM(NM_FIREWEAPON_DIR), -1.0f, 0.0f, 0.0f);
					msgFire.addInt(NMSTR_PARAM(NM_FIREWEAPON_GUN_HAND_ENUM), 1);
					pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_FIREWEAPON_MSG), &msgFire);

					m_nTimeTillBlindFireNextShot = fwTimer::GetTimeInMilliseconds() + static_cast<u32>(1000.0f * pWeaponInfo->GetTimeBetweenShots());
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Predicate class for MP games used below in ControlBehavior
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class NetPlayerDistSortPredicate
{
public:
	NetPlayerDistSortPredicate(Vector3 const& firingPedPos)
	:
		m_firingPedPos(firingPedPos)
	{}

	bool operator()(netPlayer const * leftPlayer, netPlayer const * rightPlayer) const
	{
		Assert(leftPlayer || rightPlayer);

		// array only has one entry...
		if((leftPlayer && !rightPlayer) || (!leftPlayer && rightPlayer))
		{
			return false;
		}
		else if(leftPlayer && rightPlayer)
		{
			const CNetGamePlayer *leftGamePlayer	= SafeCast(const CNetGamePlayer, leftPlayer);
			const CNetGamePlayer *rightGamePlayer	= SafeCast(const CNetGamePlayer, rightPlayer);

			if(Verifyf((leftGamePlayer && rightGamePlayer), "invalid player pointer?"))
			{
				CPed const* leftPlayerPed	= leftGamePlayer->GetPlayerPed();
				CPed const* rightPlayerPed	= rightGamePlayer->GetPlayerPed();

				if(Verifyf((leftPlayerPed && rightPlayerPed), "invalid player ped"))
				{
					Vector3 leftPos		= VEC3V_TO_VECTOR3(leftPlayerPed->GetTransform().GetPosition());
					Vector3 rightPos	= VEC3V_TO_VECTOR3(rightPlayerPed->GetTransform().GetPosition());
					if((leftPos.Dist2(m_firingPedPos)) < (rightPos.Dist2(m_firingPedPos)))
					{
						return true;
					}	
				}
			}
		}

		return false;
	}

private:
	Vector3 m_firingPedPos;
};
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMShot::ShouldBlindFireWeaponNetworkCheck(CPed* pPed, Vector3 const& vecStart, Vector3 const& vecEnd)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// When the ped blind fires, it's totally out of sync between owner and clone 
	// (as they are in different poses in TaskNM) so can cause damage to player on one machine
	// but not the other (or can shoot a ped but not recieve damage) or vice versa.
	// so we have to test if we're going to hit something before blind firing
	// when running a network game - if we are, we don't shoot.
	if(NetworkInterface::IsGameInProgress())
	{
		// players should wild fire appropriately - doesn't get rendered on clone but not much we can do...
		if(pPed && !pPed->IsPlayer())
		{
			CObject* weaponObj = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
			CWeapon* pedWeapon = weaponObj ? weaponObj->GetWeapon() : NULL;

			if(pedWeapon)
			{
				// Grenades / missiles (FIRE_TYPE_PROJECTILE) are synced properly else where with network events so we don't 
				// care about random firing as it will match but bullet types need to be checked
				// if we're actually going to hit a player, then we don't bother as it might cause 
				// a hiccup (player recieves damage even though wasn't hit by shot or 
				// player gets shot in the head but recieves no damage)...
				if((pedWeapon->GetWeaponInfo()->GetFireType() == FIRE_TYPE_INSTANT_HIT) || (pedWeapon->GetWeaponInfo()->GetFireType() == FIRE_TYPE_DELAYED_HIT))
				{
					// go through all *physical* players (i.e people with a connection) 
					unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
					const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

					netPlayer const * distSortedPhysicalPlayers[MAX_NUM_PHYSICAL_PLAYERS] = {0};

					// copy any physical players that are actually playing (i.e actual players in the game) into a temp array.
					int numPlayers = 0;
					for(int p = 0; p < numPhysicalPlayers; ++p)
					{
						const CNetGamePlayer *player = SafeCast(const CNetGamePlayer, allPhysicalPlayers[p]);
						if(player && player->GetPlayerPed())
						{
							distSortedPhysicalPlayers[numPlayers++] = allPhysicalPlayers[p];
						}
					}

					// dist sort the player array...
					Vector3 firingPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
					std::sort(&distSortedPhysicalPlayers[0], &distSortedPhysicalPlayers[0] + numPlayers, NetPlayerDistSortPredicate(firingPedPos));

					// go through all game players
					for(unsigned index = 0; index < numPlayers; index++)
					{
						const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, distSortedPhysicalPlayers[index]);
						CPed const* playerPed = pPlayer->GetPlayerPed();

						if(playerPed)
						{
							//------

							Vector3 playerPos = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());

							//------

							float weaponRange = pedWeapon->GetWeaponInfo()->GetRange();
							if((firingPedPos).Dist2(playerPos) > (weaponRange * weaponRange)) 
							{
								// That's it, we don't have to check anymore peds as we've hit the max range of the bullet...
								break;
							}

							//------

							spdSphere boundingSphere = playerPed->GetBoundSphere();
							
							Vector3 w = (vecEnd - vecStart);
							w.Normalize();
							Vector3 v1;
							Vector3 v2;
							if(fwGeom::IntersectSphereRay(boundingSphere, vecStart, w, v1, v2))
							{
								// we do intersect, no need to test any of the other players...
								return false;
							}
						}
					}
				}
			} 
		} 
	}

	// we're not filtering it out....
	return true;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
bool CTaskNMShot::IsCloneDying(const CPed* pPed) const
//////////////////////////////////////////////////////////////////////////
{
	if (pPed->IsNetworkClone())
	{
		const CTaskDyingDead* pParentDyingTask = static_cast<const CTaskDyingDead*>(FindParentTaskOfType(CTaskTypes::TASK_DYING_DEAD));
		if (pParentDyingTask != NULL)
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMShot::ShouldBlindFireWeapon(CPed* pPed)
//////////////////////////////////////////////////////////////////////////
{

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableBlindFiringInShotReactions))
		return false;

	// don't fire when injured
	if (pPed->IsInjured() || IsCloneDying(pPed))
		return false;

	CObject* weaponObj = pPed->GetWeaponManager() ? pPed->GetWeaponManager()->GetEquippedWeaponObject() : NULL;
	CWeapon* pedWeapon = weaponObj ? weaponObj->GetWeapon() : NULL;

	if (weaponObj)
	{
		// don't fire if we need to reload
		if (pedWeapon && pedWeapon->GetNeedsToReload(true))
		{
			return false;
		}

		// don't fire if hidden (two-handed weapons hidden in CTaskNMControl)
		if (!weaponObj->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY))
		{
			return false;
		}
	}

	if (pPed->IsPlayer())
	{
		// blind fire only when the player is holding the fire trigger
		taskAssert(pPed->GetPlayerInfo());
		if (pPed->GetPlayerInfo()->IsFiring() && !pPed->IsInjured())
			return true;
	}
	else
	{
		bool IsAimingAtTarget = false;

		if (fwTimer::GetTimeInMilliseconds() < m_nTimeToStopBlindFire)
			return true;
		
		CAITarget target;

		if (m_pShotBy)
		{
			target.SetEntity(m_pShotBy);
		}

		IsAimingAtTarget = IsPoseAppropriateForFiring(pPed, target, pedWeapon, weaponObj);

		// only fire if aiming at the target or the ped is blind firing due to an impact
		if ( IsAimingAtTarget )
			return true;
	}

	return false;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMShot::FinishConditions(CPed* pPed)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// try using success and failure messages to end task
	int nFlags = FLAG_SKIP_DEAD_CHECK;
	bool bUnderwater = m_eState == State_PARTIALLY_SUBMERGED || m_eState == State_SUBMERGED;
	if(m_threatenedHelper.HasBalanceFailed() || bUnderwater)
		nFlags |= FLAG_VEL_CHECK;

	if (pPed->GetVelocity().z < -sm_Tunables.m_FallingSpeedForHighFall)
	{
		m_bHasSucceeded = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_HIGH_FALL;
		m_nSuggestedBlendOption = BLEND_FROM_NM_STANDING;
		m_FallingOverWall = false;
	}

	if (CTaskNMBehaviour::sm_Tunables.m_BlockOffscreenShotReactions && !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
	{
		nmTaskDebugf(this, "Finishing task - the ped is off screen");
		m_bHasSucceeded = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
		m_FallingOverWall = false;
		return true;
	}

	if (m_FallingOverWall)
		return false;

	return ProcessFinishConditionsBase(pPed, bUnderwater ? MONITOR_SUBMERGED : MONITOR_FALL, nFlags, bUnderwater ? &sm_Tunables.m_SubmergedBlendOutThreshold : &sm_Tunables.m_BlendOutThreshold.PickBlendOut(pPed));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::HandleApplyBulletImpulse(CPed* pPed, bool bSendNewBulletMessage, bool bStart, bool doKillShot, bool wallInImpulsePath)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	ART::MessageParams msg;

	// Compute local hit position and normal
	Vector3 localHitPos = m_vecOriginalHitPositionLocalSpace;
	Vector3 localHitNormal; 
	GetLocalNormal(pPed, localHitNormal);

	bool disableWallImpulseOnStairs = false;

	nmDebugf1("ApplyBulletImpulse: Incoming impulse to component %d of %f", m_nComponent, m_vecOriginalHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
	SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Initial impulse for hit to %s (%f)", m_hitCountThisPerformance, m_nComponent<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[m_nComponent] : "out of range", m_vecOriginalHitImpulseWorldSpace.Mag()).c_str());
#endif

	// Overriding impulses is for testing only
	if (sm_DoOverrideBulletImpulses)
		ImpulseOverride(pPed);

	Vector3 currentHitPositionWorldSpace;
	GetWorldHitPosition(pPed, currentHitPositionWorldSpace);
	Vector3 currentHitImpulseWorldSpace = m_vecOriginalHitImpulseWorldSpace;

	Vector3 vBulletDir = currentHitImpulseWorldSpace;
	vBulletDir.NormalizeSafe();

	Assert(IsHighLODNMAgent(pPed));
	if (!IsHighLODNMAgent(pPed))
		return;

	bool bSniperShot = false;

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
	if (pWeaponInfo)
	{
		static const atHashString s_sniperSet("Sniper",0x94638209);
		bSniperShot = pWeaponInfo && pWeaponInfo->GetNmShotTuningSet() == s_sniperSet;
	}

	float fUprightDot = CalculateUprightDot(pPed);

	// Factor in the head shot multiplier
	if ((m_nComponent == RAGDOLL_HEAD || m_nComponent == RAGDOLL_NECK) && m_eType != SHOT_SHOTGUN)
	{
		float impulseScale = 1.0f;
		if (sm_Tunables.m_Impulses.m_ScaleHeadShotImpulseWithSpineOrientation && m_bBalanceFailed)
		{
			impulseScale = sm_Tunables.m_Impulses.m_MinHeadShotImpulseMultiplier;
			if (fUprightDot > 0.0f)
			{
				// Decrease the head shot multiplier as the ped falls down
				float maxMultiplier = NetworkInterface::IsGameInProgress() ? sm_Tunables.m_Impulses.m_HeadShotMPImpulseMultiplier : 
					sm_Tunables.m_Impulses.m_HeadShotImpulseMultiplier;
				impulseScale += ((maxMultiplier - sm_Tunables.m_Impulses.m_MinHeadShotImpulseMultiplier) * fUprightDot);
			}
		}
		else
		{
			impulseScale = NetworkInterface::IsGameInProgress() ? sm_Tunables.m_Impulses.m_HeadShotMPImpulseMultiplier : sm_Tunables.m_Impulses.m_HeadShotImpulseMultiplier;
		}
		currentHitImpulseWorldSpace.Scale(impulseScale);
		disableWallImpulseOnStairs = true;
		nmDebugf1("ApplyBulletImpulse: Head shot multiplier - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
		SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Head shot impulse scale %f (%f)", m_hitCountThisPerformance, impulseScale, currentHitImpulseWorldSpace.Mag()).c_str());
#endif
	}

	if (!doKillShot && m_eType != SHOT_SHOTGUN && m_nComponent != RAGDOLL_HEAD && m_nComponent != RAGDOLL_NECK)
	{	
		//scale the impulse based on total damage taken
		float fMinDamageTakenThreshold = sm_Tunables.m_Impulses.m_MinDamageTakenThreshold;
		float fMaxDamageTakenThreshold = sm_Tunables.m_Impulses.m_MaxDamageTakenThreshold;
		float fMinDamageTakenImpulseMult = sm_Tunables.m_Impulses.m_MinDamageTakenImpulseMult;
		float fMaxDamageTakenImpulseMult = sm_Tunables.m_Impulses.m_MaxDamageTakenImpulseMult;
		if (bSniperShot)
		{
			fMinDamageTakenThreshold = sm_Tunables.m_Impulses.m_SniperImpulses.m_MinDamageTakenThreshold;
			fMaxDamageTakenThreshold = sm_Tunables.m_Impulses.m_SniperImpulses.m_MaxDamageTakenThreshold;
			fMinDamageTakenImpulseMult = sm_Tunables.m_Impulses.m_SniperImpulses.m_MinDamageTakenImpulseMult;
			fMaxDamageTakenImpulseMult = sm_Tunables.m_Impulses.m_SniperImpulses.m_MaxDamageTakenImpulseMult;
		}
		if (fMaxDamageTakenThreshold > fMinDamageTakenThreshold)
		{
			float damageRatio = pPed->GetHealthLost()+ pPed->GetArmourLost();
			damageRatio = (damageRatio - fMinDamageTakenThreshold) / (fMaxDamageTakenThreshold - fMinDamageTakenThreshold);
			damageRatio = Clamp(damageRatio, 0.0f, 1.0f);
			float impulseScale = Lerp(damageRatio, fMinDamageTakenImpulseMult, fMaxDamageTakenImpulseMult);
			currentHitImpulseWorldSpace.Scale(impulseScale);
			nmDebugf1("ApplyBulletImpulse: Scale up impulses based on total damage taken - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
			SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Damage taken impulse scale %f (%f)", m_hitCountThisPerformance, impulseScale, currentHitImpulseWorldSpace.Mag()).c_str());
#endif
		}

		// Scale down impulses to armoured guys if not the kill shot
		if (IsArmoured(pPed))
		{
			float armourRatio = GetArmouredRatio(pPed);
			float impulseScale = Lerp(armourRatio, sm_Tunables.m_Impulses.m_MinArmourImpulseMult, sm_Tunables.m_Impulses.m_MaxArmourImpulseMult);
			currentHitImpulseWorldSpace.Scale(impulseScale);
			nmDebugf1("ApplyBulletImpulse: Scale down impulses to armoured guys if not the kill shot - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
			SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Armour impulse scale %f (%f)", m_hitCountThisPerformance, impulseScale, currentHitImpulseWorldSpace.Mag()).c_str());
#endif
		}
		else if (bStart)
		{
			float fMinHealthImpulseMult = sm_Tunables.m_Impulses.m_MinHealthImpulseMult;
			float fMaxHealthImpulseMult = sm_Tunables.m_Impulses.m_MaxHealthImpulseMult;
			if (bSniperShot)
			{
				fMinHealthImpulseMult = sm_Tunables.m_Impulses.m_SniperImpulses.m_MinHealthImpulseMult;
				fMaxHealthImpulseMult = sm_Tunables.m_Impulses.m_SniperImpulses.m_MaxHealthImpulseMult;
			}
			// If we're starting the behaviour, scale up the impulse overall based on total damage taken
			// This gives the appearance of the ped weakening over multiple performances
			float remainingHealthRatio = GetHealthRatio(pPed);
			float impulseScale = Lerp(remainingHealthRatio, fMinHealthImpulseMult, fMaxHealthImpulseMult);
			currentHitImpulseWorldSpace.Scale(impulseScale);
			nmDebugf1("ApplyBulletImpulse: Scale up the impulse overall based on total damage taken at behavior start - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
			SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Health impulse scale %f (%f)", m_hitCountThisPerformance, impulseScale, currentHitImpulseWorldSpace.Mag()).c_str());
#endif
		}
	}

	s32 knockdownCount = -1;
	float killshotImpulseScale;
	
	if (NetworkInterface::IsGameInProgress())
	{
		killshotImpulseScale = m_RapidFiring ? sm_Tunables.m_Impulses.m_DefaultMPRapidFireKillShotImpulseMult : bSniperShot ? sm_Tunables.m_Impulses.m_SniperImpulses.m_DefaultMPKillShotImpulseMult : sm_Tunables.m_Impulses.m_DefaultMPKillShotImpulseMult;
	}
	else
	{
		killshotImpulseScale = m_RapidFiring ? sm_Tunables.m_Impulses.m_DefaultRapidFireKillShotImpulseMult : bSniperShot ? sm_Tunables.m_Impulses.m_SniperImpulses.m_DefaultKillShotImpulseMult : sm_Tunables.m_Impulses.m_DefaultKillShotImpulseMult;
	}

	if (pWeaponInfo)
	{
		knockdownCount = IsArmoured(pPed) && !sm_Tunables.m_AllowArmouredKnockdown ? -1 : pWeaponInfo->GetKnockdownCount();
	}

	// Move shotgun pellets from Spine3 and the neck to Spine2 so that higher shots don't flip the character backwards
	if (m_eType == SHOT_SHOTGUN)
	{
		if (m_nComponent == RAGDOLL_SPINE3 && g_DrawRand.GetRanged(0.0f, 1.0f) < sm_Tunables.m_Impulses.m_ShotgunChanceToMoveSpine3ImpulseToSpine2)
		{
			m_nComponent = RAGDOLL_SPINE2;
			nmDebugf1("ApplyBulletImpulse: Shotgun impulse moved from Spine3 to Spine2 for stability per ShotgunChanceToMoveSpine3ImpulseToSpine2");
#if ART_ENABLE_BSPY
			SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Shotgun impulse moved from RAGDOLL_SPINE3 to RAGDOLL_SPINE2 for stability per ShotgunChanceToMoveSpine3ImpulseToSpine2", m_hitCountThisPerformance).c_str());
#endif
		}
		else if (m_nComponent == RAGDOLL_NECK && g_DrawRand.GetRanged(0.0f, 1.0f) < sm_Tunables.m_Impulses.m_ShotgunChanceToMoveNeckImpulseToSpine2)
		{
			m_nComponent = RAGDOLL_SPINE2;
			nmDebugf1("ApplyBulletImpulse: Shotgun impulse moved from Neck to Spine2 for stability per ShotgunChanceToMoveNeckImpulseToSpine2");
#if ART_ENABLE_BSPY
			SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Shotgun impulse moved from RAGDOLL_NECK to RAGDOLL_SPINE2 for stability per ShotgunChanceToMoveNeckImpulseToSpine2", m_hitCountThisPerformance).c_str());
#endif
		}
		else if (m_nComponent == RAGDOLL_HEAD && g_DrawRand.GetRanged(0.0f, 1.0f) < sm_Tunables.m_Impulses.m_ShotgunChanceToMoveHeadImpulseToSpine2)
		{
			m_nComponent = RAGDOLL_SPINE2;
			nmDebugf1("ApplyBulletImpulse: Shotgun impulse moved from Head to Spine2 for stability per ShotgunChanceToMoveHeadImpulseToSpine2");
#if ART_ENABLE_BSPY
			SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Shotgun impulse moved from RAGDOLL_HEAD to RAGDOLL_SPINE2 for stability per ShotgunChanceToMoveHeadImpulseToSpine2", m_hitCountThisPerformance).c_str());
#endif
		}
	}

	// If the ped has been knocked down his NM health is 0.0
	if ((knockdownCount>=0) && m_hitCountThisPerformance >= knockdownCount && !bStart)
	{
		m_healthRatio = 0.0f;
	}

	// If just killed by non-rapid fire, scale by killshotImpulseScale 
	if (doKillShot)
	{
		currentHitImpulseWorldSpace.Scale(killshotImpulseScale);
		nmDebugf1("ApplyBulletImpulse: killshotImpulseScale - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
		SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Kill-shot impulse scale %f (%f)", m_hitCountThisPerformance, killshotImpulseScale, currentHitImpulseWorldSpace.Mag()).c_str());
#endif

		if (pWeaponInfo != NULL)
		{
			currentHitImpulseWorldSpace.Scale(pWeaponInfo->GetKillShotImpulseScale());

			nmDebugf1("ApplyBulletImpulse: weapon kill shot impulse scale - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
			SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Weapon kill-shot impulse scale %f (%f)", m_hitCountThisPerformance, pWeaponInfo->GetKillShotImpulseScale(), currentHitImpulseWorldSpace.Mag()).c_str());
#endif
		}

		disableWallImpulseOnStairs = true;
	}
	else
	{
		// Scale the impulse up every few frames for SMG rapid firing
		// Only apply the scale while the balancer hasn't failed or the character is still somewhat upright - avoids peds getting tossed back too violently by automatic weapons
		if (m_RapidFiringBoostFrame && (!m_bBalanceFailed || fUprightDot > sm_Tunables.m_Impulses.m_LegShotFallRootImpulseMinUpright))
		{
			currentHitImpulseWorldSpace.Scale(sm_Tunables.m_Impulses.m_RapidFireBoostShotImpulseMult);
			nmDebugf1("ApplyBulletImpulse: Scale the impulse up every few frames for SMG rapid firing - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
			SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Rapid-fire boost shot impulse scale %f (%f)", m_hitCountThisPerformance, sm_Tunables.m_Impulses.m_RapidFireBoostShotImpulseMult, currentHitImpulseWorldSpace.Mag()).c_str());
#endif

			// Re-Randomize how many frames until the next burst
			m_ShotsPerBurst = (u8)fwRandom::GetRandomNumberInRange(sm_Tunables.m_Impulses.m_RapidFireBoostShotMinRandom, sm_Tunables.m_Impulses.m_RapidFireBoostShotMaxRandom);

			// Also randomize the impulse application location
			if (m_nComponent == RAGDOLL_SPINE2 || m_nComponent == RAGDOLL_SPINE3)
			{
				if (fwRandom::GetRandomNumberInRange(0.0f,1.0f) > 0.5f)
					m_nComponent = RAGDOLL_CLAVICLE_LEFT;
				else
					m_nComponent = RAGDOLL_CLAVICLE_RIGHT;
				nmDebugf1("ApplyBulletImpulse: RapidFiringBoostFrame - Hit component moved from the spine to component %d", m_nComponent);
#if ART_ENABLE_BSPY
				SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Rapid-fire boost moved hit from RAGDOLL_SPINE2/RAGDOLL_SPINE3 to %s", m_hitCountThisPerformance, m_nComponent<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[m_nComponent] : "out of range").c_str());
#endif
				// Set the hit location as the center of the part
				localHitPos.Zero();
			}
		}

		// Scale the impulse up as the number of shots this performance increases
		//if (!m_RapidFiring && m_eType != SHOT_SHOTGUN
		//	&& !IsArmoured(pPed))
		//{
		//	float impulseAdd = sm_SuccessiveImpulseIncreaseScale * (m_hitCountThisPerformance-1);
		//		currentHitImpulseWorldSpace.Scale(1.0f + impulseAdd);
		//}
	}

	// Increase impulse if the ped is running towards the bullet direction or decrease it if running against the impulse
	float speedMin = m_eType == SHOT_SHOTGUN ? 3.0f : 5.0f;
	float speedImpulseModifier = 1.0f;
	Vector3 vel = pPed->GetVelocity();
	vel.z = 0.0f;
	float velMag = vel.Mag(); 
	if (vel.Mag2() > SMALL_FLOAT)
	{
		vel /= velMag;
		float towardsDot = vel.Dot(vBulletDir);
		static float range = 0.6f;
		static float rangeRelevance = 0.5f;
		static float speedRelevance = 0.1f;
		if (towardsDot < -range && velMag > speedMin)
		{
			speedImpulseModifier = 1.0f + sm_Tunables.m_Impulses.m_RunningAgainstBulletImpulseMult * (rangeRelevance * Abs(towardsDot) + speedRelevance * velMag);
			if (m_eType == SHOT_SHOTGUN)
				speedImpulseModifier *= Min(1.0f, velMag / 5.0f);
			speedImpulseModifier = Min(speedImpulseModifier, sm_Tunables.m_Impulses.m_RunningAgainstBulletImpulseMultMax);
			currentHitImpulseWorldSpace.Scale(speedImpulseModifier);
			nmDebugf1("ApplyBulletImpulse: Increase impulse if the ped is running towards the shooter - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
			SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Running towards shooter impulse scale %f (%f)", m_hitCountThisPerformance, speedImpulseModifier, currentHitImpulseWorldSpace.Mag()).c_str());
#endif
		}
		else if (towardsDot > range && velMag > speedMin && sm_Tunables.m_Impulses.m_RunningWithBulletImpulseMult > 0.0f)
		{
			speedImpulseModifier = sm_Tunables.m_Impulses.m_RunningWithBulletImpulseMult * (rangeRelevance * Abs(towardsDot) + speedRelevance * velMag);
			if (m_eType == SHOT_SHOTGUN)
				speedImpulseModifier *= Min(1.0f, velMag / 5.0f);
			currentHitImpulseWorldSpace.Scale(speedImpulseModifier);
			nmDebugf1("ApplyBulletImpulse: Decrease impulse if the ped is running away from the shooter - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
			SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Running away from shooter impulse scale %f (%f)", m_hitCountThisPerformance, speedImpulseModifier, currentHitImpulseWorldSpace.Mag()).c_str());
#endif
		}
	}

	// Increase the impulse for shots against the wall
	if (velMag < speedMin && m_eType != SHOT_SHOTGUN && !(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairs) && disableWallImpulseOnStairs))
	{
		if (wallInImpulsePath)
		{
			currentHitImpulseWorldSpace.Scale(sm_Tunables.m_ShotAgainstWall.m_ImpulseMult);
			nmDebugf1("ApplyBulletImpulse: Increase the impulse for shots against the wall - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
			SetbSpyScratchpadString(atVarString("Bullet - %d- impulse - Shot-against-wall impulse scale %f (%f)", m_hitCountThisPerformance, sm_Tunables.m_ShotAgainstWall.m_ImpulseMult, currentHitImpulseWorldSpace.Mag()).c_str());
#endif
		}
	}

	// Ensure a minimum impulse of <sm_ThighImpulseMin> to the thigh (about 150)
	if (velMag < 4.0f && m_eType != SHOT_SHOTGUN && 
		(m_nComponent == RAGDOLL_THIGH_LEFT || m_nComponent == RAGDOLL_THIGH_RIGHT) && 
		fUprightDot > sm_Tunables.m_Impulses.m_LegShotFallRootImpulseMinUpright)
	{
		float currImpulse = currentHitImpulseWorldSpace.Mag();
		if (currImpulse < sm_ThighImpulseMin)
		{
			currentHitImpulseWorldSpace.Scale(sm_ThighImpulseMin/currImpulse);
			nmDebugf1("ApplyBulletImpulse: Ensure a minimum impulse of <sm_ThighImpulseMin> to the thigh - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
			SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Minimum %s impulse scale %f (%f)", m_hitCountThisPerformance, m_nComponent<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[m_nComponent] : "out of range", sm_ThighImpulseMin / currImpulse, currentHitImpulseWorldSpace.Mag()).c_str());
#endif
		}
	}

	// m_SettingShotMessageForShotgun indicates that this is the final pellet's message, and is our last chance to do anything
	// holistic with the shotgun reaction now that we know all the pellet impulses
	if( m_SettingShotMessageForShotgun)
	{
		if (!m_bFront)
			m_CountdownToCatchfall = static_cast<u16>(fwRandom::GetRandomNumberInRange(10, 13));

		// If the hands are in the "held up" position, apply a small force to each arm to reduce the chance of them clapping
		phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
		Matrix34 headMat, leftHandMat, rightHandMat;
		headMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_HEAD)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix()));
		leftHandMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_HAND_LEFT)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix()));
		rightHandMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_HAND_RIGHT)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix()));
		if (leftHandMat.d.z > headMat.d.z && rightHandMat.d.z > headMat.d.z)
		{
			static float fArmImpulseMag = 20.0f;
			Vector3 armImpulse = vBulletDir * fArmImpulseMag;
			pPed->ApplyImpulse(armImpulse, VEC3_ZERO, RAGDOLL_UPPER_ARM_LEFT, true);
			pPed->ApplyImpulse(armImpulse, VEC3_ZERO, RAGDOLL_UPPER_ARM_RIGHT, true);
			nmDebugf1("ApplyBulletImpulse: For shotgun blasts if the hands are in the held up position, applying an impulse of %f to each arm to reduce the chance of them clapping", armImpulse.Mag());
		}

		// Increase the impulse for peds that only get hit by a small number of pellets
		float minFinisherShotgunTotalImpulse = m_HeldWeapon && m_bUpright? 
			sm_Tunables.m_fMinFinisherShotgunTotalImpulseBraced : sm_Tunables.m_fMinFinisherShotgunTotalImpulseNormal;
		if (speedImpulseModifier > 1.0f)
		{
			minFinisherShotgunTotalImpulse *= speedImpulseModifier;
			if (m_HeldWeapon && m_bUpright)
			{
				minFinisherShotgunTotalImpulse *= sm_Tunables.m_fFinisherShotgunBonusArmedSpeedModifier;
			}
		}
		float totalAppliedImpulse = m_hitCountThisPerformance * pWeaponInfo->GetForceHitPed();
		if (totalAppliedImpulse < minFinisherShotgunTotalImpulse)
		{
			float currentImpulse = currentHitImpulseWorldSpace.Mag();
			// If the current impulse is less than the default impulse then reduced the finisher impulse by the same ratio
			float impulseRatio = 1.0f;
			if (pWeaponInfo->GetForceHitPed() > 0.0f && pWeaponInfo->GetForceHitPed() > currentImpulse)
			{
				impulseRatio = currentImpulse / pWeaponInfo->GetForceHitPed();
			}
			float inpulseNeeded = (minFinisherShotgunTotalImpulse - (totalAppliedImpulse - pWeaponInfo->GetForceHitPed())) * impulseRatio;
			if (inpulseNeeded > currentImpulse && currentImpulse > 0.0f)
			{
				currentHitImpulseWorldSpace.Scale(inpulseNeeded / currentImpulse);
				nmDebugf1("ApplyBulletImpulse: Increasing the impulse to compensate for peds that only got hit by a small number of pellets - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
				SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Minimum shotgun blast impulse scale %f (%f)", m_hitCountThisPerformance, inpulseNeeded / currentImpulse, currentHitImpulseWorldSpace.Mag()).c_str());
#endif
			}

			// Move the component from arms to shoulders and from from legs to spine
			if (m_nComponent == RAGDOLL_LOWER_ARM_LEFT || m_nComponent == RAGDOLL_UPPER_ARM_LEFT)
			{
#if ART_ENABLE_BSPY
				SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Moving shotgun hit from %s to RAGDOLL_CLAVICLE_LEFT", m_hitCountThisPerformance, m_nComponent<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[m_nComponent] : "out of range").c_str());
#endif
				m_nComponent = RAGDOLL_CLAVICLE_LEFT;
				nmDebugf1("ApplyBulletImpulse: Moving hit component from the left arm to the left clavicle");
			}
			else if (m_nComponent == RAGDOLL_LOWER_ARM_RIGHT || m_nComponent == RAGDOLL_UPPER_ARM_RIGHT)
			{
#if ART_ENABLE_BSPY
				SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Moving shotgun hit from %s to RAGDOLL_CLAVICLE_RIGHT", m_hitCountThisPerformance, m_nComponent<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[m_nComponent] : "out of range").c_str());
#endif
				m_nComponent = RAGDOLL_CLAVICLE_RIGHT;
				nmDebugf1("ApplyBulletImpulse: Moving hit component from the left arm to the left clavicle");
			}
			else if (m_nComponent < RAGDOLL_SPINE1)
			{
#if ART_ENABLE_BSPY
				SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Moving shotgun hit from %s to RAGDOLL_SPINE0", m_hitCountThisPerformance, m_nComponent<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[m_nComponent] : "out of range").c_str());
#endif
				m_nComponent = RAGDOLL_SPINE0;
				nmDebugf1("ApplyBulletImpulse: Moving hit component from below spine 1 to spine 0");
			}
		}

		m_SettingShotMessageForShotgun = false;
		m_HeldWeapon = false;
	}
	else if (m_ShotgunMessageSent && m_eType == SHOT_SHOTGUN)
	{
		// Gets here if someone was killed by a shotgun while running a shot reaction for another shotgun blast
		currentHitImpulseWorldSpace.Scale(1.0f / m_hitCountThisPerformance);	
		nmDebugf1("ApplyBulletImpulse: Reducing the impulse since this is a ped that got hit by multiple shotgun blasts in one performance - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
		SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Multiple shotgun hit reduction impulse scale %f (%f)", m_hitCountThisPerformance, 1.0f / m_hitCountThisPerformance, currentHitImpulseWorldSpace.Mag()).c_str());
#endif
	}

	float mag2 = currentHitImpulseWorldSpace.Mag2();
	Assert(sm_Tunables.m_Impulses.m_FinalShotImpulseClampMax > 0.0f);
	if (mag2 > square(sm_Tunables.m_Impulses.m_FinalShotImpulseClampMax))
	{
		Assert(mag2==mag2);
		// The vector's magnitude is larger than maxMag, so scale it down.
		currentHitImpulseWorldSpace.Scale(sm_Tunables.m_Impulses.m_FinalShotImpulseClampMax * InvSqrtfSafe(mag2));
		nmDebugf1("ApplyBulletImpulse: Clamp the final impulse - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
		SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Final clamp scale %f (%f)", m_hitCountThisPerformance, sm_Tunables.m_Impulses.m_FinalShotImpulseClampMax * InvSqrtfSafe(mag2), currentHitImpulseWorldSpace.Mag()).c_str());
#endif
	}
	
	bool newBulletApplied = false;

	// Handle some extra bullet impulse application here (that doesn't apply to shotguns)
	if (m_eType != SHOT_SHOTGUN)
	{
		// For shots to the forearms and wrists, project the bullet through to impact the chest and dampen impulses
		if (m_nComponent == RAGDOLL_HAND_LEFT || m_nComponent == RAGDOLL_HAND_RIGHT || 
			m_nComponent == RAGDOLL_LOWER_ARM_LEFT || m_nComponent == RAGDOLL_LOWER_ARM_RIGHT ||
			m_nComponent == RAGDOLL_UPPER_ARM_LEFT || m_nComponent == RAGDOLL_UPPER_ARM_RIGHT)
		{
			// Set the look at arm wound timer
			m_nLookAtArmWoundTime = static_cast<u16>(fwRandom::GetRandomNumberInRange(sm_Tunables.m_ArmShot.m_MinLookAtArmWoundTime, sm_Tunables.m_ArmShot.m_MaxLookAtArmWoundTime));

			// Figure out the impulse for a possible secondary shot
			Vector3 reducedBulletVel = currentHitImpulseWorldSpace;
			float currImpulse = currentHitImpulseWorldSpace.Mag();
			if (currImpulse < 0.01f) // throw out miniscule impulses
				return;

			// Probe for a secondary hit location
			static float startDist = 0.0f;
			static float endDist = 1.0f;
			Vector3 probeStartPosition = currentHitPositionWorldSpace + vBulletDir * startDist;
			Vector3 probeEndPosition = currentHitPositionWorldSpace + vBulletDir * endDist;

			const int numIntersections = 5;
			phIntersection intersect[numIntersections];
			phShapeTest<phShapeProbe> shapeTest;
			phSegment seg(probeStartPosition, probeEndPosition);
			shapeTest.InitProbe(seg, &intersect[0], numIntersections);
			int numHits = shapeTest.TestOneObject(*pPed->GetRagdollInst());

			// Cap the arm impulse based on whether it would project through to hit another body part
			float armImpulseCap = 0.0f;
			if (numHits==0)
			{
				nmDebugf1("ApplyBulletImpulse: A shot to the arm DIDN'T project through to hit any other body part.");
				armImpulseCap = m_nComponent == RAGDOLL_UPPER_ARM_LEFT || m_nComponent == RAGDOLL_UPPER_ARM_RIGHT ? 
					sm_Tunables.m_ArmShot.m_UpperArmNoTorsoHitImpulseCap : sm_Tunables.m_ArmShot.m_LowerArmNoTorseHitImpulseCap;
			}
			else
			{
				nmDebugf1("ApplyBulletImpulse: A shot to the arm DID project through to hit another body part.");
				armImpulseCap = m_nComponent == RAGDOLL_UPPER_ARM_LEFT || m_nComponent == RAGDOLL_UPPER_ARM_RIGHT ? 
					sm_Tunables.m_ArmShot.m_UpperArmImpulseCap : sm_Tunables.m_ArmShot.m_LowerArmImpulseCap;
			}

			if (currImpulse > armImpulseCap) 
			{
				// Subtract off the impulse that will get applied to the arm
				reducedBulletVel.Scale((currImpulse - armImpulseCap) / currImpulse);

				for (int iHit = 0; iHit < numHits; iHit++)
				{
					// Check for non-arm components
					u16 nComponent = intersect[iHit].GetComponent();
					Assert(nComponent < 21);
					if (nComponent != RAGDOLL_HAND_LEFT && nComponent != RAGDOLL_HAND_RIGHT && 
						nComponent != RAGDOLL_LOWER_ARM_LEFT && nComponent != RAGDOLL_LOWER_ARM_RIGHT &&
						nComponent != RAGDOLL_UPPER_ARM_LEFT && nComponent != RAGDOLL_UPPER_ARM_RIGHT)
					{
						Vector3 newLocalHitPos = Vector3(0.0f,0.0f,0.0f);
						Vector3 newLocalHitNormal = Vector3(1.0f,0.0f,0.0f);
						Matrix34 ragdollComponentMatrix;
						if(pPed->GetRagdollComponentMatrix(ragdollComponentMatrix, nComponent))
						{
							// vecHitPos is passed in here as a world space vector. It is stored relative to the hit component in this task.
							ragdollComponentMatrix.UnTransform(RCC_VECTOR3(intersect[iHit].GetPosition()), newLocalHitPos);
							Assertf(newLocalHitPos.Mag2() < 4.0f, "newLocalHitPos is more than 2 metres away from component origin(B)! Dist is %f. Component is %d. Part matrix is orthonormal (%d).",
								newLocalHitPos.Mag(), nComponent, ragdollComponentMatrix.IsOrthonormal());

							// Convert the normal at the point of impact to component local space.
							ragdollComponentMatrix.UnTransform3x3(RCC_VECTOR3(intersect[iHit].GetNormal()), newLocalHitNormal);
							newLocalHitNormal.Normalize();
						}

						// Give that intersection an impulse
						msg.reset();
						msg.addInt(NMSTR_PARAM(NM_APPLYBULLETIMP_PART_INDEX), nComponent);
						msg.addFloat(NMSTR_PARAM(NM_APPLYBULLETIMP_EQUALIZE_AMOUNT), 0.0f);
						msg.addVector3(NMSTR_PARAM(NM_APPLYBULLETIMP_IMPULSE), reducedBulletVel.x, reducedBulletVel.y, reducedBulletVel.z);
						msg.addVector3(NMSTR_PARAM(NM_APPLYBULLETIMP_HIT_POINT), newLocalHitPos.x, newLocalHitPos.y, newLocalHitPos.z);
						msg.addBool(NMSTR_PARAM(NM_APPLYBULLETIMP_LOCAL_HIT_POINT_INFO), true);
						pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_APPLYBULLETIMP_MSG), &msg);
						nmDebugf1("ApplyBulletImpulse: A bullet to the arm has pierced through to hit component %d with an impulse of %f", nComponent, reducedBulletVel.Mag());
#if ART_ENABLE_BSPY
						SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - A bullet to the arm has pierced through to hit component %d with an impulse of %f", m_hitCountThisPerformance, nComponent, reducedBulletVel.Mag()).c_str());
#endif
						break;
					}
				}


				// Add an extra impulse to the clavicle for more twist and push
				if (sm_Tunables.m_ArmShot.m_ClavicleImpulseScale > 0.0f)
				{
					reducedBulletVel *= sm_Tunables.m_ArmShot.m_ClavicleImpulseScale;
					int clavicleComponent = RAGDOLL_CLAVICLE_RIGHT;
					if (m_nComponent >= RAGDOLL_UPPER_ARM_LEFT && m_nComponent <= RAGDOLL_HAND_LEFT)
						clavicleComponent = RAGDOLL_CLAVICLE_LEFT;
					Vector3 newLocalHitPos = Vector3(0.0f,0.0f,0.0f);
					Vector3 newLocalHitNormal = Vector3(1.0f,0.0f,0.0f);
					Matrix34 ragdollComponentMatrix;
					if(pPed->GetRagdollComponentMatrix(ragdollComponentMatrix, clavicleComponent))
					{
						// Convert the normal at the point of impact to component local space.
						ragdollComponentMatrix.UnTransform3x3(-m_vecOriginalHitImpulseWorldSpace, newLocalHitNormal);
						newLocalHitNormal.Normalize();
					}
					if (bSendNewBulletMessage)
					{
						HandleNewBullet(pPed, newLocalHitPos, newLocalHitNormal, reducedBulletVel);
						newBulletApplied = true;
					}

					// Give that intersection an impulse
					msg.reset();
					msg.addInt(NMSTR_PARAM(NM_APPLYBULLETIMP_PART_INDEX), clavicleComponent);
					msg.addFloat(NMSTR_PARAM(NM_APPLYBULLETIMP_EQUALIZE_AMOUNT), 0.0f);
					msg.addVector3(NMSTR_PARAM(NM_APPLYBULLETIMP_IMPULSE), reducedBulletVel.x, reducedBulletVel.y, reducedBulletVel.z);
					msg.addVector3(NMSTR_PARAM(NM_APPLYBULLETIMP_HIT_POINT), newLocalHitPos.x, newLocalHitPos.y, newLocalHitPos.z);
					msg.addBool(NMSTR_PARAM(NM_APPLYBULLETIMP_LOCAL_HIT_POINT_INFO), true);
					pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_APPLYBULLETIMP_MSG), &msg);
					nmDebugf1("ApplyBulletImpulse: An impulse of %f to the clavicle (comp %d) is being added since that arm was hit (to give more desired twist/push)", reducedBulletVel.Mag(), clavicleComponent);
#if ART_ENABLE_BSPY
					SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - An impulse of %f to the clavicle (comp %d) is being added since that arm was hit (to give more desired twist/push)", m_hitCountThisPerformance, reducedBulletVel.Mag(), clavicleComponent).c_str());
#endif
				}

				// Reduce the impulse for the arms
				currentHitImpulseWorldSpace.Scale(armImpulseCap / currImpulse); 
				nmDebugf1("ApplyBulletImpulse: Capping the impulse to the arm (since the bullet would have passed through) - New mag is %f", currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
				SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Arm pass-through impulse scale %f (%f)", m_hitCountThisPerformance, armImpulseCap / currImpulse, currentHitImpulseWorldSpace.Mag()).c_str());
#endif
			}
		}
		
		if ((sm_Tunables.m_Impulses.m_COMImpulseComponent & (1 << m_nComponent)) != 0)
		{
			Matrix34 xCOMTM;
			// Check that an impulse scale was specified, that the ped is still balancing or COM impulse is allowed while un-balanced and that the ped is not moving too fast
			if (sm_Tunables.m_Impulses.m_COMImpulseScale > 0.0f && pPed->GetRagdollInst()->GetCOMTM(xCOMTM) && 
				(!sm_Tunables.m_Impulses.m_COMImpulseOnlyWhileBalancing || !m_bBalanceFailed) && 
				(!bStart || velMag < sm_Tunables.m_Impulses.m_COMImpulseMaxRootVelocityMagnitude))
			{
				// throw out miniscule impulses
				if (currentHitImpulseWorldSpace.Mag() < 0.01f)
					return;

				Matrix34 xRagdollComponentMatrix;
				int iClosestRagdollComponent = RAGDOLL_INVALID;
				float fClosestRagdollComponentDist2 = 0.0f;
				// Go through the ragdoll components and find the component closest to the COM position
				for (int i = 0; i < RAGDOLL_NUM_COMPONENTS; i++)
				{
					if (pPed->GetRagdollComponentMatrix(xRagdollComponentMatrix, i))
					{
						float fRagdollComponentDist2 = xRagdollComponentMatrix.d.Dist2(xCOMTM.d);
						if (iClosestRagdollComponent == RAGDOLL_INVALID || fRagdollComponentDist2 < fClosestRagdollComponentDist2)
						{
							fClosestRagdollComponentDist2 = fRagdollComponentDist2;
							iClosestRagdollComponent = i;
						}
					}
				}

				if (iClosestRagdollComponent == RAGDOLL_INVALID)
				{
					iClosestRagdollComponent = RAGDOLL_BUTTOCKS;
				}

				Vector3 newLocalHitPos = Vector3(0.0f,0.0f,0.0f);
				if (pPed->GetRagdollComponentMatrix(xRagdollComponentMatrix, iClosestRagdollComponent))
				{
					// vecHitPos is passed in here as a world space vector. It is stored relative to the hit component in this task.
					xRagdollComponentMatrix.UnTransform(xCOMTM.d, newLocalHitPos);
					Assertf(newLocalHitPos.Mag2() < 4.0f, "newLocalHitPos is more than 2 metres away from component origin(B)! Dist is %f. Component is %d. Part matrix is orthonormal (%d).",
						newLocalHitPos.Mag(), iClosestRagdollComponent, xRagdollComponentMatrix.IsOrthonormal());
				}

				// Figure out the impulse for secondary shot
				Vector3 reducedBulletVel = currentHitImpulseWorldSpace * sm_Tunables.m_Impulses.m_COMImpulseScale;

				if (bSendNewBulletMessage)
				{
					HandleNewBullet(pPed, newLocalHitPos, localHitNormal, reducedBulletVel);
					newBulletApplied = true;
				}

				msg.reset();
				msg.addInt(NMSTR_PARAM(NM_APPLYBULLETIMP_PART_INDEX), iClosestRagdollComponent);
				msg.addFloat(NMSTR_PARAM(NM_APPLYBULLETIMP_EQUALIZE_AMOUNT), 0.0f);
				msg.addVector3(NMSTR_PARAM(NM_APPLYBULLETIMP_IMPULSE), reducedBulletVel.x, reducedBulletVel.y, reducedBulletVel.z);
				msg.addVector3(NMSTR_PARAM(NM_APPLYBULLETIMP_HIT_POINT), newLocalHitPos.x, newLocalHitPos.y, newLocalHitPos.z);
				msg.addBool(NMSTR_PARAM(NM_APPLYBULLETIMP_LOCAL_HIT_POINT_INFO), true);
				pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_APPLYBULLETIMP_MSG), &msg);
				nmDebugf1("ApplyBulletImpulse: An impulse of %f to comp %d is being added due to the extra COM impulse requested in B*835322.", reducedBulletVel.Mag(), iClosestRagdollComponent);
#if ART_ENABLE_BSPY
				SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - An impulse of %f to comp %d is being added due to the extra COM impulse requested in B*835322.", m_hitCountThisPerformance, reducedBulletVel.Mag(), iClosestRagdollComponent).c_str());
#endif
			}
		}
	}

	if (bSendNewBulletMessage && !newBulletApplied)
		HandleNewBullet(pPed, localHitPos, localHitNormal, currentHitImpulseWorldSpace);

	msg.reset();
	msg.addInt(NMSTR_PARAM(NM_APPLYBULLETIMP_PART_INDEX), m_nComponent);
	msg.addFloat(NMSTR_PARAM(NM_APPLYBULLETIMP_EQUALIZE_AMOUNT), sm_Tunables.m_Impulses.m_EqualizeAmount);
	msg.addVector3(NMSTR_PARAM(NM_APPLYBULLETIMP_IMPULSE), currentHitImpulseWorldSpace.x, currentHitImpulseWorldSpace.y, currentHitImpulseWorldSpace.z);
	msg.addVector3(NMSTR_PARAM(NM_APPLYBULLETIMP_HIT_POINT), localHitPos.x, localHitPos.y, localHitPos.z);
	msg.addBool(NMSTR_PARAM(NM_APPLYBULLETIMP_LOCAL_HIT_POINT_INFO), true);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_APPLYBULLETIMP_MSG), &msg);
	nmDebugf1("ApplyBulletImpulse: Final impulse sent to NM for component %d is %f", m_nComponent, currentHitImpulseWorldSpace.Mag());
#if ART_ENABLE_BSPY
	SetbSpyScratchpadString(atVarString("Bullet - %d - impulse - Final impulse for hit to %s (%f)", m_hitCountThisPerformance, m_nComponent<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[m_nComponent] : "out of range", currentHitImpulseWorldSpace.Mag()).c_str());
#endif

	// Apply an upwards "lift" impulse if the impulse isn't facing very far downwards
	if (m_bUpright && velMag < sm_Tunables.m_Impulses.m_ShotgunMaxSpeedForLiftImpulse && m_eType == SHOT_SHOTGUN)
	{
		// scale lift by mass
		float massScale = pPed->GetRagdollInst()->GetARTAssetID() == 0 ? 1.0f : 0.75f;

		float liftImpulse = 0.0f;

		if( m_pShotBy && m_pShotBy->GetIsTypePed() )
		{
			CPed* pPedHitBy = static_cast<CPed*>(m_pShotBy.Get());
			float distSq = DistSquared(pPedHitBy->GetTransform().GetPosition(), pPed->GetTransform().GetPosition()).Getf();
			float nearThreshold = sm_Tunables.m_Impulses.m_ShotgunLiftNearThreshold * sm_Tunables.m_Impulses.m_ShotgunLiftNearThreshold;
			if (distSq < nearThreshold)
			{
				liftImpulse = (1.0f - (distSq / nearThreshold)) * sm_Tunables.m_Impulses.m_ShotgunMaxLiftImpulse * massScale;
			}
		}

		pPed->ApplyImpulse(Vector3(0.0f, 0.0f, liftImpulse), VEC3_ZERO, RAGDOLL_SPINE3, true);
		nmDebugf1("ApplyBulletImpulse: Applying an upwards lift impulse of %f to component %d", liftImpulse, RAGDOLL_SPINE3);
	}

	if (m_healthRatio == 0.0f)
		m_bKillShotUsed = true;

	// Bump the performance time a bit, so that shots right at the end of the performance can't get missed
	// Only extend the task if not the player or the player is dead as we want the player to regain control quickly
	if (!pPed->IsPlayer() || pPed->IsInjured() || IsCloneDying(pPed))
	{
		BumpPerformanceTime(400);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::HandleConfigureBullets(CPed* UNUSED_PARAM(pPed), bool /*bStart*/)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if ((fwTimer::GetTimeInMilliseconds() - m_LastConfigureBulletsCall) > JUST_STARTED_THRESHOLD)
	{
		// determine the rapid firing status of the assailant
		static u32 rapidShotTimeMin = 250;
		m_RapidFiring = (fwTimer::GetTimeInMilliseconds() - m_LastConfigureBulletsCall) < rapidShotTimeMin;
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
		m_RapidFiringBoostFrame = pWeaponInfo && m_RapidFiring && //pWeaponInfo->GetEffectGroup() == WEAPON_EFFECT_GROUP_SMG && 
			m_hitCountThisPerformance % m_ShotsPerBurst == 0;

		m_LastConfigureBulletsCall = fwTimer::GetTimeInMilliseconds();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::HandlePointGun(CPed* pPed, bool UNUSED_PARAM(bStart), CNmMessageList& list)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	const CObject* pWeaponObj = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	if( pWeaponObj && pWeaponObj->IsCollisionEnabled() && pWeaponObj->GetCurrentPhysicsInst())
	{
		ART::MessageParams& msgPointGun = list.GetMessage(NMSTR_MSG(NM_POINTGUN_MSG));

		if (m_DisableReachForWoundTimeMS==0)
		{
			msgPointGun.addBool(NMSTR_PARAM(NM_START), true);
		}

		const eNMStrings HandToNMHandTarget[2] =
		{
			NM_POINTGUN_RIGHT_HAND_TARGET,
			NM_POINTGUN_LEFT_HAND_TARGET
		};

		const eNMStrings HandToNMHandTargetIndex[2] =
		{
			NM_POINTGUN_RIGHT_HAND_TARGET_INDEX,
			NM_POINTGUN_LEFT_HAND_TARGET_INDEX
		};

		if( m_pShotBy && m_pShotBy->GetIsTypePed())
		{
			CPed* pPedHitBy = static_cast<CPed*>(m_pShotBy.Get());
			if (pPedHitBy->GetRagdollInst() != NULL)
			{
				// Target position needs to be local since we have a valid target index, so make it zero by default and, provided
				// the target is a ped and has a head bone, pass an offset to the head bone
				Vector3 armTarget = VEC3_ZERO;
				const crSkeleton* pSkeleton = pPedHitBy->GetSkeleton();
				if(Verifyf(pSkeleton != NULL, "CTaskNMShot::SetPointGunMessage - Invalid skeleton"))
				{
					int iBoneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(pSkeleton->GetSkeletonData(), BONETAG_HEAD);
					if (iBoneIndex != 1)
					{
						armTarget = VEC3V_TO_VECTOR3(pSkeleton->GetObjectMtx(iBoneIndex).GetCol3());
					}
				}

				for(int i = 0; i < 2; i++)
				{
					msgPointGun.addVector3(NMSTR_PARAM(HandToNMHandTarget[i]), armTarget.x, armTarget.y, armTarget.z);
					msgPointGun.addInt(NMSTR_PARAM(HandToNMHandTargetIndex[i]), pPedHitBy->GetRagdollInst()->GetLevelIndex());
				}
			}
		}
		else
		{
			// just aim in the direction that you're facing
			// Get orientation data for the Ped
			phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
			Matrix34 pelvisMat;
			pelvisMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(0)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); // The pelvis
			Vec3V dirFacing = -(RCC_MAT34V(pelvisMat).GetCol2());  
			Vector3 target = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + VEC3V_TO_VECTOR3(dirFacing) * 5.0f;
			target.y += 1.0f;

			for(int i = 0; i < 2; i++)
			{
				msgPointGun.addVector3(NMSTR_PARAM(HandToNMHandTarget[i]), target.x, target.y, target.z);
				msgPointGun.addInt(NMSTR_PARAM(HandToNMHandTargetIndex[i]), phInst::INVALID_INDEX);
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::ImpulseOverride(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	float oldMag = m_vecOriginalHitImpulseWorldSpace.Mag();
	if (oldMag != 0.0f)
		m_vecOriginalHitImpulseWorldSpace.Scale(sm_OverrideImpulse/oldMag);
	nmDebugf1("ApplyBulletImpulse: Impulse override - New mag is %f", m_vecOriginalHitImpulseWorldSpace.Mag());
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::HandleNormalShotReaction(CPed* pPed, bool bStart, CNmMessageList& list)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	Vector3 objectBehindVictimPos, objectBehindVictimNormal;
	bool wallInImpulsePath = CheckForObjectInPath(pPed, objectBehindVictimPos, objectBehindVictimNormal);

	// Throw out every other minigun impulse
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
	bool throwOutBullet = false;
	if (m_RapidFiring && pWeaponInfo && pWeaponInfo->GetTimeBetweenShots() < 0.04f)
	{
		static u16 throwOut = 2;
		if (m_ThrownOutImpulseCount++ == throwOut)
			m_ThrownOutImpulseCount = 0;
		else
			throwOutBullet = true;
	}

	bool gutShot = m_bFront && (m_nComponent == RAGDOLL_SPINE0 || m_nComponent == RAGDOLL_SPINE1);

	bool doKillShot = m_healthRatio == 0.0f && !m_bKillShotUsed && m_eType != SHOT_SHOTGUN && m_nComponent != RAGDOLL_HEAD && m_nComponent != RAGDOLL_NECK;

	bool isArmoured = IsArmoured(pPed);

	bool doKnockdown = false;

	// Check for intended knockdown
	if (pWeaponInfo)
	{
		int knockdownCount = (isArmoured && !sm_Tunables.m_AllowArmouredKnockdown) ? -1 : pWeaponInfo->GetKnockdownCount();
		doKnockdown = (knockdownCount >= 0) && m_hitCountThisPerformance >= knockdownCount;
	}

	// Add a chance for knockdowns if a gutshot
	if (!doKnockdown && gutShot && (!isArmoured || sm_Tunables.m_AllowArmouredKnockdown))
	{
		if (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < sm_Tunables.m_ChanceForGutShotKnockdown)
			doKnockdown = true;
	}

	// if the ped is already hurt, make sure he goes down
	if (pPed->HasHurtStarted())
	{
		if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_HurtThisFrame))
		{
			doKnockdown = true;
		}
		else if (pPed->GetWeaponManager() && !pPed->GetWeaponManager()->GetIsArmedGun())
		{
			// If ped got a rifle and dropped it and the new gun has not yet spawned
			doKnockdown = true;
		}
	}
	else if ((pPed->GetWeaponManager() == NULL || !pPed->GetWeaponManager()->GetIsArmed()) && pPed->GetHurtEndTime() == 0 && pPed->IsHurtHealthThreshold())
	{
		// To allow civilians and other non combat peds to be forced falling over and transit into writhe when shot below the threshold
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasHurtStarted, true);

		if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_HurtThisFrame))
		{
			doKnockdown = true;
		}
	}

	// In the midst of taking rapid fire?
	int rapidHitCount = !isArmoured ? sm_Tunables.m_RapidHitCount : sm_Tunables.m_ArmouredRapidHitCount;
	bool takingRapidFire = m_RapidFiring && m_hitCountThisPerformance >= rapidHitCount;

	bool bLegShot = false;
	static const atHashString s_sniperSet("Sniper",0x94638209);
	bool bSniperShot = pWeaponInfo && pWeaponInfo->GetNmShotTuningSet() == s_sniperSet;

	if ((!isArmoured || sm_Tunables.m_AllowArmouredLegShot) &&
		(m_nComponent == RAGDOLL_THIGH_LEFT || m_nComponent == RAGDOLL_THIGH_RIGHT || 
		m_nComponent == RAGDOLL_SHIN_LEFT || m_nComponent == RAGDOLL_SHIN_RIGHT || 
		m_nComponent==RAGDOLL_FOOT_LEFT || m_nComponent==RAGDOLL_FOOT_RIGHT))
	{
		if (bSniperShot)
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_SniperLegShot);
		}
		else
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_LegShot);
		}

		bLegShot = true;
	}

	if (m_nComponent == RAGDOLL_HEAD)
	{
		// if the ped has just been shot in the head, disable reach for wound after a short period
		if (sm_Tunables.m_DisableReachForWoundOnHeadShot && m_DisableReachForWoundTimeMS==0)
		{		
			m_DisableReachForWoundTimeMS = fwTimer::GetTimeInMilliseconds()+fwRandom::GetRandomNumberInRange(sm_Tunables.m_DisableReachForWoundOnHeadShotMinDelay, sm_Tunables.m_DisableReachForWoundOnHeadShotMaxDelay);
		}

		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
		Assert(pWeaponInfo);

		if (pWeaponInfo)
		{
			if (IsAutomatic(pWeaponInfo->GetNmShotTuningSet()))
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_AutomaticHeadShot);
			}
			else
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_HeadShot);
			}
		}
	}
	else if (m_nComponent == RAGDOLL_NECK)
	{
		// if the ped has just been shot in the head, disable reach for wound after a short period
		if (sm_Tunables.m_DisableReachForWoundOnNeckShot && m_DisableReachForWoundTimeMS==0)
		{		
			m_DisableReachForWoundTimeMS = fwTimer::GetTimeInMilliseconds()+fwRandom::GetRandomNumberInRange(sm_Tunables.m_DisableReachForWoundOnNeckShotMinDelay, sm_Tunables.m_DisableReachForWoundOnNeckShotMaxDelay);
		}

		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
		Assert(pWeaponInfo);

		if (pWeaponInfo)
		{
			if (IsAutomatic(pWeaponInfo->GetNmShotTuningSet()))
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_AutomaticNeckShot);
			}
			else
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_NeckShot);
			}
		}
	}

	if (!m_bFront && !bSniperShot && (m_nComponent == RAGDOLL_SPINE0 || m_nComponent == RAGDOLL_SPINE1 || 
		m_nComponent == RAGDOLL_SPINE2 || m_nComponent == RAGDOLL_SPINE3 || m_nComponent == RAGDOLL_BUTTOCKS))
	{
		AddTuningSet(&sm_Tunables.m_ParamSets.m_BackShot);
	}

	if (m_ElectricDamage)
	{
		AddTuningSet(&sm_Tunables.m_ParamSets.m_Electrocute);
		if (GetControlTask())
			GetControlTask()->SetFlag(CTaskNMControl::BLOCK_DRIVE_TO_GETUP);
	}

	if(m_healthRatio == 0.0f)
	{
		AddTuningSet(&sm_Tunables.m_ParamSets.m_FatallyInjured);
	}

	if (isArmoured)
	{
		AddTuningSet(&sm_Tunables.m_ParamSets.m_Armoured);
	}

	if (pPed->IsPlayer() && (pPed->GetIsDeadOrDying() || pPed->IsInjured() || IsCloneDying(pPed)))
	{
		if (NetworkInterface::IsGameInProgress())
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_PlayerDeathMP);
		}
		else
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_PlayerDeathSP);
		}
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairs))
	{
		AddTuningSet(&sm_Tunables.m_ParamSets.m_OnStairs);
	}

	// Special handling for knockdown rubber bullet shots
	if (pWeaponInfo && (pWeaponInfo->GetDamageType()==DAMAGE_TYPE_BULLET_RUBBER) && doKnockdown)
	{
		// MP wants tazed or knockdown rubber bullet victims to always go down quickly, so add in a shot fall to knees to force going down
		AddTuningSet(&sm_Tunables.m_ParamSets.m_RubberBulletKnockdown);
		if (GetControlTask())
			GetControlTask()->SetFlag(CTaskNMControl::BLOCK_DRIVE_TO_GETUP);
	}

	if (bStart && m_WasCrouchedOrInLowCover)
	{
		AddTuningSet(&sm_Tunables.m_ParamSets.m_CrouchedOrLowCover);
	}

	// Handle stay upright / staggerfall / fall to knees
	float staggerHealth = 0.0f;

	bool bUseStayUpright = false;

	if (!wallInImpulsePath || (throwOutBullet && m_healthRatio == 0.0f && !m_bKillShotUsed))
	{
		if (m_bLastStandActive)
		{
			Assert(!m_bKnockDownStarted);
		}
		else if (m_healthRatio > staggerHealth && !takingRapidFire && (!bStart || !m_WasCrouchedOrInLowCover) && !doKnockdown && !bLegShot && m_nComponent != RAGDOLL_HEAD && m_nComponent != RAGDOLL_NECK)
		{
			bUseStayUpright = true;
		}
		else 
		{
			// Check if in "Last Stand" mode, and if so delay the staggerfall
			u32 lastStandMaxTimeBetweenHitsMS = (u32) (CTaskNMBehaviour::sm_LastStandMaxTimeBetweenHits * 1000.0f);
			m_bLastStandActive = CTaskNMBehaviour::sm_DoLastStand && !m_bFallingToKnees && m_bFront && !m_bKnockDownStarted && !bLegShot && (fwTimer::GetTimeInMilliseconds() - m_LastConfigureBulletsCall) < lastStandMaxTimeBetweenHitsMS;
			if (!m_bLastStandActive)
			{
				if (m_bFallingToKnees || (m_healthRatio == 0.0f && g_DrawRand.GetRanged(0.0f, 1.0f) <= sm_Tunables.m_ChanceOfFallToKneesOnCollapse))
				{
					AddTuningSet(&sm_Tunables.m_ParamSets.m_FallToKnees);
					m_bFallingToKnees = true;
					if(pPed && pPed->GetSpeechAudioEntity() && !pPed->GetSpeechAudioEntity()->IsPainPlaying())
					{
						audDamageStats damageStats;
						damageStats.DamageReason = AUD_DAMAGE_REASON_DYING_MOAN;
						pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
					}
				}
				else if (!m_bSprinting)
				{
					AddTuningSet(&sm_Tunables.m_ParamSets.m_StaggerFall);
				}
				m_bKnockDownStarted = true;
			}
			else
			{
				m_LastStandStartTimeMS = fwTimer::GetTimeInMilliseconds();
			}
		}
	}

	if (m_bLastStandActive)
	{
		AddTuningSet(&sm_Tunables.m_ParamSets.m_LastStand);
		if (isArmoured)
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_LastStandArmoured);
		}
	}

	if (!throwOutBullet)
	{
		if (wallInImpulsePath)
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_ShotAgainstWall);
		}

		if (m_healthRatio > 0.7)
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_SetFallingReactionHealthy);
		}
		else
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_SetFallingReactionInjured);
		}
	}

	if (!throwOutBullet)
	{
		const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if (pWeaponObject != NULL && pWeaponObject->IsCollisionEnabled() && pWeaponObject->GetCurrentPhysicsInst())
		{
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pPed->GetWeaponHash(*pPed));
			Assert(pWeaponInfo);

			if (pWeaponInfo->GetIsTwoHanded())
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_HoldingTwoHandedWeapon);
			}
			else
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_HoldingSingleHandedWeapon);
			}
		}

		if (wallInImpulsePath)
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_ShotAgainstWall);
		}
	}

	if (!pPed->IsMale())
	{
		AddTuningSet(&sm_Tunables.m_ParamSets.m_Female);
	}

	// Tuning sets added after this point will be ignored. Code can override metadata parameters (And add new ones) after this
	// Send additional messages after this point
	ApplyTuningSetsToList(list);
	
	// Add stay-upright message to the list of messages so that tuning sets can override
	if (bUseStayUpright)
	{
		// add an upright bonus when moving with a weapon
		float velMag = bStart ? pPed->GetVelocity().Mag() : 0.0f;

		// Other bonuses
		float bonus = bStart ? ((pPed->GetWeaponManager()->GetEquippedWeaponObject() != NULL) ? sm_Tunables.m_StayUpright.m_HoldingWeaponBonus : sm_Tunables.m_StayUpright.m_UnarmedBonus) : 0.0f;
		if (isArmoured)
		{
			bonus += (GetArmouredRatio(pPed) * sm_Tunables.m_StayUpright.m_ArmouredBonus);
		}

		ART::MessageParams& msg = list.GetMessage(NMSTR_MSG(NM_STAYUPRIGHT_MSG));
		ART::MessageParamsBase::Parameter* pParam = CNmMessageList::FindParam(msg, NMSTR_PARAM(NM_START));
		if (pParam == NULL)
		{
			msg.addBool(NMSTR_PARAM(NM_START), true);
		}

		float maxUprightForce = sm_MaxShotUprightForce;
		pParam = CNmMessageList::FindParam(msg, NMSTR_PARAM(NM_STAYUPRIGHT_FORCE_STRENGTH));
		if (pParam != NULL)
		{
			maxUprightForce = pParam->v.f;
		}
		
		msg.addBool(NMSTR_PARAM(NM_STAYUPRIGHT_USE_FORCES), maxUprightForce != 0.0f);

		float healthMultiplier = Lerp(sm_Tunables.m_StayUpright.m_HealthMultiplierBonus, m_healthRatio, 1.0f);

		float forceStrength = Min(maxUprightForce * healthMultiplier + velMag * sm_Tunables.m_StayUpright.m_MovingMultiplierBonus + bonus, maxUprightForce + bonus);
		msg.addFloat(NMSTR_PARAM(NM_STAYUPRIGHT_FORCE_STRENGTH), forceStrength);
#if __DEV
		if (m_selected)
			sm_StayUprightMagnitude = forceStrength;
#endif

		float maxUprightTorque = sm_MaxShotUprightTorque;
		pParam = CNmMessageList::FindParam(msg, NMSTR_PARAM(NM_STAYUPRIGHT_TORQUE_STRENGTH));
		if (pParam != NULL)
		{
			maxUprightTorque = pParam->v.f;
		}

		msg.addBool(NMSTR_PARAM(NM_STAYUPRIGHT_USE_TORQUES), maxUprightTorque != 0.0f);
		
		float torqueStrength = Min(maxUprightTorque * healthMultiplier + velMag * sm_Tunables.m_StayUpright.m_MovingMultiplierBonus + bonus, maxUprightTorque + bonus);
		msg.addFloat(NMSTR_PARAM(NM_STAYUPRIGHT_TORQUE_STRENGTH), torqueStrength);
		
		pParam = CNmMessageList::FindParam(msg, NMSTR_PARAM(NM_STAYUPRIGHT_FORCE_IN_AIR_SHARE));
		if (pParam == NULL)
		{
			msg.addFloat(NMSTR_PARAM(NM_STAYUPRIGHT_FORCE_IN_AIR_SHARE), 0.0f);
		}
	}
	
	ART::MessageParams& msgShotInGuts = list.GetMessage(NMSTR_MSG(NM_SHOTINGUTS_MSG));
	ART::MessageParams::Parameter* paramShotInGuts = list.FindParam(msgShotInGuts, NMSTR_PARAM(NM_SHOTINGUTS_SHOT_IN_GUTS));
	// If the parameter exists that means that a tuning set wants the behavior, so determine if the message should be enabled
	if(paramShotInGuts)
	{
		msgShotInGuts.addBool(NMSTR_PARAM(NM_SHOTINGUTS_SHOT_IN_GUTS), gutShot);
	}
	// If the parameter doesn't exist that means that no tuning set wants the behavior, so disable the message
	else
	{
		msgShotInGuts.addBool(NMSTR_PARAM(NM_SHOTINGUTS_SHOT_IN_GUTS), false);
	}

	HandleConfigureBullets(pPed, bStart);

	if (!throwOutBullet)
	{
		ART::MessageParams& msgCharacterStrength = list.GetMessage(NMSTR_MSG(NM_SETCHARACTERSTRENGTH_MSG));
		ART::MessageParamsBase::Parameter* pParam = CNmMessageList::FindParam(msgCharacterStrength, NMSTR_PARAM(NM_START));
		if (pParam == NULL)
		{
			msgCharacterStrength.addBool(NMSTR_PARAM(NM_START), true);
		}
		pParam = CNmMessageList::FindParam(msgCharacterStrength, NMSTR_PARAM(NM_SETCHARACTERSTRENGTH_STRENGTH));
		if (pParam == NULL)
		{
			msgCharacterStrength.addFloat(NMSTR_PARAM(NM_SETCHARACTERSTRENGTH_STRENGTH), sm_DontFallUntilDeadStrength > 0.0f || m_bLastStandActive ? 1.0f : m_healthRatio);
		}

		if (wallInImpulsePath)
		{
			ART::MessageParams& msgBalancerCollisionsReaction = list.GetMessage(NMSTR_MSG(NM_BALANCERCOLLISIONSREACTION_MSG));
			pParam = CNmMessageList::FindParam(msgCharacterStrength, NMSTR_PARAM(NM_BALANCERCOLLISIONSREACTION_OBJ_BEHIND_VICTIM_POS));
			if (pParam == NULL)
			{
				msgBalancerCollisionsReaction.addVector3(NMSTR_PARAM(NM_BALANCERCOLLISIONSREACTION_OBJ_BEHIND_VICTIM_POS), objectBehindVictimPos.x, objectBehindVictimPos.y, objectBehindVictimPos.z);
			}
			pParam = CNmMessageList::FindParam(msgCharacterStrength, NMSTR_PARAM(NM_BALANCERCOLLISIONSREACTION_OBJ_BEHIND_VICTIM_NORMAL));
			if (pParam == NULL)
			{
				msgBalancerCollisionsReaction.addVector3(NMSTR_PARAM(NM_BALANCERCOLLISIONSREACTION_OBJ_BEHIND_VICTIM_NORMAL), objectBehindVictimNormal.x, objectBehindVictimNormal.y, objectBehindVictimNormal.z);
			}
		}
		else
		{
			ART::MessageParams& msgBalance = list.GetMessage(NMSTR_MSG(NM_BALANCE_MSG));
			ART::MessageParamsBase::Parameter* pParam = CNmMessageList::FindParam(msgBalance, NMSTR_PARAM(NM_BALANCE_USE_BODY_TURN));
			if (pParam == NULL)
			{
				msgBalance.addBool(NMSTR_PARAM(NM_BALANCE_USE_BODY_TURN), m_hitCountThisPerformance >= 2);
			}
		}

		// Shot
		bool reachingForWound = true;
		if (m_nComponent == RAGDOLL_HAND_LEFT || m_nComponent == RAGDOLL_HAND_RIGHT || m_nComponent == RAGDOLL_LOWER_ARM_LEFT || m_nComponent == RAGDOLL_LOWER_ARM_RIGHT
			|| m_nComponent == RAGDOLL_UPPER_ARM_LEFT || m_nComponent == RAGDOLL_UPPER_ARM_RIGHT)  
			reachingForWound = false; // disable reach for wound if shot in the wrist or forearm
		if (m_healthRatio > 0.0f && pPed->GetWeaponManager()->GetEquippedWeaponObject() && !gutShot) // disable reach for wound when armed (and not a gut shot)
			reachingForWound = false;

		HandleSnap(pPed, list);

		// Based on health and how many bullets taken
		if (reachingForWound)
		{
			bool shocked = m_hitCountThisPerformance >= 2;
			float energyValue = m_healthRatio * (shocked ? 0.3f : 0.7f);

			// The chance of only reaching with one hand increases as health decreases 
			if (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) > energyValue)
			{			
				bool reachLeft = pPed->GetWeaponManager()->GetEquippedWeaponObject() || (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) > 0.5f);
				ART::MessageParams& msgShotConfigureArms = list.GetMessage(NMSTR_MSG(NM_SHOTCONFIGUREARMS_MSG));
				ART::MessageParamsBase::Parameter* pParam = CNmMessageList::FindParam(msgShotConfigureArms, NMSTR_PARAM(NM_SHOTCONFIGUREARMS_REACH_WITH_ONE_HAND));
				if (pParam == NULL)
				{
					msgShotConfigureArms.addInt(NMSTR_PARAM(NM_SHOTCONFIGUREARMS_REACH_WITH_ONE_HAND), reachLeft ? 1 : 2);
				}
			}

			// Calculate the local space offset to the wound for later use by the animated reach
			Vec3V reachPosition;
			Mat34V boneMatrix;
			u16 boneId(BONETAG_SPINE2);

			fragTypeChild* child = pPed->GetRagdollInst()->GetTypePhysics()->GetAllChildren()[m_nComponent];
			
			if (child)
				boneId = child->GetBoneID();

			s32 boneIndex(-1);
			// find the bone index from the component hit bone Id
			pPed->GetSkeletonData().ConvertBoneIdToIndex(boneId, boneIndex);

			Vector3 worldHitPos;
			GetWorldHitPosition(pPed, worldHitPos);
			reachPosition = VECTOR3_TO_VEC3V(worldHitPos);
			
			pPed->GetSkeleton()->GetGlobalMtx(boneIndex, boneMatrix);
			// work out the offset to the hit position and store it for later.
			m_vecOriginalHitPositionBoneSpace = UnTransformOrtho( boneMatrix, reachPosition );

			TUNE_GROUP_FLOAT(NM_SHOT, fArmOutOfBodyDist, 0.1f, 0.0f, 10.0f, 0.001f);
			m_vecOriginalHitPositionBoneSpace.SetYf(m_vecOriginalHitPositionBoneSpace.GetYf() + fArmOutOfBodyDist);
		}

		HandlePointGun(pPed, bStart, list);

		if (OnMovingGround(pPed))
		{
			ART::MessageParams& msgConfigureBalance = list.GetMessage(NMSTR_MSG(NM_CONFIGURE_BALANCE_MSG));
			msgConfigureBalance.addBool(NMSTR_PARAM(NM_CONFIGURE_BALANCE_MOVING_FLOOR), true);
		}
	}

	list.Post(pPed->GetRagdollInst());

	if (!throwOutBullet)
	{
		HandleApplyBulletImpulse(pPed, true, bStart, doKillShot, wallInImpulsePath);
	}

	if (bLegShot && CalculateUprightDot(pPed) > sm_Tunables.m_Impulses.m_LegShotFallRootImpulseMinUpright)
	{
		Vector3 impulse = m_vecOriginalHitImpulseWorldSpace * sm_Tunables.m_Impulses.m_LegShotFallRootImpulseMult;

		ART::MessageParams msgApplyImpulse;
		msgApplyImpulse.addFloat(NMSTR_PARAM(NM_APPLYIMPULSE_EQUALIZE_AMOUNT), 1.0f);
		msgApplyImpulse.addVector3(NMSTR_PARAM(NM_APPLYIMPULSE_IMPULSE), impulse.x, impulse.y, impulse.z);
		msgApplyImpulse.addInt(NMSTR_PARAM(NM_APPLYIMPULSE_PART_INDEX), RAGDOLL_SPINE0);
		msgApplyImpulse.addVector3(NMSTR_PARAM(NM_APPLYIMPULSE_HIT_POINT), 0.0f, 0.0f, 0.0f);
		msgApplyImpulse.addBool(NMSTR_PARAM(NM_APPLYIMPULSE_LOCAL_HIT_POINT_INFO), true);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_APPLYIMPULSE_MSG), &msgApplyImpulse);
	}

	// HD
	// TBD: some metric deciding if we should blind-fire beyond simple probability check
	//		Phil suggested we can see if this ped was firing their weapon at the time they got shot
	if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject())
	{
		if (pPed->GetWeaponManager()->GetEquippedWeaponObject()->GetWeapon()->GetWeaponInfo()->GetIsGun())
		{
			int bfProbInt = (int)(sm_Tunables.m_fShotBlindFireProbability * 100.0f);
			if (bfProbInt >= (fwRandom::GetRandomNumber()&100))
			{
				m_nTimeToStopBlindFire = fwTimer::GetTimeInMilliseconds() + 
					fwRandom::GetRandomNumberInRange(sm_Tunables.m_iShotMaxBlindFireTimeL, sm_Tunables.m_iShotMaxBlindFireTimeH);

				m_nTimeTillBlindFireNextShot = fwTimer::GetTimeInMilliseconds();
			}
		}
	}
}

bool CTaskNMShot::ShouldReachForWound(bool& withLeft)
{
	CPed* pPed = GetPed();

	bool doReach = false;

	TUNE_GROUP_BOOL(BLEND_FROM_NM, s_bEnableContinuedReachForWound, false);

	if (
		pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInjured)
		&& s_bEnableContinuedReachForWound
		)
	{
		doReach = m_ReachingWithRight || m_ReachingWithLeft;
		withLeft = m_ReachingWithLeft; // if nm reaches with both hands, continue with the left
	}

	return doReach;
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMShot::StartAnimatedReachForWound()
{
	CPed* pPed = GetPed();

	bool reachWithLeft = false;
	bool doReach = ShouldReachForWound(reachWithLeft);

	if (doReach)
	{
		u16 boneId(BONETAG_SPINE2);
		fragTypeChild* child = pPed->GetRagdollInst()->GetTypePhysics()->GetAllChildren()[m_nComponent];

		if (child)
			boneId = child->GetBoneID();

		s32 boneIndex(-1);
		// find the bone index from the component hit bone Id
		pPed->GetSkeletonData().ConvertBoneIdToIndex(boneId, boneIndex);
		s32 shoulderBoneIndex = -1;
		
		// get the shoulder bone id
		pPed->GetSkeletonData().ConvertBoneIdToIndex(BONETAG_L_UPPERARM, shoulderBoneIndex);

		// make sure the wound isn't in an unreachable position (Stops the animated reach going behind the peds back when the nm one reaches for an exit wound)
		// note, this only works because the local y is always more or less forward on our ped bones
		TUNE_GROUP_FLOAT(NM_SHOT, fMinForwardToMaintainReachForWound, 0.075f, 0.0f, 10.0f, 0.001f);
		if (m_vecOriginalHitPositionBoneSpace.GetYf()>fMinForwardToMaintainReachForWound)
		{
			TUNE_GROUP_FLOAT(NM_SHOT, fAnimatedReachForWoundBlendInDuration, 0.25f, 0.0f, 10.0f, 0.001f);
			TUNE_GROUP_FLOAT(NM_SHOT, fAnimatedReachForWoundBlendOutDuration, 0.4f, 0.0f, 10.0f, 0.001f);
			TUNE_GROUP_INT(NM_SHOT, iAnimatedReachForWoundDuration, 60000, -1, 1000000, 1);

			//calculate a new pitch for the hand based on the object space height of the hit position in the default transform
			Mat34V shoulderDefaultTransform;
			Mat34V woundDefaultTransform;

			pPed->GetSkeletonData().ComputeObjectTransform(shoulderBoneIndex, shoulderDefaultTransform);
			pPed->GetSkeletonData().ComputeObjectTransform(boneIndex, woundDefaultTransform);

			Vec3V objectSpacePoint = Transform(woundDefaultTransform,  m_vecOriginalHitPositionBoneSpace);

			//TUNE_GROUP_FLOAT(NM_SHOT, fAnimatedReachForWoundPitchMax, 0.6f, -PI*2.0f, PI*2.0f, 0.001f );
			//TUNE_GROUP_FLOAT(NM_SHOT, fAnimatedReachForWoundPitchMin, -2.0f , -PI*2.0f, PI*2.0f, 0.001f );
			//TUNE_GROUP_FLOAT(NM_SHOT, fAnimatedReachForWoundPitchRange, 1.0f , 0.0f, 2.0f, 0.001f );

			float heightBelowShoulder = (shoulderDefaultTransform.d().GetZf() - objectSpacePoint.GetZf());
			
			//float pitchModifier = Lerp(heightBelowShoulder*fAnimatedReachForWoundPitchRange, fAnimatedReachForWoundPitchMax, fAnimatedReachForWoundPitchMin);

			// pull the hand in towards the body if we're near the shoulder
			TUNE_GROUP_FLOAT(NM_SHOT, fAnimatedReachForWoundShoulderInMax, 0.02f, 0.0f, 1.0f, 0.001f );
			TUNE_GROUP_FLOAT(NM_SHOT, fAnimatedReachForWoundShoulderInMin, 0.0f , 0.0f, 1.0f, 0.001f );
			TUNE_GROUP_FLOAT(NM_SHOT, fAnimatedReachForWoundShoulderInRange, 0.2f , 0.0f, 1.0f, 0.001f );

			float handInForShoulder = Lerp(heightBelowShoulder*fAnimatedReachForWoundShoulderInRange, fAnimatedReachForWoundShoulderInMax, fAnimatedReachForWoundShoulderInMin);
			handInForShoulder = Clamp(handInForShoulder, fAnimatedReachForWoundShoulderInMin, fAnimatedReachForWoundShoulderInMax);
			m_vecOriginalHitPositionBoneSpace.SetYf(m_vecOriginalHitPositionBoneSpace.GetYf() - handInForShoulder);

			// TODO -	Enable support for rotating the hand during the arm reach once the arm ik supports it
			//			Or should this be done through animation?

			//// make our offset matrix
			//Mat34V offset;
			//offset.SetIdentity3x3();
			//offset.Setd(Vec3V(V_ZERO));
			//Mat34VRotateLocalZ(offset, ScalarV(pitchModifier));
			//offset.Setd(m_vecOriginalHitPositionBoneSpace);

			// add a secondary ik task to extend the reach for wound for a while after the nm performance blends out
			CTaskReachArm* pReachTask = rage_new CTaskReachArm
				(
				reachWithLeft ? crIKSolverArms::LEFT_ARM : crIKSolverArms::RIGHT_ARM, 
				pPed, 
				(eAnimBoneTag)boneId, 
				m_vecOriginalHitPositionBoneSpace, 
				AIK_TARGET_WRT_POINTHELPER, 
				iAnimatedReachForWoundDuration, 
				fAnimatedReachForWoundBlendInDuration, 
				fAnimatedReachForWoundBlendOutDuration
				);

			fwMvClipSetId clipSet("move_injured_upper",0x3B43635F);
			fwMvFilterId filter = reachWithLeft ? fwMvFilterId("BONEMASK_ARMONLY_L",0x67B52A94) : fwMvFilterId("BONEMASK_ARMONLY_R",0xE0D61CDC);

			pReachTask->SetClip(clipSet, CLIP_WALK, filter);

			if (pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim() && pPed->GetPedIntelligence()->GetTaskSecondaryPartialAnim()->GetTaskType()==CTaskTypes::TASK_REACH_ARM )
			{
				pPed->GetPedIntelligence()->AddTaskSecondary(NULL, PED_TASK_SECONDARY_PARTIAL_ANIM);
			}
			pPed->GetPedIntelligence()->AddTaskSecondary(pReachTask,PED_TASK_SECONDARY_PARTIAL_ANIM);

		}

	}
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::HandleShotgunReaction(CPed* pPed, bool bStart, CNmMessageList& list)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (pPed->IsPlayer() && (pPed->GetIsDeadOrDying() || pPed->IsInjured() || IsCloneDying(pPed)))
	{
		if (NetworkInterface::IsGameInProgress())
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_PlayerDeathMP);
		}
		else
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_PlayerDeathSP);
		}
	}

	ApplyTuningSetsToList(list);

	ART::MessageParams& msgShotRelax = list.GetMessage(NMSTR_MSG(NM_SHOT_RELAX_MSG));
	static float lowerDeadMin = 1.5f;
	static float lowerDeadMax = 1.5f;
	static float upperDead = 3.0f;
	static float lowerAlive = 1.5f;
	static float upperAlive = 20.0f;
	float lower = m_healthRatio == 0.0f ?  fwRandom::GetRandomNumberInRange(lowerDeadMin, lowerDeadMax) :  lowerAlive;
	float upper = m_healthRatio == 0.0f ?  upperDead :  upperAlive;
	msgShotRelax.addFloat(NMSTR_PARAM(NM_SHOT_RELAX_PERIOD_UPPER), upper);
	msgShotRelax.addFloat(NMSTR_PARAM(NM_SHOT_RELAX_PERIOD_LOWER), lower);

	list.Post(pPed->GetRagdollInst());

	HandleConfigureBullets(pPed, bStart);
	HandleApplyBulletImpulse(pPed, true, bStart);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::HandleUnderwaterReaction(CPed* pPed, bool bStart, CNmMessageList& list)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	AddTuningSet(&sm_Tunables.m_ParamSets.m_Underwater);
	
	if (pPed->GetPrimaryWoundData()->valid && (pPed->GetPrimaryWoundData()->component == RAGDOLL_HEAD || pPed->GetPrimaryWoundData()->numHits > 5))
	{
		AddTuningSet(&sm_Tunables.m_ParamSets.m_UnderwaterRelax);
	}

	if (m_nComponent == RAGDOLL_HAND_LEFT || m_nComponent == RAGDOLL_HAND_RIGHT || m_nComponent == RAGDOLL_LOWER_ARM_LEFT || m_nComponent == RAGDOLL_LOWER_ARM_RIGHT)  
		AddTuningSet(&sm_Tunables.m_ParamSets.m_ArmShot);

	ApplyTuningSetsToList(list);

	list.Post(pPed->GetRagdollInst());

	HandleConfigureBullets(pPed, bStart);
	HandleApplyBulletImpulse(pPed, true, bStart);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::HandleBoundAnklesReaction(CPed* pPed, bool bStart, CNmMessageList&  list)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Shot reaction if ankles are bound (dangling from a meat hook)
	AddTuningSet(&sm_Tunables.m_ParamSets.m_BoundAnkles);

	ApplyTuningSetsToList(list);

	list.Post(pPed->GetRagdollInst());

	HandleConfigureBullets(pPed, bStart);

	HandleApplyBulletImpulse(pPed, true, bStart);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::HandleMeleeReaction(CPed* pPed, bool bStart, CNmMessageList& list)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Need this for headlook
	m_bUsingBalance = true;

	Vector3 objectBehindVictimPos, objectBehindVictimNormal;
	bool wallInImpulsePath = CheckForObjectInPath(pPed, objectBehindVictimPos, objectBehindVictimNormal);

	AddTuningSet(&sm_Tunables.m_ParamSets.m_Melee);

	ApplyTuningSetsToList(list);

	Assert(static_cast<CEntity*>(pPed) != m_pShotBy);
	if(m_pShotBy)
	{
		Vector3 vecLookAtPos = VEC3V_TO_VECTOR3(m_pShotBy->GetTransform().GetPosition());
		ART::MessageParams& msg = list.GetMessage(NMSTR_MSG(NM_BALANCE_MSG));
		msg.addBool(NMSTR_PARAM(NM_BALANCE_LOOKAT_B), true);
		msg.addVector3(NMSTR_PARAM(NM_BALANCE_LOOKAT_POS_VEC3), vecLookAtPos.x, vecLookAtPos.y, vecLookAtPos.z);
	}

	list.Post(pPed->GetRagdollInst());

	HandleApplyBulletImpulse(pPed, true, bStart, false, wallInImpulsePath);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::HandleRunningReaction(CPed *pPed, bool bStart, CNmMessageList& list)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Need this for headlook
	m_bUsingBalance = true;

	bool bLegShot = m_nComponent >= RAGDOLL_THIGH_LEFT && m_nComponent <= RAGDOLL_FOOT_RIGHT;
	if(bLegShot)
	{
		AddTuningSet(&sm_Tunables.m_ParamSets.m_SprintingLegShot);
	}

	if(pPed->GetIsDeadOrDying() || pPed->IsInjured() || IsCloneDying(pPed))
	{
		AddTuningSet(&sm_Tunables.m_ParamSets.m_SprintingDeath);

		if(pPed->IsPlayer())
		{
			if (NetworkInterface::IsGameInProgress())
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_PlayerDeathMP);
			}
			else
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_PlayerDeathSP);
			}
		}
	}

	AddTuningSet(&sm_Tunables.m_ParamSets.m_Sprinting);

	if (m_ElectricDamage)
	{
		AddTuningSet(&sm_Tunables.m_ParamSets.m_Electrocute);
		if (GetControlTask())
			GetControlTask()->SetFlag(CTaskNMControl::BLOCK_DRIVE_TO_GETUP);
	}

	// Send the appropriate set for the equipped weapon
	atHashString shotTuningSet;

	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
	Assert(pWeaponInfo);

	if (pWeaponInfo)
	{
		shotTuningSet = pWeaponInfo->GetNmShotTuningSet();
		taskAssertf(shotTuningSet.GetHash(), "No nm shot tuning set specified for weapon %s!", pWeaponInfo->GetName());

		CNmTuningSet* pWeaponSet = sm_Tunables.m_WeaponSets.Get(shotTuningSet);
		taskAssertf(pWeaponSet, "Nm shot tuning set does not exist! Weapon:%s, Set:%s", pWeaponInfo->GetName(), shotTuningSet.TryGetCStr() ? shotTuningSet.GetCStr() : "UNKNOWN");

		if(pWeaponSet)
		{
			AddTuningSet(pWeaponSet);
		}
	}

	if (m_nComponent == RAGDOLL_HEAD)
	{
		// if the ped has just been shot in the head, disable reach for wound after a short period
		if (sm_Tunables.m_DisableReachForWoundOnHeadShot && m_DisableReachForWoundTimeMS==0)
		{		
			m_DisableReachForWoundTimeMS = fwTimer::GetTimeInMilliseconds()+fwRandom::GetRandomNumberInRange(sm_Tunables.m_DisableReachForWoundOnHeadShotMinDelay, sm_Tunables.m_DisableReachForWoundOnHeadShotMaxDelay);
		}

		if (IsAutomatic(shotTuningSet))
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_AutomaticHeadShot);
		}
		else
		{
			AddTuningSet(&sm_Tunables.m_ParamSets.m_HeadShot);
		}
	}

	const CObject* pWeaponObject = pPed->GetWeaponManager()->GetEquippedWeaponObject();
	if (pWeaponObject != NULL && pWeaponObject->IsCollisionEnabled() && pWeaponObject->GetCurrentPhysicsInst())
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(pPed->GetWeaponHash(*pPed));
		Assert(pWeaponInfo);

		if (pWeaponInfo)
		{
			if (pWeaponInfo->GetIsTwoHanded())
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_HoldingTwoHandedWeapon);
			}
			else
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_HoldingSingleHandedWeapon);
			}
		}
	}

	ApplyTuningSetsToList(list);

	HandlePointGun(pPed, bStart, list);

	list.Post(pPed->GetRagdollInst());

	HandleConfigureBullets(pPed, bStart);
	HandleApplyBulletImpulse(pPed, true, bStart);

}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::ClampHitToSurfaceOfBound(CPed* pPed, Vector3 &localPos, int component)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	phBoundComposite *pBoundComp = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
	if (pBoundComp)
	{
		phBound *pBound = pBoundComp->GetBound(component);

		// Only capsules are currently handled
		if (pBound->GetType() == phBound::CAPSULE)
		{
			// if the local offset is 0, give it a vector such that it will be clamped to a spot on the surface that is closest to the attacker
			if (localPos.Mag2() == 0.0f && m_pShotBy)
			{
				localPos = VEC3V_TO_VECTOR3(m_pShotBy->GetTransform().GetPosition());
				Matrix34 ragdollComponentMatrix;
				if(pPed && pPed->GetRagdollComponentMatrix(ragdollComponentMatrix, component))
				{
					ragdollComponentMatrix.UnTransform(localPos);
				}
			}

			// Determine the offset
			phBoundCapsule *pCapsule = static_cast<phBoundCapsule*>(pBound);
			float offset = 0.0f;
			float halflength = pCapsule->GetLength() / 2.0f;
			float radius = pCapsule->GetRadius();
			if (Abs(localPos.y) <= halflength)
			{
				offset = localPos.XZMag() - radius;
				Vector3 vLocalPosXZ = localPos;
				vLocalPosXZ.y = 0.0f;
				vLocalPosXZ.NormalizeSafe();
				localPos = localPos - vLocalPosXZ*offset;
			}
			else
			{
				if (localPos.y < -halflength)
				{
					localPos.y += halflength;
					float localPosMag = localPos.Mag();
					offset = localPosMag - radius;
					localPos.Scale((localPosMag - offset) / localPosMag);
					localPos.y -= halflength;
				}
				else
				{
					localPos.y -= halflength;
					float localPosMag = localPos.Mag();
					offset = localPosMag - radius;
					localPos.Scale((localPosMag - offset) / localPosMag);
					localPos.y += halflength;
				}
			}
		}
	}

	Assertf(localPos.Mag2() < 2.1f*2.1f, "localPos.Mag() is %f. component is %d.", localPos.Mag(), component); // The 2.1 matches an NM assert in NmRsCBU_ShotNewHit.cpp
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::HandleNewBullet(CPed* pPed, Vector3 &posLocal, const Vector3 &normalLocal, const Vector3 &impulseWorld)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Check bullet is on surface of a bound
	ClampHitToSurfaceOfBound(pPed, posLocal, m_nComponent);

	///// BULLET PARAMETERS MESSAGE:
	ART::MessageParams msg;
	msg.addBool(NMSTR_PARAM(NM_START), true);
	msg.addInt(NMSTR_PARAM(NM_SHOTNEWBULLET_BODYPART), m_nComponent);
	// Shot hit position is stored relative to the centre of the component of the ragdoll which was hit. This is the
	// form now expected by NM after a recent RAGE integrate. (Actually, it can be set as either using the localHitPointInfo param.)
	msg.addBool(NMSTR_PARAM(NM_SHOTNEWBULLET_LOCAL_HIT_POINT_INFO), true);
	msg.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_POS_VEC3), posLocal.x, posLocal.y, posLocal.z);
	msg.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_VEL_VEC3), impulseWorld.x, impulseWorld.y, impulseWorld.z);
	msg.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_NORMAL_VEC3), normalLocal.x, normalLocal.y, normalLocal.z);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_SHOTNEWBULLET_MSG), &msg);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::HandleHeadLook(CPed* pPed, CNmMessageList& list)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (m_pShotBy)
	{
		Vector3 vecLookAtPos = VEC3V_TO_VECTOR3(m_pShotBy->GetTransform().GetPosition());
		if(m_pShotBy->GetIsTypePed())
		{
			vecLookAtPos = ((CPed *)m_pShotBy.Get())->GetBonePositionCached(BONETAG_HEAD);
		}

		if (m_nLookAtArmWoundTime > 0 && 
			((m_nComponent >= RAGDOLL_UPPER_ARM_LEFT && m_nComponent <= RAGDOLL_HAND_LEFT) ||
			(m_nComponent >= RAGDOLL_UPPER_ARM_RIGHT && m_nComponent <= RAGDOLL_HAND_RIGHT)))
		{
			phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
			Matrix34 mat;
			mat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(m_nComponent)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
			vecLookAtPos = mat.d;
			m_nLookAtArmWoundTime--;
		} 

		if(m_bUsingBalance)
		{
			ART::MessageParams& msgLookAt = list.GetMessage(NMSTR_MSG(NM_HEADLOOK_MSG));
			// Always want to kill head look when shot in the head
			if(m_nComponent == RAGDOLL_HEAD)
			{
				msgLookAt.addBool(NMSTR_PARAM(NM_START), false);
			}
			else
			{
				msgLookAt.addVector3(NMSTR_PARAM(NM_HEADLOOK_POS), vecLookAtPos.x, vecLookAtPos.y, vecLookAtPos.z);
			}
		}
		else
		{
			ART::MessageParams& msgLookAt = list.GetMessage(NMSTR_MSG(NM_SHOTHEADLOOK_MSG));
			// Always want to kill head look when shot in the head
			if(m_nComponent == RAGDOLL_HEAD)
			{
				msgLookAt.addBool(NMSTR_PARAM(NM_START), false);
			}
			else
			{
				AddTuningSet(&sm_Tunables.m_ParamSets.m_HeadLook);
				msgLookAt.addVector3(NMSTR_PARAM(NM_SHOTHEADLOOK_LOOKAT_VEC3), vecLookAtPos.x, vecLookAtPos.y, vecLookAtPos.z);
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::HandleSnap(CPed* pPed, CNmMessageList& list)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	if (sm_Tunables.m_ScaleSnapWithSpineOrientation && m_bBalanceFailed)
	{
		// Decrease the snap magnitude as the ped falls down
		float uprightDot = Max(sm_Tunables.m_MinSnap, CalculateUprightDot(pPed));

		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
		Assert(pWeaponInfo);

		if (GetTimeInState()==0)
		{
			atHashString shotTuningSet = pWeaponInfo->GetNmShotTuningSet();

			static atHashString s_burstFire("BurstFire",0xC48A0A41);

			if (shotTuningSet.GetHash()==s_burstFire.GetHash())
			{
				uprightDot*=sm_Tunables.m_Impulses.m_BurstFireInitialSnapMult;
			}
			else if (IsAutomatic(shotTuningSet))
			{
				uprightDot*=sm_Tunables.m_Impulses.m_AutomaticInitialSnapMult;
			}
		}

		ART::MessageParams& msg = list.GetMessage(NMSTR_MSG(NM_SHOT_SNAP_MSG));
		ART::MessageParams::Parameter* pParam = CNmMessageList::FindParam(msg, NMSTR_PARAM(NM_SHOT_SNAP_MAG));
		if (pParam)
		{
			pParam->v.f = Clamp(pParam->v.f*uprightDot, 0.0f, 10.0f);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::CalculateHealth(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Get the health ratio
	Assert(pPed->GetDyingHealthThreshold() > 0.0f);
	float health = Max(pPed->GetDyingHealthThreshold(), pPed->GetHealth());

	// If this task is running locally on a remote ped that should be dead then it's likely that the health
	// hasn't yet been synchronized across the network so we assume here that our health is at the dying threshold
	if (IsCloneDying(pPed))
	{
		health = pPed->GetDyingHealthThreshold();
	}

	float healthyRange = pPed->GetMaxHealth() - pPed->GetDyingHealthThreshold();

	if (healthyRange>0.0f)
		m_healthRatio = (health - pPed->GetDyingHealthThreshold()) / healthyRange;
	else
		m_healthRatio = 0.0f;

	m_healthRatio = ClampRange(m_healthRatio, 0.0f, 1.0f);

	if (m_healthRatio < 0.0f || m_healthRatio > 1.0f || m_healthRatio != m_healthRatio)
	{
		Assertf(m_healthRatio >= 0.0f && m_healthRatio <= 1.0f && m_healthRatio == m_healthRatio, "CTaskNMShot::CalculateHealth - For this %s pPed->GetHealth() is %f, pPed->GetMaxHealth() is %f, pPed->GetDyingHealthThreshold() is %f", 
			pPed->GetPedModelInfo()->GetModelName(), pPed->GetHealth(), pPed->GetMaxHealth(), pPed->GetDyingHealthThreshold());
		m_healthRatio = 0.5f;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTaskNMShot::CheckForObjectInPath(CPed* pPed, Vector3 &objectBehindVictimPos, Vector3 &objectBehindVictimNormal)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Only do this for peds that are standing and of low health
	bool wallInImpulsePath = false;
	if (m_bUpright && m_healthRatio < sm_Tunables.m_ShotAgainstWall.m_HealthRatioLimit && m_eType != SHOT_SHOTGUN)
	{
		// Check whether the ped might collide with the environment

		phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
		Matrix34 pelvisMat;
		pelvisMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(0)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); // The pelvis

		Vector3 vBack = m_vecOriginalHitImpulseWorldSpace;
		Vector3 up(0.0f,0.0f,sm_Tunables.m_ShotAgainstWall.m_ProbeHeightAbovePelvis);
		vBack.z = 0.0f;
		vBack.Normalize();
		Vector3 capStartPosition = pelvisMat.d + up;
		Vector3 capEndPosition = capStartPosition + (vBack * sm_Tunables.m_ShotAgainstWall.m_WallProbeDistance);
		//#if DEBUG_DRAW
		//			grcDebugDraw::Capsule(RCC_VEC3V(capStartPosition), RCC_VEC3V(capEndPosition), fRadius, Color_red, true);
		//#endif // DEBUG_DRAW
		phIntersection intersect;
		phShapeTest<phShapeSweptSphere> shapeTest;
		phSegment seg(capStartPosition, capEndPosition);
		shapeTest.InitSweptSphere(seg, sm_Tunables.m_ShotAgainstWall.m_WallProbeRadius, &intersect);
		if (shapeTest.TestInLevel(pPed->GetRagdollInst()))
		{
			int classType = intersect.GetInstance()->GetClassType();
			if ((classType == PH_INST_MAPCOL || 
				classType == PH_INST_BUILDING || 
				classType == PH_INST_VEH || 
				classType == PH_INST_FRAG_VEH || 
				classType == PH_INST_FRAG_BUILDING) &&
				vBack.Dot(RCC_VECTOR3(intersect.GetNormal())) < -Cosf(sm_Tunables.m_ShotAgainstWall.m_MaxWallAngle * DtoR))
			{
				wallInImpulsePath = true;
				objectBehindVictimPos = RCC_VECTOR3(intersect.GetPosition());  
				objectBehindVictimNormal = RCC_VECTOR3(intersect.GetNormal());  
			}
		}
	}
	return wallInImpulsePath;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTaskNMShot::GatherSituationalData(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Get the health ratio
	CalculateHealth(pPed);

	// Determine whether this is a front or back shot
	Vector3 vecImpactDir = m_vecOriginalHitImpulseWorldSpace;
	vecImpactDir.Normalize();
	phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
	Matrix34 pelvisMat;
	pelvisMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(0)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); // The pelvis

	Vec3V dirFacing = -(RCC_MAT34V(pelvisMat).GetCol2());  
	dirFacing.SetZ(ScalarV(V_ZERO));
	dirFacing = NormalizeSafe(dirFacing, Vec3V(V_X_AXIS_WZERO));
	m_bFront = Dot(dirFacing, VECTOR3_TO_VEC3V(vecImpactDir)).Getf() <= 0.0f;

	Vec3V dirSide = RCC_MAT34V(pelvisMat).GetCol1();  
	dirSide.SetZ(ScalarV(V_ZERO));
	dirSide = NormalizeSafe(dirSide, Vec3V(V_Z_AXIS_WZERO));

	// Check for being upright / standing
	Matrix34 footMat;
	footMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_FOOT_LEFT)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
	Matrix34 headMat;
	headMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(RAGDOLL_HEAD)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
	Vector3 footToHead = headMat.d - footMat.d;
	float footToHeadMag = NormalizeAndMag(footToHead);
	float dot = footToHead.z;
	static float minDist = 1.0f;
	static float minDot = 0.9f;
	m_bUpright = footToHeadMag > minDist && dot > minDot;
}
//////////////////////////////////////////////////////////////////////////
bool CTaskNMShot::IsPoseAppropriateForFiring(CPed* pFiringPed, CAITarget& target, CWeapon* pWeapon, CObject* pWeaponObject)
{
	// Is the target valid?
	if (!target.GetIsValid())
		return false;

	if (!pWeapon)
		return false;

	if (!pWeaponObject)
		return false;

	// Is the target in range?
	Vector3 targetPosition(VEC3_ZERO);
	target.GetPosition(targetPosition);
	Mat34V weaponMatrix = pWeaponObject->GetMatrix();

	Vector3 muzzlePosition(VEC3_ZERO); 
	pWeapon->GetMuzzlePosition(RC_MATRIX34(weaponMatrix), muzzlePosition);

	if (pWeapon->GetRange()*pWeapon->GetRange() < (targetPosition-muzzlePosition).Mag2())
		return false;

	// Is the weapon pointing at the target?
	Vector3 vecToTarget(targetPosition);
	vecToTarget-=muzzlePosition;
	vecToTarget.Normalize();

	Vector3 vecStart(VEC3_ZERO);
	Vector3 vecEnd(VEC3_ZERO);

	pWeapon->CalcFireVecFromGunOrientation(pFiringPed, RC_MATRIX34(weaponMatrix), vecStart, vecEnd);
	Vector3 vecMuzzle(vecEnd);
	vecMuzzle-=vecStart;
	vecMuzzle.Normalize();

	if (vecMuzzle.Dot(vecToTarget) < sm_Tunables.m_fShotWeaponAngleToFireGun)
		return false;

	// Is the ped looking in the general direction of the target?

	Matrix34 headMatrix(M34_IDENTITY);
	pFiringPed->GetBoneMatrix(headMatrix, BONETAG_HEAD);
	vecToTarget=targetPosition;
	vecToTarget-=headMatrix.d;
	vecToTarget.Normalize();

	if (headMatrix.b.Dot(vecToTarget) < sm_Tunables.m_fShotHeadAngleToFireGun)
		return false;

	return true;
}

void CTaskNMShot::CreateAnimatedFallbackTask(CPed* pPed, bool bFalling)
{
	fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
	fwMvClipId clipId = CLIP_ID_INVALID;

	CTaskNMControl* pControlTask = GetControlTask();

	Vector3 vecHeadPos(0.0f,0.0f,0.0f);
	pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);
	// If the ped is already prone then use an on-ground damage reaction
	if (vecHeadPos.z < pPed->GetTransform().GetPosition().GetZf())
	{
		// If we're already on the ground don't do anything when falling
		const EstimatedPose pose = pPed->EstimatePose();
		if (pose == POSE_ON_BACK)
		{
			clipId = CLIP_DAM_FLOOR_BACK;
		}
		else
		{
			clipId = CLIP_DAM_FLOOR_FRONT;
		}

		Matrix34 rootMatrix;
		if (pPed->GetBoneMatrix(rootMatrix, BONETAG_ROOT))
		{
			float fAngle = AngleZ(pPed->GetTransform().GetForward(), RCC_VEC3V(rootMatrix.c)).Getf();
			if (fAngle < -QUARTER_PI)
			{
				clipId = CLIP_DAM_FLOOR_LEFT;
			}
			else if (fAngle > QUARTER_PI)
			{
				clipId = CLIP_DAM_FLOOR_RIGHT;
			}
		}

		clipSetId = pPed->GetPedModelInfo()->GetFullBodyDamageClipSet();

		SetNewTask(rage_new CTaskAnimatedFallback(clipSetId, clipId, 0.0f, 0.0f));
	}
	else
	{
		Vector3 vTempDir = -m_vecOriginalHitImpulseWorldSpace;
		vTempDir.NormalizeSafe();
		float fHitDir = rage::Atan2f(-vTempDir.x, vTempDir.y);
		fHitDir -= pPed->GetTransform().GetHeading();
		fHitDir = fwAngle::LimitRadianAngle(fHitDir);
		fHitDir += QUARTER_PI;
		if(fHitDir < 0.0f)
		{
			fHitDir += TWO_PI;
		}
		int nHitDirn = int(fHitDir / HALF_PI);

		if (bFalling)
		{
			float fStartPhase = 0.0f;
			// If this task is starting as the result of a migration then use the suggested clip and start phase
			CTaskNMControl* pControlTask = GetControlTask();
			if (pControlTask != NULL && (pControlTask->GetFlags() & CTaskNMControl::ALREADY_RUNNING) != 0 && 
				m_suggestedClipSetId != CLIP_SET_ID_INVALID && m_suggestedClipId != CLIP_SET_ID_INVALID)
			{
				clipSetId = m_suggestedClipSetId;
				clipId = m_suggestedClipId;
				fStartPhase = m_suggestedClipPhase;
			}
			else
			{
				CTaskFallOver::eFallDirection dir = CTaskFallOver::kDirFront;
				switch(nHitDirn)
				{
				case CEntityBoundAI::FRONT:
					dir = CTaskFallOver::kDirFront;
					break;
				case CEntityBoundAI::LEFT:
					dir = CTaskFallOver::kDirLeft;
					break;
				case CEntityBoundAI::REAR:
					dir = CTaskFallOver::kDirBack;
					break;
				case CEntityBoundAI::RIGHT:
					dir = CTaskFallOver::kDirRight;
					break;
				}

				CTaskFallOver::eFallContext context = CTaskFallOver::ComputeFallContext(m_nWeaponHash, pPed->GetBoneTagFromRagdollComponent(m_nComponent));
				CTaskFallOver::PickFallAnimation(pPed, context, dir, clipSetId, clipId);
			}
			
			CTaskFallOver* pNewTask = rage_new CTaskFallOver(clipSetId, clipId, 1.0f, fStartPhase);
			pNewTask->SetRunningLocally(true);

			SetNewTask(pNewTask);
		}
		// If this task is starting as the result of a migration then bypass the hit reaction
		else if (pControlTask != NULL && (pControlTask->GetFlags() & CTaskNMControl::ALREADY_RUNNING) == 0)
		{
			switch(nHitDirn)
			{
			case CEntityBoundAI::FRONT:
				clipId = CLIP_DAM_FRONT;
				break;
			case CEntityBoundAI::LEFT:
				clipId = CLIP_DAM_LEFT;
				break;
			case CEntityBoundAI::REAR:
				clipId = CLIP_DAM_BACK;
				break;
			case CEntityBoundAI::RIGHT:
				clipId = CLIP_DAM_RIGHT;
				break;
			default:
				clipId = CLIP_DAM_FRONT;
			}

			clipSetId = pPed->GetPedModelInfo()->GetAdditiveDamageClipSet();

			pPed->GetMovePed().GetAdditiveHelper().BlendInClipBySetAndClip(pPed, clipSetId, clipId);
		}
	}
}

void CTaskNMShot::StartAnimatedFallback(CPed* pPed)
{
	CreateAnimatedFallbackTask(pPed, false);
}

bool CTaskNMShot::ControlAnimatedFallback(CPed* pPed)
{
	nmDebugf3("LOCAL: Controlling animated fallback task %s (0x%p) Ped ragdolling : %s\n", GetSubTask() ? GetSubTask()->GetTaskName() : "None", this, pPed->GetUsingRagdoll() ? "true" : "false");

	if (GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		m_nSuggestedNextTask = CTaskTypes::TASK_FINISHED;
		if (!pPed->GetIsDeadOrDying())
		{
			if (pPed->ShouldBeDead())
			{
				CEventDeath deathEvent(false, fwTimer::GetTimeInMilliseconds(), true);
				pPed->GetPedIntelligence()->AddEvent(deathEvent);
			}
			else
			{
				m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
			}
		}
		return false;
	}
	else if (!GetSubTask())
	{
		// If this is running on a clone ped then wait until the owner ped has fallen over before starting the animated fall-back task
		CTaskNMControl* pTaskNMControl = GetControlTask();
		if (pTaskNMControl == NULL || !pPed->IsNetworkClone() || pPed->IsDead() ||  
			((pTaskNMControl->GetFlags() & CTaskNMControl::FORCE_FALL_OVER) != 0 || pTaskNMControl->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE)))
		{
			// Pretend that the balancer failed at this point (if it hasn't already been set) so that clones will know that the owner has 'fallen'
			if (!pPed->IsNetworkClone() && (pTaskNMControl == NULL || !pTaskNMControl->IsFeedbackFlagSet(CTaskNMControl::BALANCE_FAILURE)))
			{
				ARTFeedbackInterfaceGta feedbackInterface;
				feedbackInterface.SetParentInst(pPed->GetRagdollInst());
				strncpy(feedbackInterface.m_behaviourName, NMSTR_PARAM(NM_BALANCE_FB), ART_FEEDBACK_BHNAME_LENGTH);
				feedbackInterface.onBehaviourFailure();
			}

			CreateAnimatedFallbackTask(pPed, true);
		}
		// We won't have started playing a full body animation - so ensure the ped continues playing their idling animations
		else if ((pTaskNMControl->GetFlags() & CTaskNMControl::ON_MOTION_TASK_TREE) == 0)
		{
			pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Idle);
		}
	}
	
	if (GetNewTask() != NULL || GetSubTask() != NULL)
	{
		// disable ped capsule control so the blender can set the velocity directly on the ped
		if (pPed->IsNetworkClone())
		{
			NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleControl, true);
		}
	}

	return true;
}

