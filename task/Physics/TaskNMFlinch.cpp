// Filename   :	TaskNMFlinch.cpp
// Description:	Natural Motion flinch class (FSM version)

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
#include "Physics\RagdollConstraints.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "stats/StatsInterface.h"
#include "task/combat/TaskCombatMelee.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task/Physics/TaskNMBrace.h"
#include "Task/Physics/TaskNMFlinch.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMFlinch::Tunables CTaskNMFlinch::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMFlinch, 0xd4d7d2a0);
//////////////////////////////////////////////////////////////////////////

#if __DEV || __ASSERT
extern const char* parser_RagdollComponent_Strings[];
#endif

#if __DEV && DEBUG_DRAW
bool CTaskNMFlinch::sm_bDebugDraw = false;
#endif

//////////////////////////////////////////////////////////////////////////
// CClonedNMFlinchInfo
//////////////////////////////////////////////////////////////////////////
CClonedNMFlinchInfo::CClonedNMFlinchInfo(const Vector3& flinchPos, CEntity* pFlinchEntity, u32 nFlinchType, u32 nWeaponHash, int nComponent, const Vector3& flinchLocation, const Vector3& flinchNormal, const Vector3& flinchImpulse)
: m_flinchPos(flinchPos)
, m_flinchType(nFlinchType)
, m_bHasFlinchEntity(false)
, m_nWeaponHash(nWeaponHash)
, m_nComponent(nComponent)
, m_flinchLocation(flinchLocation)
, m_flinchNormal(flinchNormal)
, m_flinchImpulse(flinchImpulse)
{
	if (pFlinchEntity && pFlinchEntity->GetIsDynamic() && static_cast<CDynamicEntity*>(pFlinchEntity)->GetNetworkObject())
	{
		m_flinchEntity = static_cast<CDynamicEntity*>(pFlinchEntity)->GetNetworkObject()->GetObjectID();
		m_bHasFlinchEntity = true;
	}
}

CClonedNMFlinchInfo::CClonedNMFlinchInfo()
{

}

CTaskFSMClone *CClonedNMFlinchInfo::CreateCloneFSMTask()
{
	CEntity *pFlinchEntity = NULL;

	if (m_bHasFlinchEntity)
	{
		netObject* pObj = NetworkInterface::GetNetworkObject(m_flinchEntity);

		if (pObj)
		{
			pFlinchEntity = pObj->GetEntity();
		}
	}

	Vector3* pFlinchLocation = m_nComponent >= 0 ? &m_flinchLocation : NULL;
	Vector3* pFlinchNormal = m_flinchNormal.Mag2() >= square(0.99f) ? &m_flinchNormal : NULL;
	Vector3* pFlinchImpulse = m_flinchImpulse.Mag2() > 0.01f ? &m_flinchImpulse : NULL;

	return rage_new CTaskNMFlinch(10000, 10000, m_flinchPos, pFlinchEntity, static_cast<CTaskNMFlinch::eFlinchType>(m_flinchType), m_nWeaponHash, m_nComponent, true, NULL, pFlinchLocation, pFlinchNormal, pFlinchImpulse);
}

//////////////////////////////////////////////////////////////////////////
// CTaskNMFlinch
//////////////////////////////////////////////////////////////////////////

const char* CTaskNMFlinch::ms_TypeToString[NUM_FLINCH_TYPES] = 
{
	"MELEE", "MELEE_PASSIVE", "WATER_CANNON"
};

CTaskNMFlinch::CTaskNMFlinch(u32 nMinTime, u32 nMaxTime, const Vector3& flinchPos, CEntity* pFlinchEntity, eFlinchType nFlinchType, u32 nWeaponHash, 
	s32 nHitComponent, bool bLocalHitPointInfo, const CPed* pPed, const Vector3* vHitLocation, const Vector3* vHitNormal, const Vector3* vHitImpulse)
:	CTaskNMBehaviour(nMinTime, nMaxTime)
, m_nFlinchType(nFlinchType)
, m_vecFlinchPos(flinchPos)
, m_pFlinchEntity(pFlinchEntity)
, m_bBalanceFailed(false)
, m_bRollingDownStairs(false)
, m_nTimeToStopLean(0)
, m_vLocalHitLocation(Vector3::ZeroType)
, m_vWorldHitNormal(Vector3::ZeroType)
, m_vLocalHitImpulse(Vector3::ZeroType)
, m_vWorldHitImpulse(Vector3::ZeroType)
, m_nWeaponHash(nWeaponHash)
, m_nHitComponent(nHitComponent)
{
	Assert((unsigned)nFlinchType < (unsigned)NUM_FLINCH_TYPES);
	SetInternalTaskType(CTaskTypes::TASK_NM_FLINCH);

	CalculateHitPointInfo(pPed, vHitLocation, vHitNormal, vHitImpulse, bLocalHitPointInfo);
}

CTaskNMFlinch::CTaskNMFlinch(const CTaskNMFlinch& otherTask)
:	CTaskNMBehaviour(otherTask)
, m_nFlinchType(otherTask.m_nFlinchType)
, m_vecFlinchPos(otherTask.m_vecFlinchPos)
, m_pFlinchEntity(otherTask.m_pFlinchEntity)
, m_bBalanceFailed(otherTask.m_bBalanceFailed)
, m_bRollingDownStairs(otherTask.m_bRollingDownStairs)
, m_nTimeToStopLean(otherTask.m_nTimeToStopLean)
, m_nWeaponHash(otherTask.m_nWeaponHash)
, m_nHitComponent(otherTask.m_nHitComponent)
, m_vLocalHitLocation(otherTask.m_vLocalHitLocation)
, m_vWorldHitNormal(otherTask.m_vWorldHitNormal)
, m_vLocalHitImpulse(otherTask.m_vLocalHitImpulse)
, m_vWorldHitImpulse(otherTask.m_vWorldHitImpulse)
{
	Assert((unsigned)m_nFlinchType < (unsigned)NUM_FLINCH_TYPES);
	SetInternalTaskType(CTaskTypes::TASK_NM_FLINCH);
}


CTaskNMFlinch::~CTaskNMFlinch()
{
}

void CTaskNMFlinch::CalculateHitPointInfo(const CPed* pPed, const Vector3* vHitLocation, const Vector3* vHitNormal, const Vector3* vHitImpulse, bool bLocalHitPointInfo)
{
#if __ASSERT
	if (m_nHitComponent >= 0)
	{
		Assert(vHitLocation);
		Assert(vHitNormal);
		Assert(vHitImpulse);
	}
#endif

	if (vHitNormal)
	{
		m_vWorldHitNormal = *vHitNormal;

		if (!AssertVerify(m_vWorldHitNormal.x == m_vWorldHitNormal.x && (m_vWorldHitNormal.x * 0.0f) == 0.0f && InRange(m_vWorldHitNormal.Mag2(), square(0.99f), square(1.01f))))
		{
			m_vWorldHitNormal.NormalizeSafe(XAXIS);
		}
	}

	if (vHitImpulse)
	{
		m_vWorldHitImpulse = *vHitImpulse;
	}

	if (vHitLocation)
	{
		m_vLocalHitLocation = *vHitLocation;
	}

	if (!bLocalHitPointInfo)
	{
		Matrix34 xRagdollComponentTransform;
		if (m_nHitComponent >= 0 && taskVerifyf(pPed && pPed->GetRagdollComponentMatrix(xRagdollComponentTransform, m_nHitComponent), "CTaskNMFlinch::CalculateHitPointInfo: Could not switch hit position <%f %f %f> to local space for component %s", vHitLocation->x, vHitLocation->y, vHitLocation->z, m_nHitComponent<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[m_nHitComponent] : "out of range"))
		{
			taskAssertf(xRagdollComponentTransform.IsOrthonormal(), "CTaskNMFlinch::CalculateHitPointInfo: Invalid ragdoll transform for component %s", m_nHitComponent<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[m_nHitComponent] : "out of range");

			xRagdollComponentTransform.UnTransform3x3(m_vWorldHitImpulse, m_vLocalHitImpulse);

			taskAssertf(xRagdollComponentTransform.d.Dist2(m_vLocalHitLocation) < square(3.0f), "CTaskNMFlinch::CalculateHitPointInfo: Hit position is more then 3 meters away from ped position!");
			xRagdollComponentTransform.UnTransform(m_vLocalHitLocation);

#if __ASSERT
			// Network games mess with the component that was hit without adjusting the hit location - causing this assert to trigger
			// It was deemed too risky to fix the issue in GTAV so just removing the assert in multiplayer games for now (see url:bugstar:2000138 and url:bugstar:1764181)
			if (!NetworkInterface::IsGameInProgress())
			{
				float fThighLength = 0.7f;
				taskAssertf(m_vLocalHitLocation.Mag2() < square(fThighLength),
					"CTaskNMFlinch::CalculateHitPointInfo: Local hit point too far from component centre = %.3f (max = %.3f), %.3f, %.3f, %.3f. Component hit was %s (%d)",
					m_vLocalHitLocation.Mag(), fThighLength, m_vLocalHitLocation.x, m_vLocalHitLocation.y, m_vLocalHitLocation.z,
					m_nHitComponent<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[m_nHitComponent] : "out of range", m_nHitComponent);
			}
#endif
		}
	}

	Assert(m_vLocalHitLocation.x == m_vLocalHitLocation.x && (m_vLocalHitLocation.x * 0.0f) == 0.0f && m_vLocalHitLocation.Mag2() < square(7000.0f));
}

void CTaskNMFlinch::UpdateFlinch(CPed* pPed, const Vector3& flinchPos, CEntity* pFlinchEntity, eFlinchType nFlinchType, u32 nWeaponHash, 
	s32 nHitComponent, bool bLocalHitPointInfo, const Vector3* vHitLocation, const Vector3* vHitNormal, const Vector3* vHitImpulse)
{
	m_nFlinchType = nFlinchType;
	m_pFlinchEntity = pFlinchEntity;
	m_vecFlinchPos = flinchPos;
	m_nHitComponent = nHitComponent;
	m_nWeaponHash = nWeaponHash;
	CalculateHitPointInfo(pPed, vHitLocation, vHitNormal, vHitImpulse, bLocalHitPointInfo);

	SetFlinchMessage(pPed, false);
}

Vector3 CTaskNMFlinch::GetFlinchPos(CPed* UNUSED_PARAM(pPed)) const
{
	Vector3 vecPos = m_vecFlinchPos;
	if(m_pFlinchEntity)
	{
		vecPos = VEC3V_TO_VECTOR3(m_pFlinchEntity->GetTransform().GetPosition());
		if(m_pFlinchEntity->GetIsTypePed())
		{
			((CPed *)m_pFlinchEntity.Get())->GetBonePosition(vecPos, BONETAG_NECK);
		}
	}

	return vecPos;
}

CTaskInfo* CTaskNMFlinch::CreateQueriableState() const
{
	return rage_new CClonedNMFlinchInfo(m_vecFlinchPos, m_pFlinchEntity.Get(), m_nFlinchType, m_nWeaponHash, m_nHitComponent, m_vLocalHitLocation, m_vWorldHitNormal, m_vWorldHitImpulse);
}

void CTaskNMFlinch::StartBehaviour(CPed* pPed)
{
	SetFlinchMessage(pPed, true);
}

void CTaskNMFlinch::SetFlinchMessage(CPed* pPed, bool UNUSED_PARAM(bStartBehaviour))
{
	//nmDebugf1("CTaskNMFlinch type %s(%n)\n", ms_TypeToString[m_nFlinchType], m_nFlinchType );

	// TODO - hook up contextual and weapon messages

	AddTuningSet(&sm_Tunables.m_Start);

	if (m_nFlinchType == FLINCHTYPE_MELEE_PASSIVE)
	{
		AddTuningSet(&sm_Tunables.m_Passive);
	}
	else if (m_nFlinchType == FLINCHTYPE_WATER_CANNON)
	{
		AddTuningSet(&sm_Tunables.m_WaterCannon);
	}

	if (pPed->GetArmour()>0.0f)
	{
		AddTuningSet(&sm_Tunables.m_Armoured);
	}

	if (pPed->GetRagdollConstraintData()->BoundAnklesAreEnabled())
	{
		AddTuningSet(&sm_Tunables.m_BoundAnkles);
	}

	if (pPed->IsFatallyInjured())
	{
		AddTuningSet(&sm_Tunables.m_FatallyInjured);
	}

	if (pPed->IsPlayer() && pPed->GetIsDeadOrDying())
	{
		AddTuningSet(&sm_Tunables.m_PlayerDeath);
	}

	if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_StairsDetected))
	{
		AddTuningSet(&sm_Tunables.m_OnStairs);
	}

	if (pPed->GetWeaponManager())
	{
		if ( pPed->GetWeaponManager()->GetIsArmed1Handed())
		{
			AddTuningSet(&sm_Tunables.m_HoldingSingleHandedWeapon);
		}

		if ( pPed->GetWeaponManager()->GetIsArmed2Handed())
		{
			AddTuningSet(&sm_Tunables.m_HoldingTwoHandedWeapon);
		}
	}

	float hitImpulseMag = 0.0f;

	// add the appropriate weapon tuning set.
	if (m_nWeaponHash!=0)
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
		if (AssertVerify(pWeaponInfo != NULL))
		{
			atHashString shotTuningSet = pWeaponInfo->GetNmShotTuningSet();
			if (shotTuningSet.GetHash()!=0)
			{
				CNmTuningSet* pSet = sm_Tunables.m_WeaponSets.Get(shotTuningSet);

				if (pSet)
				{
					AddTuningSet(pSet);
				}
			}

			if(m_nHitComponent > -1)
			{
				eAnimBoneTag iHitBoneTag = pPed->GetBoneTagFromRagdollComponent(m_nHitComponent);

				// Determine whether this is a front or back shot
				Mat34V xComponentMat;
				Transform(xComponentMat, pPed->GetRagdollInst()->GetMatrix(), pPed->GetRagdollInst()->GetCacheEntry()->GetBound()->GetCurrentMatrix(m_nHitComponent));

				Vec3V vDirFacing = -xComponentMat.GetCol2();  
				vDirFacing.SetZ(ScalarV(V_ZERO));
				vDirFacing = NormalizeSafe(vDirFacing, Vec3V(V_X_AXIS_WZERO));
				bool bFront = Dot(vDirFacing, VECTOR3_TO_VEC3V(m_vWorldHitImpulse)).Getf() <= 0.0f;

				Vec3V vFlinchPosition = RCC_VEC3V(m_vecFlinchPos);
				if (GetFlinchEntity() != NULL)
				{
					vFlinchPosition = GetFlinchEntity()->GetTransform().GetPosition();
				}
				float fDistance = Dist(pPed->GetTransform().GetPosition(), vFlinchPosition).Getf();

				hitImpulseMag = pWeaponInfo->GetForceHitPed(iHitBoneTag, bFront, fDistance);
				nmDebugf2("[%u] Calculating flinch impulse:%s(%p) Initial weapon force: %.3f", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, hitImpulseMag);
				Assertf(hitImpulseMag==hitImpulseMag && hitImpulseMag*0.0f==0.0f && Abs(hitImpulseMag) < 9999.0f, "Bad flinch impulse (1). hitImpulseMag is %f", hitImpulseMag);

				if (GetFlinchEntity() != NULL && GetFlinchEntity()->GetIsTypePed() && static_cast<CPed*>(GetFlinchEntity())->IsPlayer())
				{
					float fStrengthValue = Clamp(static_cast<float>(StatsInterface::GetIntStat(STAT_STRENGTH.GetStatId())) / 100.0f, 0.0f, 1.0f);
					// Determine the maximum force to be applied (given a maximum strength value)
					float fMaxHitImpulseMag = hitImpulseMag * pWeaponInfo->GetForceMaxStrengthMult();
					hitImpulseMag = Lerp(fStrengthValue, hitImpulseMag, fMaxHitImpulseMag);
					nmDebugf2("[%u] Calculating flinch impulse:%s(%p) Force including strength: %.3f", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, hitImpulseMag);
					Assertf(hitImpulseMag==hitImpulseMag && hitImpulseMag*0.0f==0.0f && Abs(hitImpulseMag) < 9999.0f, "Bad flinch impulse (2). hitImpulseMag is %f", hitImpulseMag);
				}
			}
		}
	}

	// add the appropriate melee move tuning set
	if(m_pFlinchEntity && m_pFlinchEntity->GetIsTypePed())
	{
		CPed* pFlinchPed = static_cast<CPed*>(m_pFlinchEntity.Get());
		// find the melee task and get the action result
		CTaskMeleeActionResult* pTask = static_cast<CTaskMeleeActionResult*>(pFlinchPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_MELEE_ACTION_RESULT));

		if (pTask)
		{
			const CActionResult* actionResult = pTask->GetActionResult();

			if (actionResult)
			{
				atHashString set = actionResult->GetNmReactionSet();
				if (set.GetHash() != 0)
				{
					CNmTuningSet* pSet = sm_Tunables.m_ActionSets.Get(set);

					if (pSet)
					{
						static const atHashString sKickStandingNmReactionSet("KICK_STANDING", 0xC2EE0AD7);
						if (set == sKickStandingNmReactionSet)
						{
							const CPlayerSpecialAbility* pSpecialAbility = pFlinchPed->GetSpecialAbility();
							if (pSpecialAbility != NULL && pSpecialAbility->GetType() == SAT_RAGE && pSpecialAbility->IsActive())
							{
								hitImpulseMag *= sm_Tunables.m_fSpecialAbilityRageKickImpulseModifier;
							}
						}

						AddTuningSet(pSet);
					}
				}

				Vector3 vPedVelocity = pPed->GetVelocity();
				Vector3 vRelativeVelocity = vPedVelocity - VEC3V_TO_VECTOR3(pPed->GetCollider()->GetReferenceFrameVelocity());
				vRelativeVelocity.z = 0.0f;
				if (vRelativeVelocity.Mag2() > SMALL_FLOAT)
				{
					Vector3 vRelativeVelocityNormalized(vRelativeVelocity);
					float fRelativeVelocityMag = NormalizeAndMag(vRelativeVelocityNormalized);
					static dev_float s_fVelocityThreshold = 1.0f;

					nmAssert(pPed->GetRagdollInst());
					nmAssert(pPed->GetRagdollInst()->GetTypePhysics());
					if (m_nHitComponent>=0 
						&& m_nHitComponent<pPed->GetRagdollInst()->GetTypePhysics()->GetNumChildren() 
						&& fRelativeVelocityMag >= s_fVelocityThreshold)
					{
						float fAverageMass = pPed->GetRagdollInst()->GetTypePhysics()->GetTotalUndamagedMass() / pPed->GetRagdollInst()->GetTypePhysics()->GetNumChildren();
						float fPartMass = pPed->GetRagdollInst()->GetTypePhysics()->GetChild(m_nHitComponent)->GetUndamagedMass();
						float fProportionalMassScale = fPartMass / fAverageMass;
						float fVelocityDirection = DotProduct(m_vWorldHitImpulse, vRelativeVelocityNormalized);
						// If the victim (of a melee hit) is moving over a certain speed threshold and in the same direction as the impulse
						// then cancel out some of the impulse
						// otherwise the victim will receive too much impulse and go flying
						// This is mostly to counteract the very large impulses that peds were receiving when jumping away from melee hits...
						if (fVelocityDirection > 0.0f)
						{
							// Never want to reduce impulse by more than s_fImpulseScaleMax percentage
							float fVelocityScale = Min(fRelativeVelocityMag * fVelocityDirection * fPartMass * fProportionalMassScale, hitImpulseMag * sm_Tunables.m_fImpulseReductionScaleMax);
							hitImpulseMag -= fVelocityScale;
							nmDebugf2("[%u] Calculating flinch impulse:%s(%p) Relative velocity: %.3f, Velocity direction: %.3f, Part mass: %.3f, Velocity scale: %.3f", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, 
								fRelativeVelocityMag, fVelocityDirection, fPartMass, fVelocityScale);
							nmDebugf2("[%u] Calculating flinch impulse:%s(%p) Force including reduction for parallel velocity: %.3f", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, hitImpulseMag);
							Assertf(hitImpulseMag==hitImpulseMag && hitImpulseMag*0.0f==0.0f && Abs(hitImpulseMag) < 9999.0f, "Bad flinch impulse (3). hitImpulseMag is %f", hitImpulseMag);
						}

						if (m_pFlinchEntity != NULL && m_pFlinchEntity->GetIsPhysical())
						{
							Vector3 vFlinchEntityVelocity = static_cast<CPhysical*>(m_pFlinchEntity.Get())->GetVelocity();
							if (m_pFlinchEntity->GetIsTypePed() && static_cast<CPed*>(m_pFlinchEntity.Get())->GetGroundPhysical() != NULL)
							{
								vFlinchEntityVelocity -= VEC3V_TO_VECTOR3(static_cast<CPed*>(m_pFlinchEntity.Get())->GetGroundVelocity());
							}
							else if (m_pFlinchEntity->GetCollider() != NULL)
							{
								vFlinchEntityVelocity -= VEC3V_TO_VECTOR3(pPed->GetCollider()->GetReferenceFrameVelocity());
							}

							vFlinchEntityVelocity.z = 0;
							Vector3 vFlinchEntityVelocityNormalized;
							vFlinchEntityVelocityNormalized.Normalize(vFlinchEntityVelocity);
							float fFlinchEntityVelocityDirection = DotProduct(vFlinchEntityVelocityNormalized, vRelativeVelocityNormalized);
							// If the attacker is moving towards the victim then subtract out the attacker's velocity so that the attacker
							// doesn't end up walking through the victim
							if (fFlinchEntityVelocityDirection < 0.0f)
							{
								vRelativeVelocity -= vFlinchEntityVelocity;
								vRelativeVelocityNormalized = vRelativeVelocity;
								fRelativeVelocityMag = NormalizeAndMag(vRelativeVelocityNormalized);
							}
						}

						Vec3V vPedToFlinchPed = Subtract(pFlinchPed->GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
						if (IsGreaterThanAll(MagSquared(vPedToFlinchPed), ScalarV(V_FLT_SMALL_5)) != 0)
						{
							fVelocityDirection = DotProduct(VEC3V_TO_VECTOR3(Normalize(vPedToFlinchPed)), vRelativeVelocityNormalized);
							// If the victim (of a melee hit) is moving over a certain speed threshold and towards the attacker
							// then cancel out some of the victim's velocity by applying a counter impulse
							// otherwise the victim will fall into the attacker which looks strange
							// This is mostly to counteract the very large speeds that peds attain when 'homing' towards each other...
							if (fVelocityDirection > 0.0f)
							{
								Vector3 vImpulse(vRelativeVelocityNormalized);
								vImpulse.Scale(-fRelativeVelocityMag * fVelocityDirection * fPartMass * sm_Tunables.m_fCounterImpulseScale);
								pPed->ApplyImpulse(vImpulse, VEC3_ZERO, RAGDOLL_BUTTOCKS, true);
								nmDebugf2("[%u] Calculating flinch impulse:%s(%p) Relative velocity: %.3f, Velocity direction: %.3f, Part mass: %.3f, Velocity scale: %.3f", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, 
									-fRelativeVelocityMag, fVelocityDirection, fPartMass, -fRelativeVelocityMag * fVelocityDirection * fPartMass);
								nmDebugf2("[%u] Applying counter flinch impulse:%s(%p) Counter impulse: %.3f", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, vImpulse.Mag());
							}
						}
					}
				}
			}
		}
	}

	CNmMessageList list;

	ApplyTuningSetsToList(list);

	if (sm_Tunables.m_RandomiseLeadingHand)
	{
		ART::MessageParams& msgFlinch = list.GetMessage(NMSTR_MSG(NM_FLINCH_MSG));
		bool leadWithLeft = fwRandom::GetRandomTrueFalse();
		msgFlinch.addBool(NMSTR_PARAM(NM_FLINCH_LEFT_HANDED), leadWithLeft);
		msgFlinch.addBool(NMSTR_PARAM(NM_FLINCH_RIGHT_HANDED), !leadWithLeft);
	}	

	// lean in direction to keep us moving, if req'd
	if (list.HasMessage(NMSTR_MSG(NM_BALANCE_LEAN_DIR_MSG)))
	{
		// only want to lean for a controllable time period
		m_nTimeToStopLean = fwTimer::GetTimeInMilliseconds() + (u32)fwRandom::GetRandomNumberInRange((int)sm_Tunables.m_MinLeanInDirectionTime, (int)sm_Tunables.m_MaxLeanInDirectionTime);
	}

	// apply the hit impulse
	if (m_nHitComponent > -1)
	{
		ART::MessageParams& msg = list.GetMessage(NMSTR_MSG(NM_APPLYIMPULSE_MSG));
		ART::MessageParamsBase::Parameter* pParam = CNmMessageList::FindParam(msg,NMSTR_PARAM(NM_APPLYIMPULSE_IMPULSE));
		if (pParam)
		{
			// Full body
			hitImpulseMag *= pParam->v.vec[0];
			nmDebugf2("[%u] Calculating flinch impulse:%s(%p) Force including full-body reduction: %.3f", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, hitImpulseMag);
			// Lower body
			if (m_nHitComponent <= RAGDOLL_SPINE1)
			{
				hitImpulseMag *= pParam->v.vec[1];
				nmDebugf2("[%u] Calculating flinch impulse:%s(%p) Force including lower-body reduction: %.3f", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, hitImpulseMag);
			}
			// Upper body
			else
			{
				hitImpulseMag *= pParam->v.vec[2];
				nmDebugf2("[%u] Calculating flinch impulse:%s(%p) Force including upper-body reduction: %.3f", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, hitImpulseMag);
			}
			Assertf(hitImpulseMag==hitImpulseMag && hitImpulseMag*0.0f==0.0f && Abs(hitImpulseMag) < 9999.0f, "Bad flinch impulse (4). hitImpulseMag is %f", hitImpulseMag);
		}

		msg.addInt(NMSTR_PARAM(NM_APPLYIMPULSE_PART_INDEX), m_nHitComponent);
		Vector3 impulse;
		if (m_vLocalHitImpulse.IsZero())
		{
			impulse = m_vWorldHitImpulse * hitImpulseMag;
			nmDebugf2("[%u] Final flinch impulse:%s(%p) Final impulse: %.3f (%.3f)", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, hitImpulseMag, m_vWorldHitImpulse.Mag());
			msg.addBool(NMSTR_PARAM(NM_APPLYIMPULSE_LOCAL_IMPULSE_INFO), false);
			Assertf(impulse==impulse, "Bad flinch m_vWorldHitImpulse - hitImpulseMag is %f, m_vWorldHitImpulse is %f %f %f, , impulse is %f %f %f", 
				hitImpulseMag, m_vWorldHitImpulse.x, m_vWorldHitImpulse.y, m_vWorldHitImpulse.z, impulse.x, impulse.y, impulse.z);
		}
		else
		{
			impulse = m_vLocalHitImpulse * hitImpulseMag;
			nmDebugf2("[%u] Final flinch impulse:%s(%p) Final impulse: %.3f (%.3f)", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, hitImpulseMag, m_vLocalHitImpulse.Mag());
			msg.addBool(NMSTR_PARAM(NM_APPLYIMPULSE_LOCAL_IMPULSE_INFO), true);
			Assertf(impulse==impulse, "Bad flinch m_vWorldHitImpulse - hitImpulseMag is %f, m_vLocalHitImpulse is %f %f %f, , impulse is %f %f %f", 
				hitImpulseMag, m_vLocalHitImpulse.x, m_vLocalHitImpulse.y, m_vLocalHitImpulse.z, impulse.x, impulse.y, impulse.z);
		}
		Assert(m_vLocalHitLocation==m_vLocalHitLocation);
		if (impulse==impulse)
		{
			msg.addVector3(NMSTR_PARAM(NM_APPLYIMPULSE_IMPULSE), impulse.x, impulse.y, impulse.z);
			msg.addBool(NMSTR_PARAM(NM_APPLYIMPULSE_LOCAL_HIT_POINT_INFO), true);
			msg.addVector3(NMSTR_PARAM(NM_APPLYIMPULSE_HIT_POINT), m_vLocalHitLocation.x, m_vLocalHitLocation.y, m_vLocalHitLocation.z);
		}
	}

	list.Post(pPed->GetRagdollInst());

	if (m_nHitComponent > -1)
	{
		ART::MessageParams msg;
		msg.addBool(NMSTR_PARAM(NM_START), true);
		msg.addInt(NMSTR_PARAM(NM_SHOTNEWBULLET_BODYPART), m_nHitComponent);
		msg.addBool(NMSTR_PARAM(NM_SHOTNEWBULLET_LOCAL_HIT_POINT_INFO), true);
		msg.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_POS_VEC3), m_vLocalHitLocation.x, m_vLocalHitLocation.y, m_vLocalHitLocation.z);
		Vector3 impulse = m_vWorldHitImpulse * hitImpulseMag;
		msg.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_VEL_VEC3), impulse.x, impulse.y, impulse.z);
		msg.addVector3(NMSTR_PARAM(NM_SHOTNEWBULLET_BULLET_NORMAL_VEC3), m_vWorldHitNormal.x, m_vWorldHitNormal.y, m_vWorldHitNormal.z);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_SHOTNEWBULLET_MSG), &msg);

#if __DEV && DEBUG_DRAW
		if (sm_bDebugDraw)
		{
			Matrix34 xRagdollComponentTransform;
			if (pPed != NULL && pPed->GetRagdollComponentMatrix(xRagdollComponentTransform, m_nHitComponent))
			{
				Vector3 vWorldHitLocation;
				xRagdollComponentTransform.Transform(m_vLocalHitLocation, vWorldHitLocation);

				CPhysics::ms_debugDrawStore.AddArrow(RCC_VEC3V(vWorldHitLocation), AddScaled(RCC_VEC3V(vWorldHitLocation), RCC_VEC3V(m_vWorldHitNormal), ScalarVFromF32(2.0f)), 0.2f, Color_green, 0, atStringHash("fling_debug_draw_normal"));
				CPhysics::ms_debugDrawStore.AddArrow(RCC_VEC3V(vWorldHitLocation), Add(RCC_VEC3V(vWorldHitLocation), RCC_VEC3V(m_vWorldHitImpulse)), 0.2f, Color_blue, 0, atStringHash("fling_debug_draw_impulse"));
				CPhysics::ms_debugDrawStore.AddSphere(RCC_VEC3V(vWorldHitLocation), 0.1f, Color_red, 0, atStringHash("fling_debug_draw_location"));
				char debugText[64];
				formatf(debugText, sizeof(debugText), "%s, Mag: %.3f", m_nHitComponent<RAGDOLL_NUM_COMPONENTS ? parser_RagdollComponent_Strings[m_nHitComponent] : "out of range", m_vWorldHitImpulse.Mag() * hitImpulseMag);
				CPhysics::ms_debugDrawStore.AddText(RCC_VEC3V(vWorldHitLocation), 0, 10, debugText, Color_white, 0, atStringHash("fling_debug_draw_text"));
			}
		}
#endif
	}
}


void CTaskNMFlinch::BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourSuccess(pPed, pFeedbackInterface);

	if(fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMinTime)
	{
		if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
		{
			m_bHasSucceeded = true;
		}
	}
}

void CTaskNMFlinch::BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFailure(pPed, pFeedbackInterface);

	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		if(!m_bBalanceFailed)
		{
			// If the balance has failed, see if we were hit, if we were generate an audio line
			// to say we were knocked over
			if( m_pFlinchEntity && m_pFlinchEntity->GetIsTypePed() )
			{
				CPed* pPedHitBy = static_cast<CPed*>(m_pFlinchEntity.Get());
				if( !pPedHitBy->IsInjured() && pPedHitBy->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_MELEE) )
				{
					pPedHitBy->NewSay("MELEE_KNOCK_DOWN");
				}
			}
			pPed->SetPedResetFlag( CPED_RESET_FLAG_KnockedToTheFloorByPlayer, true );

			AddTuningSet(&sm_Tunables.m_OnBalanceFailed);

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
					AddTuningSet(&sm_Tunables.m_OnBalanceFailedStairs);
				}
			}

			CNmMessageList list;
			
			ApplyTuningSetsToList(list);
			list.Post(pPed->GetRagdollInst());
		}

		m_bBalanceFailed = true;
	}
}

void CTaskNMFlinch::BehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourEvent(pPed, pFeedbackInterface);

	if(fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMinTime)
	{
		if(CTaskNMBehaviour::ProcessBalanceBehaviourEvent(pPed, pFeedbackInterface))
		{
			m_bHasSucceeded = true;
			m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
			m_nSuggestedBlendOption = BLEND_FROM_NM_STANDING;
		}
	}
}

void CTaskNMFlinch::ControlBehaviour(CPed* pPed)
{
	// TODO - hook up update message

	// get direction to flinch from via entity position, if available
	if(m_pFlinchEntity)
	{
		Vector3 vecFlinchPos = GetFlinchPos(pPed);

		ART::MessageParams msgFlinchUpd;
		msgFlinchUpd.addVector3(NMSTR_PARAM(NM_FLINCH_POS_VEC3), vecFlinchPos.x, vecFlinchPos.y, vecFlinchPos.z);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_FLINCH_MSG), &msgFlinchUpd);

		ART::MessageParams msgBodyBalance;
		msgBodyBalance.addVector3(NMSTR_PARAM(NM_BALANCE_LOOKAT_POS_VEC3), vecFlinchPos.x, vecFlinchPos.y, vecFlinchPos.z);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_BALANCE_MSG), &msgBodyBalance);
	}

	// lean in direction to keep us moving, if req'd
	if (!m_bBalanceFailed && m_nFlinchType == FLINCHTYPE_MELEE_PASSIVE && m_nTimeToStopLean > 0)
	{
		// stop the lean after a short period of time so the character can stabilize
		if (fwTimer::GetTimeInMilliseconds() > m_nTimeToStopLean)
		{
			ART::MessageParams msgLean;
			msgLean.addBool(NMSTR_PARAM(NM_START), false);
			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_BALANCE_LEAN_DIR_MSG), &msgLean);
			m_nTimeToStopLean = 0;
		}
	}
}

bool CTaskNMFlinch::FinishConditions(CPed* pPed)
{
	static float fallingSpeed = -4.0f;
	if (pPed->GetVelocity().z < fallingSpeed)
	{
		m_bHasFailed = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_HIGH_FALL;
	}

	int nFlags = FLAG_SKIP_DEAD_CHECK | FLAG_VEL_CHECK;
	if (pPed->GetIsDeadOrDying() || pPed->ShouldBeDead())
		nFlags |= FLAG_RELAX_AP_LOW_HEALTH;
	
	const CTaskNMBehaviour::Tunables::BlendOutThreshold* pBlendOutThreshold = NULL;
	const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_nWeaponHash);
	// If an AI is hit with a melee weapon in the head or neck then use a longer blend out threshold
	if (!pPed->IsPlayer() && pWeaponInfo != NULL && pWeaponInfo->GetIsMelee() && !pWeaponInfo->GetIsUnarmed() && 
		(m_nHitComponent == RAGDOLL_HEAD || m_nHitComponent == RAGDOLL_NECK || m_nHitComponent == RAGDOLL_SPINE3 ||
		m_nHitComponent == RAGDOLL_CLAVICLE_LEFT || m_nHitComponent == RAGDOLL_CLAVICLE_RIGHT))
	{
		pBlendOutThreshold = &CTaskNMBrace::sm_Tunables.m_HighVelocityBlendOut.m_Player;
	}

	return ProcessFinishConditionsBase(pPed, MONITOR_FALL, nFlags, pBlendOutThreshold);
}
