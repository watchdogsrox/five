// Filename   :	TaskNMBrace.cpp
// Description:	Natural Motion brace class (FSM version)

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
#include "phbound/boundcomposite.h"
#include "physics/shapetest.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "fwanimation/pointcloud.h"
#include "grcore/debugdraw.h"
#include "fwmaths\Angle.h"

// Game headers
//#include "Task/Physics/NmDebug.h"
#include "Animation\FacialData.h"
#include "camera/CamInterface.h"
#include "Event\EventDamage.h"
#include "modelinfo/PedModelInfo.h"
#include "Network\NetworkInterface.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Peds\PedMotionData.h"
#include "Peds\PedPlacement.h"
#include "Peds/Ped.h"
#include "PedGroup\PedGroup.h"
#include "Physics/debugcontacts.h"
#include "Physics\GtaInst.h"
#include "Physics\Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\TaskAnimatedFallback.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task/Physics/TaskNMBrace.h"
#include "task/Physics/TaskNMHighFall.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()

dev_u32 CTaskNMBrace::snBracePlayerTimeoutBeforeVelCheck = 5000;
dev_u32 CTaskNMBrace::snBraceTimeoutBeforeVelCheckForCatchfall = 1500;

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMBrace::Tunables CTaskNMBrace::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMBrace, 0x1f13be3a);
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CClonedNMBraceInfo
//////////////////////////////////////////////////////////////////////////

CClonedNMBraceInfo::CClonedNMBraceInfo(CEntity* pBraceEntity, u32 braceType)
: m_braceType((u8)braceType)
, m_bHasBraceEntity(false)
, m_bHasBraceType(braceType != CTaskNMBrace::BRACE_DEFAULT )
{
	if (pBraceEntity && pBraceEntity->GetIsDynamic() && static_cast<CDynamicEntity*>(pBraceEntity)->GetNetworkObject())
	{
		m_braceEntity = static_cast<CDynamicEntity*>(pBraceEntity)->GetNetworkObject()->GetObjectID();
		m_bHasBraceEntity = true;
	}
}

CClonedNMBraceInfo::CClonedNMBraceInfo()
: m_braceType(CTaskNMBrace::BRACE_DEFAULT)
, m_bHasBraceEntity(false)
{
}

CTaskFSMClone *CClonedNMBraceInfo::CreateCloneFSMTask()
{
	CEntity *pBraceEntity = NULL;

	if (m_bHasBraceEntity)
	{
		netObject* pObj = NetworkInterface::GetNetworkObject(m_braceEntity);

		if (pObj)
		{
			pBraceEntity = pObj->GetEntity();
		}
	}

	return rage_new CTaskNMBrace(10000, 10000, pBraceEntity, static_cast<CTaskNMBrace::eBraceType>(m_braceType));
}

//////////////////////////////////////////////////////////////////////////
// CTaskNMBrace
//////////////////////////////////////////////////////////////////////////

CTaskNMBrace::CTaskNMBrace(u32 nMinTime, u32 nMaxTime, CEntity* pBraceEntity, eBraceType eType /*= BRACE_DEFAULT*/, const Vector3& vInitialPedVelocity /*= VEC3_ZERO*/)
 : CTaskNMBehaviour(nMinTime, nMaxTime),
m_threatenedHelper(),
m_pBraceEntity(pBraceEntity),
m_eType(eType),
m_bBalanceFailed(false),
m_HasStartedCatchfall(false),
m_bRollingDownStairs(false),
m_StuckOnVehicle(false),
m_StuckUnderVehicle(false),
m_bHighVelocityReaction(false),
m_GoOverVehicle(false),
m_GoUnderVehicle(false),
m_ClearedVehicle(false),
m_LastHitCarTime(0),
m_pOverrides(NULL),
m_LocalSideSwipeImpulsePos(V_ZERO),
m_InitialPedVelocity(RCC_VEC3V(vInitialPedVelocity))
{
	SetInternalTaskType(CTaskTypes::TASK_NM_BRACE);
}

CTaskNMBrace::~CTaskNMBrace()
{
}

CTaskNMBrace::CTaskNMBrace(const CTaskNMBrace& otherTask)
: CTaskNMBehaviour(otherTask)
, m_threatenedHelper()
, m_pBraceEntity(otherTask.m_pBraceEntity)
, m_eType(otherTask.m_eType)
, m_bBalanceFailed(otherTask.m_bBalanceFailed)
, m_HasStartedCatchfall(otherTask.m_HasStartedCatchfall)
, m_bRollingDownStairs(otherTask.m_bRollingDownStairs)
, m_StuckOnVehicle(otherTask.m_StuckOnVehicle)
, m_StuckUnderVehicle(otherTask.m_StuckUnderVehicle)
, m_GoUnderVehicle(otherTask.m_GoUnderVehicle)
, m_GoOverVehicle(otherTask.m_GoOverVehicle)
, m_ClearedVehicle(otherTask.m_ClearedVehicle)
, m_LastHitCarTime(otherTask.m_LastHitCarTime)
, m_bHighVelocityReaction(otherTask.m_bHighVelocityReaction)
, m_pOverrides(otherTask.m_pOverrides)
, m_LocalSideSwipeImpulsePos(otherTask.m_LocalSideSwipeImpulsePos)
, m_InitialPedVelocity(otherTask.m_InitialPedVelocity)
{
	SetInternalTaskType(CTaskTypes::TASK_NM_BRACE);
}

CTaskInfo* CTaskNMBrace::CreateQueriableState() const
{
	return rage_new CClonedNMBraceInfo(m_pBraceEntity.Get(), static_cast<u32>(m_eType));
}

void CTaskNMBrace::BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourFailure(pPed, pFeedbackInterface);

	// probably flag failure straight away
	// need to look at what has failed and decide what to do
	//	m_bHasFailed = true;
	//	m_nSuggestedNextTask = CTaskTypes::TASK_SIMPLE_NM_RELAX;

	m_threatenedHelper.BehaviourFailure(pFeedbackInterface);

	if(m_threatenedHelper.HasBalanceFailed())
	{
		if(!m_bBalanceFailed)
		{
			Matrix34 ragdollCompMatrix = MAT34V_TO_MATRIX34(pPed->GetMatrix());
			pPed->GetRagdollComponentMatrix(ragdollCompMatrix, RAGDOLL_SPINE3);

			// if ped fell on stairs, try and make them roll down the slope
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
					sm_Tunables.m_OnBalanceFailedStairs.Post(*pPed, this);
				}
			}

			// If we're a cop & have been knocked over by a player, then give them a 1-star wanted level.
			if(pPed->GetPedType()==PEDTYPE_COP && m_pBraceEntity && m_pBraceEntity->GetIsTypeVehicle())
			{
				CPed * pDriver = ((CVehicle*)m_pBraceEntity.Get())->GetDriver();
				if (pDriver && pDriver->IsPlayer())
				{
					CWanted * pPlayerWanted = pDriver->GetPlayerWanted();
					if(pPlayerWanted->GetWantedLevel() < WANTED_LEVEL1)
						pPlayerWanted->SetWantedLevel(VEC3V_TO_VECTOR3(pDriver->GetTransform().GetPosition()), WANTED_LEVEL1, fwRandom::GetRandomNumberInRange(1000, 3000), WL_FROM_TASK_NM_BRACE);
				}
			}
		}
		m_bBalanceFailed = true;
	}
}

void CTaskNMBrace::SetSideSwipeImpulsePos(Vec3V_In worldPos, const CPed* pPed)
{
	Mat34V ragdollCompMatrix = pPed->GetMatrix();
	pPed->GetRagdollComponentMatrix(RC_MATRIX34(ragdollCompMatrix), RAGDOLL_SPINE3);
	m_LocalSideSwipeImpulsePos = UnTransformFull(ragdollCompMatrix, worldPos);
}

void CTaskNMBrace::BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
{
	// Call the base class version to update feedback flags as necessary.
	CTaskNMBehaviour::BehaviourSuccess(pPed, pFeedbackInterface);

	// check min time and then flag success
	if((fwTimer::GetTimeInMilliseconds() > m_nStartTime + m_nMinTime) && CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BALANCE_FB))
	{
		m_bHasSucceeded = true;
		m_nSuggestedNextTask = CTaskTypes::TASK_BLEND_FROM_NM;
		m_nSuggestedBlendOption = BLEND_FROM_NM_STANDING;
	}
}

void CTaskNMBrace::BehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface)
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
#if __DEV
	if(CTaskNMBehaviour::QueryNmFeedbackMessage(pFeedbackInterface, NM_BRACE_FB))
	{
		const int  iFlags = pFeedbackInterface->m_args[0].m_int;

		const bool bLeft  = (iFlags & 1) != 0;
		const bool bRight = (iFlags & 2) != 0;
		const bool bLeftChanged  = (iFlags & 4) != 0;
		const bool bRightChanged = (iFlags & 8) != 0;

		/// left hand changed ?
		if(bLeftChanged)
		{
			if (bLeft)
			{
				/// 4 Jonathon: at this time the hands make contact
				Displayf("Left hand bracing\n");
			}
			else
			{
				/// 4 Jonathon: at this time the hands go back to a relaxed pose,
				Displayf("Left hand NOT bracing\n");
			}
		}

		/// left hand changed ?
		if(bRightChanged)
		{
			if (bRight)
			{
				/// 4 Jonathon: at this time the hands make contact
				Displayf("Right hand bracing\n");
			}
			else
			{
				/// 4 Jonathon: at this time the hands go back to a relaxed pose,
				Displayf("Right hand NOT bracing\n");
			}
		}
	}
#endif
}


//CTaskNMBrace::Parameters CTaskNMBrace::ms_Parameters[CTaskNMBrace::NUM_BRACE_TYPES];

const char* CTaskNMBrace::ms_TypeToString[CTaskNMBrace::NUM_BRACE_TYPES] =
{
	"DEFAULT", "WEAK"
};

void CTaskNMBrace::Cleanup()
{
	CPed* pPed = GetPed();
	// restore original max angular velocity
	if (pPed && sm_Tunables.m_AngularVelocityLimits.m_Apply && pPed->GetCollider())
	{
		phCollider* pCollider = pPed->GetCollider();
		pCollider->SetMaxAngSpeed(m_InitialMaxAngSpeed);
	}
}

bool CTaskNMBrace::HandlesDeadPed()
{
	CPed *pPed = GetPed();

	if (pPed && (MagSquared(RCC_VEC3V(pPed->GetVelocity())).Getf() > 0.25f || !m_ClearedVehicle) )
	{
		return true;
	}
	return false;
}

void CTaskNMBrace::StartBehaviour(CPed* pPed)
{
	nmDebugf1("CTaskNMBrace type %d, %s\n", m_eType, ms_TypeToString[m_eType]);

	if (m_pBraceEntity && m_pBraceEntity->GetIsTypeVehicle())
	{
		m_pOverrides = sm_Tunables.m_VehicleOverrides.Get(static_cast<CVehicle*>(m_pBraceEntity.Get()));
	}

	// cache off the original max angular velocity (we're going to mess around with it later)
	if (sm_Tunables.m_AngularVelocityLimits.m_Apply && pPed->GetCollider())
	{
		phCollider* pCollider = pPed->GetCollider();
		m_InitialMaxAngSpeed = pCollider->GetMaxAngSpeed();
	}

	// get velocity of other inst if it's active
	Vec3V vecVel(V_ZERO);
	ScalarV velMag(V_ZERO);
	if (m_pBraceEntity)
	{
		nmAssert(pPed->GetRagdollInst());
		phCollider *pOtherCollider = m_pBraceEntity->GetCurrentPhysicsInst() ? CPhysics::GetSimulator()->GetCollider(m_pBraceEntity->GetCurrentPhysicsInst()) : NULL;
		if(pOtherCollider)
		{
			vecVel = pOtherCollider->GetLocalVelocity(pPed->GetRagdollInst()->GetPosition().GetIntrin128());
		}
		else if (m_pBraceEntity->GetIsPhysical())
		{
			vecVel = VECTOR3_TO_VEC3V(static_cast<CPhysical*>(m_pBraceEntity.Get())->GetLocalSpeed(VEC3V_TO_VECTOR3(pPed->GetRagdollInst()->GetPosition()), true));
		}

		ScalarV velMag2 = MagSquared(vecVel);
		if (velMag2.Getf() >= SMALL_FLOAT)
		{
			Vec3V vecPedVel(m_InitialPedVelocity);

			velMag = Sqrt(velMag2);
			Vec3V vecVelDirection = InvScale(vecVel, velMag);

			// Never want to counter more than the velocity of the physical itself
			ScalarV velDotProduct = rage::Min(Dot(m_InitialPedVelocity, vecVelDirection), velMag);
			// Figure out how much of the ped's velocity is in the same direction as the physical's and then subtract that out from the physical's velocity
			vecPedVel = Scale(vecVelDirection, velDotProduct);

			vecVel = Subtract(vecVel, vecPedVel);
		}

		velMag = Mag(vecVel);

		vecVel = NormalizeSafe(vecVel, Vec3V(V_ZERO));
		Assert(vecVel.GetXf() == vecVel.GetXf());
		if (vecVel.GetXf() != vecVel.GetXf())
		{
			vecVel.ZeroComponents();
			velMag = ScalarV(V_ZERO);
		}
	}

	m_bHighVelocityReaction = velMag.Getf() > sm_Tunables.m_LowVelocityReactionThreshold;

	if (m_bHighVelocityReaction)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PreferInjuredGetup, true);
		m_GoUnderVehicle = ShouldUseUnderVehicleReaction(pPed, m_pBraceEntity);
		m_GoOverVehicle = !m_GoUnderVehicle;
		// Initialize the last time a car has hit us here in case we don't register any more vehicle collisions
		m_LastHitCarTime = fwTimer::GetTimeInMilliseconds();
	}
	
	AddTuningSet(&sm_Tunables.m_Start);

	if (m_GoUnderVehicle)
	{
		AddTuningSet(&sm_Tunables.m_UnderVehicle);
	}
	else if (m_GoOverVehicle)
	{
		AddTuningSet(&sm_Tunables.m_OverVehicle);
	}

	if (m_eType == BRACE_WEAK)
	{
		AddTuningSet(&sm_Tunables.m_Weak);
	}

	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_OnStairs))
	{
		AddTuningSet(&sm_Tunables.m_OnStairs);
	}

	if (m_bHighVelocityReaction)
	{
		AddTuningSet(&sm_Tunables.m_HighVelocity);
	}

	CNmMessageList list;

	ApplyTuningSetsToList(list);

	list.Post(pPed->GetRagdollInst());

	// If moving slowly, apply a tunable initial force based on the velocity of the vehicle
	if (m_pBraceEntity && m_pBraceEntity->GetIsTypeVehicle() && m_pBraceEntity->GetCurrentPhysicsInst() && (sm_Tunables.m_InitialForce.m_VelocityMax > sm_Tunables.m_InitialForce.m_VelocityMin) && !m_bHighVelocityReaction && (m_eType==BRACE_WEAK || m_eType==BRACE_DEFAULT))
	{
		Tunables::InitialForce& initialForce = m_pOverrides && m_pOverrides->m_OverrideInitialForce ? m_pOverrides->m_InitialForce : sm_Tunables.m_InitialForce;

		float t = (velMag.Getf()-initialForce.m_VelocityMin)/(initialForce.m_VelocityMax - initialForce.m_VelocityMin);
		t=Clamp(t, 0.0f, 1.0f);
		float impulseMag = Lerp(t, initialForce.m_ForceAtMinVelocity, initialForce.m_ForceAtMaxVelocity)*fwTimer::GetTimeStep();

		if (initialForce.m_ScaleWithUpright)
		{
			// Decrease the force magnitude as the ped falls down
			phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
			Mat34V boundMat;
			Transform(boundMat, pPed->GetRagdollInst()->GetMatrix(), bound->GetCurrentMatrix(RAGDOLL_BUTTOCKS));
			Vector3 pelvisPos = VEC3V_TO_VECTOR3(boundMat.GetCol3());
			Transform(boundMat, pPed->GetRagdollInst()->GetMatrix(), bound->GetCurrentMatrix(RAGDOLL_NECK));
			Vector3 pelvisToHead = VEC3V_TO_VECTOR3(boundMat.GetCol3()) - pelvisPos;
			pelvisToHead.Normalize();

			float uprightDot = Max(0.0f, pelvisToHead.z);
			impulseMag*=uprightDot;
		}

		Assert(impulseMag == impulseMag);
		if (impulseMag != impulseMag)
		{
			impulseMag = 0.0f;
		}

		vecVel *= ScalarV(impulseMag);
		pPed->ApplyImpulse(RCC_VECTOR3(vecVel), VEC3_ZERO, RAGDOLL_BUTTOCKS, true);
	}

	// if our ped has a weapon, see if he will want to use it
	if(!m_threatenedHelper.HasBalanceFailed() && m_threatenedHelper.PedCanThreaten(pPed))
	{
		int braceMinTimeBeforeGunThreaten = 0;
		int braceMaxTimeBeforeGunThreaten = 800;
		int braceMinTimeBetweenFireGun = 500;
		int braceMaxTimeBetweenFireGun = 900;

		CThreatenedHelper::Parameters timingParams;
		timingParams.m_nMinTimeBeforeGunThreaten = static_cast<u16>(braceMinTimeBeforeGunThreaten);
		timingParams.m_nMaxTimeBeforeGunThreaten = static_cast<u16>(braceMaxTimeBeforeGunThreaten);
		timingParams.m_nMinTimeBeforeFireGun = static_cast<u16>(braceMinTimeBetweenFireGun);
		timingParams.m_nMaxTimeBeforeFireGun = static_cast<u16>(braceMaxTimeBetweenFireGun);

		// currently we only know how to threaten guys in cars
		if(m_pBraceEntity && m_pBraceEntity->GetIsTypeVehicle() && ((CVehicle *)m_pBraceEntity.Get())->GetDriver())
		{
			CPed* vehDriver = ((CVehicle *)m_pBraceEntity.Get())->GetDriver();

			if (pPed->GetPedIntelligence()->IsThreatenedBy(*vehDriver) && !m_threatenedHelper.HasStarted())
			{
				// if we're dealing with the player, just get him to wave the gun
				// at the incoming car; user can then choose to fire the gun themselves
				if (pPed->IsPlayer())
				{
					m_threatenedHelper.Start(CThreatenedHelper::THREATENED_GUN_THREATEN, timingParams);
				}
				else
				{
					// TBD - distinguish between whether the ped 'dislikes' the driver or really hates
					// his guts enough to empty a clip of ammo into his windscreen
					// ...for now, lets just shoot at him
					m_threatenedHelper.Start(CThreatenedHelper::THREATENED_GUN_THREATEN_AND_FIRE, timingParams);
				}				
			}
		}
	}

	CFacialDataComponent* pFacialData = pPed->GetFacialData();
	if(pFacialData)
	{
		if(fwRandom::GetRandomTrueFalse())
			pFacialData->PlayFacialAnim(pPed, FACIAL_CLIP_SHOCKED);
		else
			pFacialData->PlayFacialAnim(pPed, FACIAL_CLIP_ANGRY);
	}

	// If peds are hit by a car, they get a bit pissed.
	if( pPed->PopTypeIsMission() && pPed->GetIsInVehicle() && pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_CAR)
	{
		if(pPed->CheckBraveryFlags(BF_GET_PISSED_WHEN_HIT_BY_CAR))
		{
			if(!pPed->NewSay("GENERIC_FUCK_OFF"))
				if(!pPed->NewSay("GENERIC_DEJECTED"))
					pPed->NewSay("SHIT");
		}
		else
		{
			if(!pPed->NewSay("GENERIC_DEJECTED"))
				if(!pPed->NewSay("GENERIC_FUCK_OFF"))
					pPed->NewSay("SHIT");
		}
	}

	if(pPed->GetMotionData())
	{
		pPed->GetMotionData()->SetUsingStealth(false);
	}
}

dev_float sfBrakePlayerImpulseThresholdForTimerReset = 50.0f;
//
void CTaskNMBrace::ControlBehaviour(CPed* pPed)
{
	float fImpulseThreshold = 5.0f;
	if(pPed->IsPlayer())
		fImpulseThreshold = sfBrakePlayerImpulseThresholdForTimerReset;

	AddTuningSet(&sm_Tunables.m_Update);

	CVehicle* pVehicle = NULL;
	if (pPed->GetFrameCollisionHistory()->GetMaxCollisionImpulseMagLastFrame() > 0.0f)
	{
		const CCollisionRecord* pColRecord = pPed->GetFrameCollisionHistory()->GetMostSignificantCollisionRecordOfType(ENTITY_TYPE_VEHICLE);
		if (pColRecord != NULL)
		{
			pVehicle = static_cast<CVehicle*>(pColRecord->m_pRegdCollisionEntity.Get());
		}
	}

	phCollider* pCollider = pPed->GetCollider();
	pCollider->SetMaxAngSpeed(sm_Tunables.m_AngularVelocityLimits.m_Max);

	if (
		!pVehicle && 
		sm_Tunables.m_AngularVelocityLimits.m_Apply && 
		(fwTimer::GetTimeInMilliseconds() - m_nStartTime) > sm_Tunables.m_AngularVelocityLimits.m_Delay &&
		pPed->GetCollider()
		)
	{
		Assert(pPed->GetRagdollInst());
		if (pPed->GetRagdollInst()->GetArchetype())
		{
			phArchetypeDamp* pArch = static_cast<phArchetypeDamp*>(pPed->GetRagdollInst()->GetArchetype());
			pArch->ActivateDamping(phArchetypeDamp::ANGULAR_C, sm_Tunables.m_AngularVelocityLimits.m_Constant);
			pArch->ActivateDamping(phArchetypeDamp::ANGULAR_V, sm_Tunables.m_AngularVelocityLimits.m_Velocity);
			pArch->ActivateDamping(phArchetypeDamp::ANGULAR_V2, sm_Tunables.m_AngularVelocityLimits.m_Velocity2);
		}
	}

	if (pVehicle)
	{
		m_LastHitCarTime = fwTimer::GetTimeInMilliseconds();
	}

	if (!m_ClearedVehicle)
	{
		if (!pVehicle && m_LastHitCarTime>0)
		{
			if ((fwTimer::GetTimeInMilliseconds() - m_LastHitCarTime)>(pPed->IsPlayer() ? sm_Tunables.m_PlayerClearedVehicleDelay : sm_Tunables.m_AiClearedVehicleDelay))
			{
				AddTuningSet(&sm_Tunables.m_ClearedVehicle);
				m_ClearedVehicle = true;
			}
		}
	}

	Tunables::StuckOnVehicle& stuckSettings = (m_pOverrides && m_pOverrides->m_OverrideStuckOnVehicleSets) ? m_pOverrides->m_StuckOnVehicle : sm_Tunables.m_StuckOnVehicle;
	bool addBaseSet = m_pOverrides && m_pOverrides->m_AddToBaseStuckOnVehicleSets;

	// turn on or off the stuck on vehicle mode.
	float velMag = pPed->GetVelocity().Mag();

	// turn on or off the stuck under vehicle mode
	if (pVehicle && velMag>=stuckSettings.m_UnderVehicleVelocityThreshold && 
		((fwTimer::GetTimeInMilliseconds() - m_nStartTime)>stuckSettings.m_UnderVehicleInitialDelay) &&
		((fwTimer::GetTimeInMilliseconds() - m_LastHitCarTime) < stuckSettings.m_UnderVehicleContinuousContactTime) &&
		IsStuckUnderVehicle(pPed, pVehicle)
		)
	{
		if (!m_StuckUnderVehicle)
		{
			StartStuckUnderVehicle(pPed, stuckSettings, addBaseSet);

			m_StuckUnderVehicle = true;
		}
	}
	else
	{
		if (m_StuckUnderVehicle)
		{
			EndStuckUnderVehicle(pPed, stuckSettings, addBaseSet);

			m_StuckUnderVehicle = false;
		}
	}

	if (pVehicle && velMag>=stuckSettings.m_VelocityThreshold && 
		((fwTimer::GetTimeInMilliseconds() - m_nStartTime)>stuckSettings.m_InitialDelay) &&
		m_ContinuousContactTime > stuckSettings.m_ContinuousContactTime && m_ContinuousContactTime < CTaskNMBehaviour::sm_Tunables.m_StuckOnVehicleMaxTime &&
		( !IsStuckUnderVehicle(pPed, pVehicle) || ((velMag < stuckSettings.m_UnderCarMaxVelocity)
		&& pPed->GetPedResetFlag(CPED_RESET_FLAG_RagdollOnVehicle)))
		)
	{
		if (!m_StuckOnVehicle)
		{
			StartStuckOnVehicle(pPed, stuckSettings, addBaseSet);

			m_StuckOnVehicle = true;
		}

		UpdateStuckOnVehicle(pPed, stuckSettings, addBaseSet);
	}
	else
	{
		if (m_StuckOnVehicle)
		{
			EndStuckOnVehicle(pPed, stuckSettings, addBaseSet);

			m_StuckOnVehicle = false;
		}
	}

	if (!m_ClearedVehicle)
	{
		if (pVehicle)
		{
			m_LastHitCarTime = fwTimer::GetTimeInMilliseconds();
		}
		else if (m_LastHitCarTime>0)
		{
			if ((fwTimer::GetTimeInMilliseconds() - m_LastHitCarTime)>(pPed->IsPlayer() ? sm_Tunables.m_PlayerClearedVehicleDelay : sm_Tunables.m_AiClearedVehicleDelay))
			{
				AddTuningSet(&sm_Tunables.m_ClearedVehicle);
				m_ClearedVehicle = true;
			}
		}
	}


	if (pPed->GetIsDeadOrDying() || pPed->ShouldBeDead())
	{
		AddTuningSet(&sm_Tunables.m_Dead);
	}

	CNmMessageList list;

	ApplyTuningSetsToList(list);

	list.Post(pPed->GetRagdollInst());

	// get velocity of other inst if it's active
	Vec3V vecVel(V_ZERO);
	if (m_pBraceEntity && m_pBraceEntity->GetCurrentPhysicsInst() && m_pBraceEntity->GetCurrentPhysicsInst()->IsInLevel())
	{
		phCollider *pOtherCollider = CPhysics::GetSimulator()->GetCollider(m_pBraceEntity->GetCurrentPhysicsInst());
		if(pOtherCollider)
		{
			nmAssert(pPed->GetRagdollInst());
			vecVel = pOtherCollider->GetLocalVelocity(pPed->GetRagdollInst()->GetPosition().GetIntrin128());
		}
		else if (m_pBraceEntity->GetIsPhysical())
		{
			vecVel = RCC_VEC3V(static_cast<CPhysical*>(m_pBraceEntity.Get())->GetVelocity());
		}
	}

	// apply a lateral force to the hit ped based on the vehicle matrix position
	if (pVehicle && m_pOverrides && m_pOverrides->m_LateralForce.ShouldApply(m_nStartTime, pVehicle!=NULL))
	{
		Vec3V vecForce;
		Vec3V vehicleRight = pVehicle->GetMatrix().a();
		Vec3V toPed = NormalizeSafe(pPed->GetMatrix().d() - pVehicle->GetMatrix().d(), Vec3V(V_ZERO));
		
		ScalarV rightDot = Dot(vehicleRight, toPed);
		if (rightDot.Getf()>0.0f)
		{
			// apply the force to the right
			vecForce = vehicleRight;
		}
		else
		{
			vecForce = -vehicleRight;
		}

		m_pOverrides->m_LateralForce.GetForceVec(vecForce, vecVel, pVehicle, pPed);

		if (MagSquared(vecForce).Getf()>0.0f)
		{
			pPed->ApplyImpulse(RCC_VECTOR3(vecForce), VEC3_ZERO, RAGDOLL_SPINE0, true);
		}
	}

	// Apply an additional force to the chest in the direction of motion
	if (m_bHighVelocityReaction && m_GoOverVehicle && sm_Tunables.m_ChestForce.ShouldApply(m_nStartTime, pVehicle!=NULL ))
	{
		Vec3V vecForce = NormalizeSafe(vecVel, Vec3V(V_ZERO));

		if (MagSquared(vecForce).Getf()>0.0f)
		{
			// pitch the force to the chest by the specified amount
			Vec3V vecUp(V_Z_AXIS_WZERO);
			Vec3V vecAxis = Cross(vecForce, vecUp);
			vecAxis = Normalize(vecAxis);
			QuatV rotation = QuatVFromAxisAngle(vecAxis, ScalarV(CTaskNMBrace::sm_Tunables.m_ChestForcePitch));
			vecForce = Transform(rotation, vecForce);

			sm_Tunables.m_ChestForce.GetForceVec(vecForce, vecVel, pVehicle, pPed);

			pPed->ApplyImpulse(RCC_VECTOR3(vecForce), VEC3_ZERO, RAGDOLL_SPINE0, true);
		}
	}

	Tunables::ApplyForce& rootLiftForce = m_pOverrides && m_pOverrides->m_OverrideRootLiftForce ? m_pOverrides->m_RootLiftForce : sm_Tunables.m_RootLiftForce;

	// Apply an upward force to the root
	if (m_bHighVelocityReaction && rootLiftForce.ShouldApply(m_nStartTime, pVehicle!=NULL ))
	{
		Vec3V vecForce(V_Z_AXIS_WZERO);
		rootLiftForce.GetForceVec(vecForce, vecVel, pVehicle, pPed);
		if (MagSquared(vecForce).Getf()>0.0f)
		{
			pPed->ApplyImpulse(RCC_VECTOR3(vecForce), VEC3_ZERO, RAGDOLL_BUTTOCKS, true);
		}
	}

	// Apply an upward force to the feet
	if (m_bHighVelocityReaction && sm_Tunables.m_FeetLiftForce.ShouldApply(m_nStartTime, pVehicle!=NULL ))
	{
		Vec3V vecForce(V_Z_AXIS_WZERO);
		sm_Tunables.m_FeetLiftForce.GetForceVec(vecForce, vecVel, pVehicle, pPed);
		if (MagSquared(vecForce).Getf()>0.0f)
		{
			vecForce *= ScalarV(V_HALF);
			pPed->ApplyImpulse(RCC_VECTOR3(vecForce), VEC3_ZERO, RAGDOLL_FOOT_LEFT, true);
			pPed->ApplyImpulse(RCC_VECTOR3(vecForce), VEC3_ZERO, RAGDOLL_FOOT_RIGHT, true);
		}
	}

	Tunables::ApplyForce& flipForce = m_pOverrides && m_pOverrides->m_OverrideFlipForce ? m_pOverrides->m_FlipForce : sm_Tunables.m_FlipForce;

	// flip the ped forward or backward depending on the height of the root versus the centre of mass of the vehicle
	if (m_bHighVelocityReaction && m_GoUnderVehicle && flipForce.ShouldApply(m_nStartTime, pVehicle!=NULL))
	{
		Vec3V vecForce = NormalizeSafe(vecVel, Vec3V(V_ZERO));
		flipForce.GetForceVec(vecForce, vecVel, pVehicle, pPed);

		if (MagSquared(vecForce).Getf()>0.0f)
		{
			vecForce = Negate(vecForce);

			Vec3V chestForce(vecForce);
			// pitch the force to the chest by the specified amount
			Vec3V vecUp(V_Z_AXIS_WZERO);
			Vec3V vecAxis = Cross(vecForce, vecUp);
			vecAxis = Normalize(vecAxis);
			QuatV rotation = QuatVFromAxisAngle(vecAxis, ScalarV(CTaskNMBrace::sm_Tunables.m_ChestForcePitch));
			chestForce = Negate(Transform(rotation, chestForce));

			pPed->ApplyImpulse(RCC_VECTOR3(chestForce), VEC3_ZERO, RAGDOLL_SPINE3, true);

			vecForce *= ScalarV(V_HALF);
			pPed->ApplyImpulse(RCC_VECTOR3(vecForce), VEC3_ZERO, RAGDOLL_FOOT_LEFT, true);
			pPed->ApplyImpulse(RCC_VECTOR3(vecForce), VEC3_ZERO, RAGDOLL_FOOT_RIGHT, true);
		}		
	}

	if (m_eType==BRACE_CAPSULE_HIT)
	{
		if (sm_Tunables.m_CapsuleHitForce.ShouldApply(m_nStartTime, pVehicle!=NULL))
		{
			Vec3V vecForce = NormalizeSafe(vecVel, Vec3V(V_ZERO));
			sm_Tunables.m_CapsuleHitForce.GetForceVec(vecForce, vecVel, pVehicle, pPed);

			if (MagSquared(vecForce).Getf()>0.0f)
			{
				pPed->ApplyImpulse(RCC_VECTOR3(vecForce), VEC3_ZERO, RAGDOLL_SPINE3, true);
			}
		}
	}

	if (m_eType==BRACE_SIDE_SWIPE)
	{
		if (sm_Tunables.m_SideSwipeForce.ShouldApply(m_nStartTime, pVehicle!=NULL))
		{
			Vec3V vecForce = NormalizeSafe(vecVel, Vec3V(V_ZERO));
			sm_Tunables.m_SideSwipeForce.GetForceVec(vecForce, vecVel, pVehicle, pPed);

			if (MagSquared(vecForce).Getf()>0.0f)
			{
				pPed->ApplyImpulse(RCC_VECTOR3(vecForce), RCC_VECTOR3(m_LocalSideSwipeImpulsePos), RAGDOLL_SPINE3, true);
			}
		}
	}

	CTaskNMBehaviour::ControlBehaviour(pPed);
	m_threatenedHelper.ControlBehaviour(pPed);

	// Brace needs to continually update the position it's bracing from
	UpdateBracePosition(pPed);
}

void CTaskNMBrace::UpdateBracePosition(CPed* pPed)
{
	if(pPed->GetRagdollInst()->GetNMAgentID() != -1 && !m_HasStartedCatchfall)
	{
		if(m_pBraceEntity && m_pBraceEntity->GetCurrentPhysicsInst() && m_pBraceEntity->GetCurrentPhysicsInst()->IsInLevel())
		{
			Vector3 vecPos;//, vecVel;

			Matrix34 matRoot;
			pPed->GetBoneMatrix(matRoot, BONETAG_ROOT);

			static float height = 0.3f;
			Vector3 vecStartPos = matRoot.d + Vector3(0.0f, 0.0f, height);

			Vector3 vBraceEntityPosition = VEC3V_TO_VECTOR3(m_pBraceEntity->GetTransform().GetPosition());

			if (!rage::FPIsFinite(vBraceEntityPosition.x) || !rage::FPIsFinite(vBraceEntityPosition.y) || !rage::FPIsFinite(vBraceEntityPosition.z))
			{
				nmAssertf(0, "Invalid brace entity position! m_pBraceEntity=%p, x=%f, y=%f, z=%f.", m_pBraceEntity.Get(), vBraceEntityPosition.x, vBraceEntityPosition.y, vBraceEntityPosition.z);
				return;
			}

			static float fEntityHeightOffset = -0.4f;
			vBraceEntityPosition.z += fEntityHeightOffset;

			float fDistance = (vBraceEntityPosition - vecStartPos).Mag();
			const Vector3 vDirection = (vBraceEntityPosition - vecStartPos) / fDistance;
			static float fLength = 2.5f;
			Vector3 vecEndPos = vecStartPos + vDirection * fLength;
			//grcDebugDraw::Sphere(vecStartPos, 0.2f, Color32(1.0f,0.0f,0.0f,1.0f));
			//grcDebugDraw::Sphere(vecEndPos, 0.2f, Color32(0.0f,1.0f,0.0f,1.0f));

			WorldProbe::CShapeTestCapsuleDesc sphereDesc;
			WorldProbe::CShapeTestFixedResults<> sphereResult;
			sphereDesc.SetResultsStructure(&sphereResult);
			static float radius = 0.1f;
			sphereDesc.SetCapsule(vecStartPos, vecEndPos, radius);
			sphereDesc.SetIsDirected(true);
			sphereDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
			sphereDesc.SetIncludeEntity(m_pBraceEntity);
			if(WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc))
			{
				vecPos = sphereResult[0].GetHitPosition();

				//vecVel.Zero();
				//if(m_pBraceEntity->GetIsPhysical())
				//	vecVel = ((CPhysical*)m_pBraceEntity.Get())->GetLocalSpeed(vecPos, true);

#if __DEV
				if(CTaskNMBehaviour::ms_bDisplayDebug)
				{
					Vector3 vecRight(VEC3V_TO_VECTOR3(m_pBraceEntity->GetTransform().GetA()));
					Vector3 vecForward(VEC3V_TO_VECTOR3(m_pBraceEntity->GetTransform().GetB()));
					Vector3 vecUp(VEC3V_TO_VECTOR3(m_pBraceEntity->GetTransform().GetC()));
					grcDebugDraw::Line(vecPos - 0.25f*vecRight, vecPos + 0.25f*vecRight, Color32(1.0f,0.0f,0.0f,1.0f));
					grcDebugDraw::Line(vecPos - 0.25f*vecForward, vecPos + 0.25f*vecForward, Color32(0.0f,1.0f,0.0f,1.0f));
					grcDebugDraw::Line(vecPos - 0.25f*vecUp, vecPos + 0.25f*vecUp, Color32(0.0f,0.0f,1.0f,1.0f));
				}
#endif
			}
			else
			{
				vecPos = vBraceEntityPosition;
				vecPos.z -= 0.3f;

				//vecVel.Zero();
				//if(m_pBraceEntity->GetIsPhysical())
				//	vecVel = ((CPhysical*)m_pBraceEntity.Get())->GetLocalSpeed(vPedPosition, true);
			}

			// get position to look at - preferably the driver's head
			Vector3 vecLookAtPos = vBraceEntityPosition;
			if(m_pBraceEntity->GetIsTypeVehicle() && ((CVehicle *)m_pBraceEntity.Get())->GetDriver())
			{
				((CVehicle *)m_pBraceEntity.Get())->GetDriver()->GetBonePosition(vecLookAtPos, BONETAG_HEAD);
			}
			Vector3 worldPosTarget(vecLookAtPos); // used by weapon firing code later

			vecPos = VEC3V_TO_VECTOR3(m_pBraceEntity->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vecPos)));
			vecLookAtPos = VEC3V_TO_VECTOR3(m_pBraceEntity->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vecLookAtPos)));

			ART::MessageParams msg;
			Assert(vecPos.x == vecPos.x);
			Assert(vecPos.Mag() < 9999.0f);
			if (vecPos.x != vecPos.x || vecPos.Mag() >= 9999.0f)
			{
				return;
			}
			msg.addVector3(NMSTR_PARAM(NM_BRACE_POS_VEC3), vecPos.x, vecPos.y, vecPos.z);
			msg.addVector3(NMSTR_PARAM(NM_BRACE_LOOKAT_VEC3), vecLookAtPos.x, vecLookAtPos.y, vecLookAtPos.z);

			msg.addInt(NMSTR_PARAM(NM_BRACE_INSTANCE_LEVEL_INDEX), m_pBraceEntity->GetCurrentPhysicsInst()->GetLevelIndex());
			
			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_BRACE_MSG), &msg);

			ART::MessageParams msgFlinch;
			msgFlinch.addVector3(NMSTR_PARAM(NM_FLINCH_POS_VEC3), worldPosTarget.x, worldPosTarget.y, worldPosTarget.z);
			pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_FLINCH_MSG), &msgFlinch);

			m_threatenedHelper.Update(pPed, vecLookAtPos, worldPosTarget, m_pBraceEntity->GetCurrentPhysicsInst()->GetLevelIndex());

			// Check if the velocity is low enough and relax the body 
			if(m_threatenedHelper.HasBalanceFailed() && fwTimer::GetTimeInMilliseconds() > m_nStartTime + snBraceTimeoutBeforeVelCheckForCatchfall)
			{
				float velMag = pPed->GetVelocity().Mag();
				static float magLim = 5.0f;
				if (velMag < magLim)
				{	
					sm_Tunables.m_OnBalanceFailed.Post(*pPed, this);
					m_HasStartedCatchfall = true;
				}
			}
		}
	}
}

bool CTaskNMBrace::FinishConditions(CPed* pPed)
{
	int nFlags = FLAG_SKIP_DEAD_CHECK;
	if (pPed->GetIsDeadOrDying() || pPed->ShouldBeDead())
		nFlags |= FLAG_RELAX_AP_LOW_HEALTH;
	if(m_bBalanceFailed)
		nFlags |= FLAG_VEL_CHECK;

	// for the player we want to use a vel check even if the balance hasn't failed (or succeeded)
	// after a certain amount of time, so you can't get stuck in a pose where you're unable to balance
	// (wedgied on a car door for example)
	if(pPed->IsPlayer() && fwTimer::GetTimeInMilliseconds() > m_nStartTime + snBracePlayerTimeoutBeforeVelCheck)
		nFlags |= (FLAG_VEL_CHECK | FLAG_QUIT_IN_WATER);


	if (pPed->GetVelocity().z < -sm_Tunables.m_FallingSpeedForHighFall || (m_LastHitCarTime > 0 && (fwTimer::GetTimeInMilliseconds() - m_LastHitCarTime) > (pPed->IsPlayer() ? sm_Tunables.m_PlayerClearedVehicleSmartFallDelay : sm_Tunables.m_AiClearedVehicleSmartFallDelay )))
	{
		nmDebugf2("[%u] NM Brace:%s(%p) Aborting for high fall: zVel: %.3f", fwTimer::GetTimeInMilliseconds(), pPed->GetModelName(), pPed, pPed->GetVelocity().z);
		m_bHasFailed = true;

		CTaskNMHighFall* pNewTask = NULL;
		static dev_u32 snBraceMaxTimeSinceLastCarHit = 1000;
		if (pPed->GetFrameCollisionHistory()->HasCollidedWithAnyOfTypes(ENTITY_TYPE_VEHICLE | ENTITY_TYPE_BUILDING | ENTITY_TYPE_ANIMATED_BUILDING) ||
			((fwTimer::GetTimeInMilliseconds() - pPed->GetWeaponDamagedTimeByHash(WEAPONTYPE_RUNOVERBYVEHICLE)) < snBraceMaxTimeSinceLastCarHit))
		{
			pNewTask = rage_new CTaskNMHighFall(250, NULL, CTaskNMHighFall::HIGHFALL_FROM_CAR_HIT, NULL, m_bHighVelocityReaction);
		}
		else
		{
			pNewTask = rage_new CTaskNMHighFall(250, NULL, CTaskNMHighFall::HIGHFALL_IN_AIR, NULL, m_bHighVelocityReaction);
		}

		if (pNewTask)
		{
			GetControlTask()->ForceNewSubTask(pNewTask);
			return true;
		}
	}

	return ProcessFinishConditionsBase(pPed, MONITOR_FALL, nFlags, m_bHighVelocityReaction ? &sm_Tunables.m_HighVelocityBlendOut.PickBlendOut(pPed) : NULL);
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMBrace::HandleRagdollImpact(float fMag, const CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId)
//////////////////////////////////////////////////////////////////////////
{
	// Handles any new vehicle impacts
	if (pEntity != NULL && pEntity->GetIsTypeVehicle())
	{
		return true;
	}
	
	return CTaskNMBehaviour::HandleRagdollImpact(fMag, pEntity, vPedNormal, nComponent, nMaterialId);
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMBrace::IsStuckUnderVehicle(CPed* pPed, CVehicle* pVehicle)
//////////////////////////////////////////////////////////////////////////
{

	// Check theres a contact vehicle (and a ped)
	if (!pPed || !pVehicle)
		return false;

	// Check the ped is under the vehicle's centre of mass
	//float pedZ = pPed->GetBonePositionCached(BONETAG_SPINE1).z;
	//Vector3 vehicleCentre(VEC3_ZERO);
	//pVehicle->GetBoundCentre(vehicleCentre);
	//float carZ = vehicleCentre.z;

	//CTaskNMBrace::Tunables::VehicleTypeOverrides* pData = sm_Tunables.m_VehicleOverrides.Get(pVehicle->GetVehicleModelInfo()->GetNmBraceOverrideSet());

	//float fVehCentreZOffset = 0.0f;
	//if (pData && pData->m_OverrideReactionType)
	//{
	//	fVehCentreZOffset = pData->m_VehicleCentreZOffset;
	//}
	//if( pedZ > (carZ+fVehCentreZOffset))
	//	return false;

	if (!pPed->GetPedResetFlag(CPED_RESET_FLAG_VehicleCrushingRagdoll))
		return false;

	// check the ped is roughly horizontal
	float uprightDot = CalculateUprightDot(pPed);
	if (abs(uprightDot)>sm_Tunables.m_StuckUnderVehicleMaxUpright)
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBrace::StartStuckOnVehicle(CPed* pPed, Tunables::StuckOnVehicle& stuckSettings, bool bAddBaseSet)
//////////////////////////////////////////////////////////////////////////
{
	if (pPed->IsPlayer())
	{
		AddTuningSet(&stuckSettings.m_StuckOnVehiclePlayer);
		if (bAddBaseSet)
		{
			AddTuningSet(&sm_Tunables.m_StuckOnVehicle.m_StuckOnVehiclePlayer);
		}
	}
	else
	{
		AddTuningSet(&stuckSettings.m_StuckOnVehicle);
		if (bAddBaseSet)
		{
			AddTuningSet(&sm_Tunables.m_StuckOnVehicle.m_StuckOnVehicle);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBrace::UpdateStuckOnVehicle(CPed* pPed, Tunables::StuckOnVehicle& stuckSettings, bool bAddBaseSet)
//////////////////////////////////////////////////////////////////////////
{
	if (pPed->IsPlayer())
	{
		AddTuningSet(&stuckSettings.m_UpdateOnVehiclePlayer);
		if (bAddBaseSet)
		{
			AddTuningSet(&sm_Tunables.m_StuckOnVehicle.m_UpdateOnVehiclePlayer);
		}
	}
	else
	{
		AddTuningSet(&stuckSettings.m_UpdateOnVehicle);
		if (bAddBaseSet)
		{
			AddTuningSet(&sm_Tunables.m_StuckOnVehicle.m_UpdateOnVehicle);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBrace::EndStuckOnVehicle(CPed* pPed, Tunables::StuckOnVehicle& stuckSettings, bool bAddBaseSet)
//////////////////////////////////////////////////////////////////////////
{
	if (pPed->IsPlayer())
	{
		AddTuningSet(&stuckSettings.m_EndStuckOnVehiclePlayer);
		if (bAddBaseSet)
		{
			AddTuningSet(&sm_Tunables.m_StuckOnVehicle.m_EndStuckOnVehiclePlayer);
		}
	}
	else
	{
		AddTuningSet(&stuckSettings.m_EndStuckOnVehicle);
		if (bAddBaseSet)
		{
			AddTuningSet(&sm_Tunables.m_StuckOnVehicle.m_EndStuckOnVehicle);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBrace::StartStuckUnderVehicle(CPed* pPed, Tunables::StuckOnVehicle& stuckSettings, bool bAddBaseSet)
//////////////////////////////////////////////////////////////////////////
{
	if (pPed->IsPlayer())
	{
		AddTuningSet(&stuckSettings.m_StuckUnderVehiclePlayer);
		if (bAddBaseSet)
		{
			AddTuningSet(&sm_Tunables.m_StuckOnVehicle.m_StuckUnderVehiclePlayer);
		}
	}
	else
	{
		AddTuningSet(&stuckSettings.m_StuckUnderVehicle);
		if (bAddBaseSet)
		{
			AddTuningSet(&sm_Tunables.m_StuckOnVehicle.m_StuckUnderVehicle);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBrace::EndStuckUnderVehicle(CPed* pPed, Tunables::StuckOnVehicle& stuckSettings, bool bAddBaseSet)
//////////////////////////////////////////////////////////////////////////
{
	if (pPed->IsPlayer())
	{
		AddTuningSet(&stuckSettings.m_EndStuckUnderVehiclePlayer);
		if (bAddBaseSet)
		{
			AddTuningSet(&sm_Tunables.m_StuckOnVehicle.m_EndStuckUnderVehiclePlayer);
		}
	}
	else
	{
		AddTuningSet(&stuckSettings.m_EndStuckUnderVehicle);
		if (bAddBaseSet)
		{
			AddTuningSet(&sm_Tunables.m_StuckOnVehicle.m_EndStuckUnderVehicle);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMBrace::ShouldUseUnderVehicleReaction(CPed* pHitPed, CEntity* pBraceEntity)
//////////////////////////////////////////////////////////////////////////
{
	Assert(pHitPed);
	Assert(pBraceEntity);
	if (!pHitPed || !pBraceEntity)
	{
		return false;
	}

	if (!pBraceEntity->GetIsTypeVehicle())
	{
		return false;
	}

	CVehicle* pVeh = static_cast<CVehicle*>(pBraceEntity);
	bool bForceUnderVehicle = sm_Tunables.m_ForceUnderVehicle;
	bool bForceOverVehicle = sm_Tunables.m_ForceOverVehicle;
	float fVehCentreZOffset = 0.0f;

	// first, check for any per vehicle overrides
	CTaskNMBrace::Tunables::VehicleTypeOverrides* pData = sm_Tunables.m_VehicleOverrides.Get(pVeh->GetVehicleModelInfo()->GetNmBraceOverrideSet());

	if (pData && pData->m_OverrideReactionType)
	{
		bForceOverVehicle = pData->m_ForceOverVehicle;
		bForceUnderVehicle = pData->m_ForceUnderVehicle;
		fVehCentreZOffset = pData->m_VehicleCentreZOffset;
	}

	if (bForceUnderVehicle)
	{
		return true;
	}
	else if (bForceOverVehicle)
	{
		return false;
	}
	else
	{
		// If the peds spine is above the centre of mass, go over. Otherwise, under
		Vec3V pedCoords = VECTOR3_TO_VEC3V(pHitPed->GetBonePositionCached(BONETAG_SPINE1));
		// transform the ped coords into the vehicle's coordinate space
		pedCoords = UnTransformOrtho(pBraceEntity->GetMatrix(), pedCoords);

		float pedZ = pedCoords.GetZf();
		float carZ = pBraceEntity->GetBaseModelInfo()->GetBoundingSphereOffset().z;

		if( pedZ > (carZ+fVehCentreZOffset))
		{
			return false;
		}
		else
		{
			return true;
		}
	}
}

void CTaskNMBrace::ApplyInverseMassScales(phContactIterator& impacts, CVehicle* pVeh)
{
	nmAssert(!impacts.AtEnd());
	nmAssert(pVeh && pVeh->GetIsTypeVehicle() && pVeh->GetVehicleModelInfo());

	// first, check for any per vehicle overrides
	CTaskNMBrace::Tunables::VehicleTypeOverrides* pData = sm_Tunables.m_VehicleOverrides.Get(pVeh->GetVehicleModelInfo()->GetNmBraceOverrideSet());
	
	if (pData && pData->m_OverrideInverseMassScales)
	{
		pData->m_InverseMassScales.Apply(&impacts);
	}
	else
	{
		sm_Tunables.m_InverseMassScales.Apply(&impacts);
	}
}

#if !__NO_OUTPUT
void CTaskNMBrace::Debug() const
{
	// coming soon
}
#endif //!__NO_OUTPUT

//////////////////////////////////////////////////////////////////////////
bool CTaskNMBrace::Tunables::ApplyForce::ShouldApply(u32 startTime, bool inContact)
//////////////////////////////////////////////////////////////////////////
{
	if (!m_Apply)
	{
		return false;
	}

	if (m_OnlyInContact && !inContact)
	{
		return false;
	}

	if (m_OnlyNotInContact && inContact)
	{
		return false;
	}

	if ((fwTimer::GetTimeInMilliseconds() - startTime)>m_Duration)
	{
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CTaskNMBrace::Tunables::ApplyForce::GetForceVec(Vec3V_InOut forceVec, Vec3V_In velVec, const CPhysical *pPhys, const CPed* pHitPed)
//////////////////////////////////////////////////////////////////////////
{
	ScalarV mag(V_ONE);
	if (m_ScaleWithVelocity && m_MaxVelThreshold>m_MinVelThreshold)
	{
		ScalarV velMag = Mag(velVec);
		ScalarV t = (velMag - ScalarV(m_MinVelThreshold)) / ScalarV(m_MaxVelThreshold - m_MinVelThreshold);
		t= Clamp(t, ScalarV(V_ZERO), ScalarV(V_ONE));
		velMag = Lerp(t, ScalarV(m_MinVelMag), ScalarV(m_MaxVelMag));
		mag *= velMag;
	}

	if (m_ScaleWithMass && pPhys)
	{		
		mag *= ScalarV(pPhys->GetMass());
	}

	mag *= ScalarV(m_ForceMag*fwTimer::GetTimeStep());

	mag = Clamp(mag, ScalarV(m_MinMag), ScalarV(m_MaxMag));

	if (m_ReduceWithPedVelocity || m_ReduceWithPedAngularVelocity)
	{
		// reduce the force based on the current velocity / angular velocity of the ped
		ScalarV pedMag(V_ZERO);

		if (m_ReduceWithPedVelocity)
		{
			Vec3V pedVel;
			pHitPed->GetRagdollInst()->GetCOMVel(RC_VECTOR3(pedVel));
			pedMag+= Mag(pedVel);
		}
		
		if (m_ReduceWithPedAngularVelocity)
		{
			Vec3V pedAngVel ;
			pHitPed->GetRagdollInst()->GetCOMRotVel(RC_VECTOR3(pedAngVel));
			pedMag+= Mag(pedAngVel);
		}		

		mag /= pedMag;
	}

	forceVec*=mag;

	float resultMagSq = MagSquared(forceVec).Getf();
	if (resultMagSq > 3000.0f*3000.0f)
	{
		Warningf("CTaskNMBrace::Tunables::ApplyForce::GetForceVec() is producing very large impulses. Mag(forceVec) is %f.  Setting the impulse to 1.0.", Mag(forceVec).Getf());
		forceVec = Vec3V(V_ONE);
	}
}


void CTaskNMBrace::Tunables::VehicleTypeTunables::Append(VehicleTypeOverrides& newItem)
{
	m_sets.PushAndGrow(newItem);
}
void CTaskNMBrace::Tunables::VehicleTypeTunables::Revert(VehicleTypeOverrides& newItem)
{
	for(int index=0;index<m_sets.GetCount();index++)
	{
		if(newItem.m_Id == m_sets[index].m_Id)
		{
			m_sets.Delete(index);
			break;
		}
	}
}

CTaskFallOver::eFallDirection CTaskNMBrace::PickCarHitDirection(CPed* pPed)
{
	CVehicle* pVehicle = NULL;

	if (GetBraceEntity() && GetBraceEntity()->GetIsTypeVehicle())
	{
		pVehicle = static_cast<CVehicle*>(GetBraceEntity());
	}

	if (!(pVehicle && pPed))
		return CTaskFallOver::kDirBack;

	u8 nUDLR = 0;
	u8 nForceUD = u8(fwRandom::GetRandomNumber()&3);
	u32 nHitType = WEAPONTYPE_RAMMEDBYVEHICLE;

	CBaseModelInfo *pVehModelInfo = pVehicle->GetBaseModelInfo();

	if (!pVehModelInfo)
		return CTaskFallOver::kDirBack;

	Vector3 vecDelta = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - pVehicle->GetTransform().GetPosition());

	Vector2 dir2d(-pVehicle->GetVelocity().x, -pVehicle->GetVelocity().y);
	s32 dirOffset = pPed->GetLocalDirection(dir2d);

	float fCarRightSize = pVehModelInfo->GetBoundingBoxMax().x;
	float fPedSidePos = DotProduct(vecDelta, VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA()));
	float fPedRelativeHeight = DotProduct(vecDelta, VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetC()));// + pVehicle->m_vecCOM.z;
	if(pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN)
	{
		nUDLR = 2;	// down
		nHitType = WEAPONTYPE_RUNOVERBYVEHICLE;

		// only want to use front and back knockdown anims for running over ped
		if(dirOffset==1 || dirOffset==3)
			dirOffset = 2;
	}
	else if(DotProduct(pVehicle->GetVelocity(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB())) < 0.0f)
	{

	}
	// Collision knocks ped off to the side
	else if(ABS(fPedSidePos) > 0.99f*fCarRightSize)
	{
		if(fPedSidePos>0.0f)
			nUDLR = 4;	// right
		else
			nUDLR = 3;	// left

		// if ped has been sideswiped by car, knock them under
		if( rage::Abs(DotProduct(vecDelta, VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()))) < 0.85f*pVehModelInfo->GetBoundingBoxMax().y)
		{
			nHitType = WEAPONTYPE_RUNOVERBYVEHICLE;

			// only want to use front and back knockdown anims for running over ped
			if(dirOffset==1 || dirOffset==3)	
				dirOffset = 2;
		}

	}
	// Collision pushes ped up and over car (not big vehicles)
	// (if nForceUD==1 force knock down , if nForceUD==0 force knock up, else use relative height)
	else if((nForceUD==0 || (fPedRelativeHeight > 0.1f && nForceUD>1)) && !(pVehicle->pHandling->mFlags &MF_IS_BIG))
	{
		nUDLR = 1;	// up

		// first we need to calculate the height of the vehicle the ped must pass over
		float fVehicleFront = pVehModelInfo->GetBoundingBoxMax().y;
		float fVehicleRear = pVehModelInfo->GetBoundingBoxMin().y;
		float fVehicleTop = pVehModelInfo->GetBoundingBoxMax().z;
		// if vehicle tilted down use height of boundbox at rear of vehicle
		if(pVehicle->GetTransform().GetB().GetZf() < -0.2f){
			fVehicleTop = ( VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) + fVehicleTop*(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetC())) +
				fVehicleRear*(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB())) ).z;
			//				fVehicleTop += 0.3f;
			// now save dist from front of veh to height cross point - longer if using the rear
			fVehicleRear =  -fVehicleRear + fVehicleFront;
		}
		// else if vehicle going up hill
		else if(pVehicle->GetTransform().GetB().GetZf() > 0.1f){
			fVehicleTop = ( VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) + fVehicleTop*(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetC())) +
				fVehicleFront*(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB())) ).z;
			// now save dist from front of veh to height cross point - longer if using the rear
			fVehicleRear =  fVehicleFront;
		}
		// otherwise height of boundbox at vehicle centre
		else{
			fVehicleTop = ( VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) + fVehicleTop*(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetC())) ).z;
			fVehicleRear = pVehModelInfo->GetBoundingBoxMax().y;
		}

		float fRiseTime = fVehicleRear / pVehicle->GetVelocity().Mag();
		float fReqRiseSpeed = (fVehicleTop - pPed->GetTransform().GetPosition().GetZf()) / fRiseTime;
		fReqRiseSpeed = (1.5f+0.002f*(fwRandom::GetRandomNumber()&255))*fReqRiseSpeed;
		vecDelta = pVehicle->GetVelocity();
		vecDelta.Normalize();
		vecDelta = 0.2f*fReqRiseSpeed*vecDelta;
		vecDelta.z += fReqRiseSpeed;
		// reverse hit direction
		if( (dirOffset+=2) > 3 ) dirOffset-=4;

		vecDelta = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition() - pVehicle->GetTransform().GetPosition());
	}
	// collision pushes ped under car
	else
	{
		nUDLR = 2;	// down

		// only want to use front and back knockdown anims for running over ped
		if(dirOffset==1 || dirOffset==3)	
			dirOffset = 2;
	}

	int nRand = int(fwRandom::GetRandomNumber()&3);
	//		if (this->m_nFlags.bNotDamagedByCollisions) return;
	switch(dirOffset){
	case 0:
		if((nUDLR==3 && nRand>1))
			return CTaskFallOver::kDirRight;
		else if((nUDLR==4 && nRand>1))
			return CTaskFallOver::kDirLeft;
		else
			return CTaskFallOver::kDirBack;
		break;
	case 1:
		return CTaskFallOver::kDirRight;
		break;
	case 2:
		if((nUDLR==3 && nRand>1))
			return CTaskFallOver::kDirRight;
		else if((nUDLR==4 && nRand>1))
			return CTaskFallOver::kDirLeft;
		else
			return CTaskFallOver::kDirFront;
		break;
	case 3:
		return CTaskFallOver::kDirLeft;
		break;
	}

	return CTaskFallOver::kDirBack;
}

void CTaskNMBrace::StartAnimatedFallback(CPed* pPed)
{
	fwMvClipSetId clipSetId = CLIP_SET_ID_INVALID;
	fwMvClipId clipId = CLIP_ID_INVALID;

	Vector3 vecHeadPos(0.0f,0.0f,0.0f);
	pPed->GetBonePosition(vecHeadPos, BONETAG_HEAD);
	// If the ped is already prone then use an on-ground damage reaction
	if (vecHeadPos.z < pPed->GetTransform().GetPosition().GetZf())
	{
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
			CTaskFallOver::eFallDirection dir = PickCarHitDirection(pPed);
			CTaskFallOver::eFallContext context = CTaskFallOver::kContextVehicleHit;

			CTaskFallOver::PickFallAnimation(pPed, context, dir, clipSetId, clipId);
		}

		CTaskFallOver* pNewTask = rage_new CTaskFallOver(clipSetId, clipId, 2.0f, fStartPhase);
		pNewTask->SetRunningLocally(true);

		SetNewTask(pNewTask);
	}
}

bool CTaskNMBrace::ControlAnimatedFallback(CPed* pPed)
{
	nmDebugf3("LOCAL: Controlling animated fallback task %s (0x%p) Ped ragdolling : %s\n", GetSubTask() ? GetSubTask()->GetTaskName() : "None", this, pPed->GetUsingRagdoll() ? "true" : "false");

	// disable ped capsule control so the blender can set the velocity directly on the ped
	if (pPed->IsNetworkClone())
	{
		NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleControl, true);
	}

	if (!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
	{
		m_nSuggestedNextTask = CTaskTypes::TASK_FINISHED;
		return false;
	}

	return true;
}