// File header
#include "Task/Movement/Jumping/TaskFallGetUp.h"

#include "fwanimation/pointcloud.h"
#include "fwanimation/directorcomponentragdoll.h"

// Game headers
#include "Event/EventDamage.h"
#include "Ik/solvers/RootSlopeFixupSolver.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Task/General/TaskBasic.h"
#include "Task/Movement/TaskFall.h"
#include "Task/Movement/TaskGetUp.h"
#include "Weapons/Weapon.h"

// Disables optimisations if active
AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS();
AI_MOVEMENT_OPTIMISATIONS()

bank_float CTaskFallOver::ms_fCapsuleRadiusMult = 0.6f;
bank_float CTaskFallOver::ms_fCapsuleRadiusGrowSpeedMult = 0.5f;

//////////////////////////////////////////////////////////////////////////
// CClonedFallOverInfo
//////////////////////////////////////////////////////////////////////////
CClonedFallOverInfo::CClonedFallOverInfo(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fDownTime, float fStartPhase, bool bKnockedIntoAir, bool bIncreasedRate, bool bCapsuleRestrictionsDisabled) 
: m_clipSetId(clipSetId)
, m_clipId(clipId)
, m_fDownTime(fDownTime > 0.0f ? fDownTime : 0.0f)
, m_fStartPhase(fStartPhase)
, m_bKnockedIntoAir(bKnockedIntoAir)
, m_bIncreasedRate(bIncreasedRate)
, m_bCapsuleRestrictionsDisabled(bCapsuleRestrictionsDisabled)
{
}

CClonedFallOverInfo::CClonedFallOverInfo() 
: m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipId(CLIP_ID_INVALID)
, m_fDownTime(0.0f)
, m_fStartPhase(0.0f)
, m_bKnockedIntoAir(false)
, m_bIncreasedRate(false)
, m_bCapsuleRestrictionsDisabled(false)
{
}

bool CClonedFallOverInfo::AutoCreateCloneTask(CPed* pPed)
{
	// if a fall and get up clone task is running, it will create the fall over clone task
	return !pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_FALL_AND_GET_UP);
}

CTaskFSMClone *CClonedFallOverInfo::CreateCloneFSMTask()
{
	CTaskFallOver *pTask = rage_new CTaskFallOver(m_clipSetId, m_clipId, m_fDownTime, m_fStartPhase);
	pTask->SetCapsuleRestrictionsDisabled(m_bCapsuleRestrictionsDisabled);
	return pTask;
}

//////////////////////////////////////////////////////////////////////////
// CTaskFallOver
//////////////////////////////////////////////////////////////////////////

CTaskFallOver::CTaskFallOver(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fDownTime, float fStartPhase, float fRootSlopeFixupPhase)
: m_fDownTimeTimer(fDownTime)
, m_clipSetId(clipSetId)
, m_clipId(clipId)
, m_bKnockedIntoAir(false)
, m_bIncreasedRate(false)
, m_bFinishTask(false)
, m_bCapsuleRestrictionsDisabled(false)
, m_fRootSlopeFixupPhase(fRootSlopeFixupPhase)
, m_fStartPhase(fStartPhase)
, m_fFallTime(0.0f)
{
	SetInternalTaskType(CTaskTypes::TASK_FALL_OVER);
}

CTaskFallOver::CTaskFallOver(const CTaskFallOver& other)
: m_fDownTimeTimer(other.m_fDownTimeTimer)
, m_clipSetId(other.m_clipSetId)
, m_clipId(other.m_clipId)
, m_bKnockedIntoAir(false)
, m_bIncreasedRate(false)
, m_bFinishTask(false)
, m_bCapsuleRestrictionsDisabled(other.m_bCapsuleRestrictionsDisabled)
, m_fRootSlopeFixupPhase(other.m_fRootSlopeFixupPhase)
, m_fStartPhase(other.m_fStartPhase)
, m_fFallTime(other.m_fFallTime)
{
	SetInternalTaskType(CTaskTypes::TASK_FALL_OVER);
}

//////////////////////////////////////////////////////////////////////////
CTaskFallOver::eFallContext CTaskFallOver::ComputeFallContext(u32 weaponHash, u32 hitBoneTag)
{
	if(weaponHash == WEAPONTYPE_FALL)
	{
		return kContextFallImpact;
	}
	else if(weaponHash == WEAPONTYPE_DROWNING)
	{
		return kContextDrown;
	}
	else
	{
		const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(weaponHash);

		if (aiVerifyf(pWeaponInfo, "CTaskFallOver::ComputeFallContext - Unrecognised weapon hash %u", weaponHash))
		{
			if (pWeaponInfo->GetDamageType()==DAMAGE_TYPE_EXPLOSIVE)
			{
				return kContextExplosion;
			}
			else if(pWeaponInfo->GetDamageType()==DAMAGE_TYPE_FIRE)
			{
				return kContextOnFire;
			}
			else if(pWeaponInfo->GetGroup() == WEAPONGROUP_SHOTGUN)
			{
				return kContextShotShotgun;
			}
			else if ( hitBoneTag==BONETAG_HEAD)
			{
				return kContextHeadShot;
			}
			else if ( 
				hitBoneTag==BONETAG_L_THIGH || 
				hitBoneTag==BONETAG_L_CALF ||
				hitBoneTag==BONETAG_L_FOOT ||
				hitBoneTag==BONETAG_R_THIGH ||
				hitBoneTag==BONETAG_R_CALF ||
				hitBoneTag==BONETAG_R_FOOT
				)
			{
				return kContextLegShot;
			}
			else if ( pWeaponInfo->GetGroup()==WEAPONGROUP_SNIPER )
			{
				return kContextShotSniper;
			}
			else if (pWeaponInfo->GetGroup()==WEAPONGROUP_SMG )
			{
				return kContextShotSMG;
			}
			else if (pWeaponInfo->GetGroup()==WEAPONGROUP_MG)
			{
				return kContextShotAutomatic;
			}
			else
			{
				return kContextShotPistol;
			}
		}
	}

	return kContextStandard;
}

bool CTaskFallOver::PickFallAnimation(CPed* pPed, eFallContext context, eFallDirection dir, fwMvClipSetId& clipSetOut, fwMvClipId& clipOut)
{
	switch(context)
	{
		//////////////////////////////////////////////////////////////////////////
		// Standard
	case kContextStandard:
		{
			clipSetOut=pPed->GetPedModelInfo()->GetFullBodyDamageClipSet();
			switch (dir)
			{
			case kDirFront: clipOut = CLIP_KO_FRONT; return true;
			case kDirBack: clipOut = CLIP_KO_BACK; return true;
			case kDirRight: clipOut = CLIP_KO_RIGHT; return true;
			case kDirLeft: clipOut = CLIP_KO_LEFT; return true;
			default: return false;
			}
		}
		break;
		//////////////////////////////////////////////////////////////////////////
		// fall damage
	case kContextFallImpact:
	case kContextOnFire:
		{
			clipSetOut=pPed->GetPedModelInfo()->GetFullBodyDamageClipSet();
			clipOut= CLIP_KO_COLLAPSE;
			return true;
		}
		break;

		//////////////////////////////////////////////////////////////////////////
		// drowning
	case kContextDrown:
		{
			clipSetOut=pPed->GetPedModelInfo()->GetFullBodyDamageClipSet();
			clipOut= CLIP_KO_DROWN;
			return true;
		}
		break;		
		//////////////////////////////////////////////////////////////////////////
		// vehicle hits
	case kContextExplosion:
		{
			switch (dir)
			{
			case kDirFront: clipSetOut = CLIP_SET_KNOCKDOWN_EXPLOSION_FRONT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirBack: clipSetOut = CLIP_SET_KNOCKDOWN_EXPLOSION_BACK; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirRight: clipSetOut = CLIP_SET_KNOCKDOWN_EXPLOSION_RIGHT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirLeft: clipSetOut = CLIP_SET_KNOCKDOWN_EXPLOSION_LEFT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			default: return false;
			}
		}
		break;
		//////////////////////////////////////////////////////////////////////////
		// vehicle hits
	case kContextVehicleHit:
		{
			switch (dir)
			{
			case kDirFront: clipSetOut = CLIP_SET_KNOCKDOWN_VEHICLE_FRONT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirBack: clipSetOut = CLIP_SET_KNOCKDOWN_VEHICLE_BACK; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirRight: clipSetOut = CLIP_SET_KNOCKDOWN_VEHICLE_RIGHT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirLeft: clipSetOut = CLIP_SET_KNOCKDOWN_VEHICLE_LEFT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			default: return false;
			}
		}
		break;
		//////////////////////////////////////////////////////////////////////////
		// shot contexts
	case kContextHeadShot:
		{
			switch (dir)
			{
			case kDirFront: clipSetOut = CLIP_SET_KNOCKDOWN_HEADSHOT_FRONT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirBack: clipSetOut = CLIP_SET_KNOCKDOWN_HEADSHOT_BACK; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirRight: clipSetOut = CLIP_SET_KNOCKDOWN_HEADSHOT_RIGHT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirLeft: clipSetOut = CLIP_SET_KNOCKDOWN_HEADSHOT_LEFT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			default: return false;
			}
		}
		break;
	case kContextShotPistol:
		{
			switch (dir)
			{
			case kDirFront: clipSetOut = CLIP_SET_KNOCKDOWN_PISTOL_FRONT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirBack: clipSetOut = CLIP_SET_KNOCKDOWN_PISTOL_BACK; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirRight: clipSetOut = CLIP_SET_KNOCKDOWN_PISTOL_RIGHT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirLeft: clipSetOut = CLIP_SET_KNOCKDOWN_PISTOL_LEFT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			default: return false;
			}
		}
		break;
	case kContextShotSMG:
		{
			switch (dir)
			{
			case kDirFront: clipSetOut = CLIP_SET_KNOCKDOWN_SMG_FRONT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirBack: clipSetOut = CLIP_SET_KNOCKDOWN_SMG_BACK; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirRight: clipSetOut = CLIP_SET_KNOCKDOWN_SMG_RIGHT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirLeft: clipSetOut = CLIP_SET_KNOCKDOWN_SMG_LEFT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			default: return false;
			}
		}
		break;
	case kContextShotAutomatic:
		{
			switch (dir)
			{
			case kDirFront: clipSetOut = CLIP_SET_KNOCKDOWN_AUTOMATIC_FRONT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirBack: clipSetOut = CLIP_SET_KNOCKDOWN_AUTOMATIC_BACK; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirRight: clipSetOut = CLIP_SET_KNOCKDOWN_AUTOMATIC_RIGHT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirLeft: clipSetOut = CLIP_SET_KNOCKDOWN_AUTOMATIC_LEFT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			default: return false;
			}
		}
		break;
	case kContextShotSniper:
		{
			switch (dir)
			{
			case kDirFront: clipSetOut = CLIP_SET_KNOCKDOWN_SNIPER_FRONT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirBack: clipSetOut = CLIP_SET_KNOCKDOWN_SNIPER_BACK; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirRight: clipSetOut = CLIP_SET_KNOCKDOWN_SNIPER_RIGHT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirLeft: clipSetOut = CLIP_SET_KNOCKDOWN_SNIPER_LEFT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			default: return false;
			}
		}
		break;
	case kContextShotShotgun:
		{
			switch (dir)
			{
			case kDirFront: clipSetOut = CLIP_SET_KNOCKDOWN_SHOTGUN_FRONT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirBack: clipSetOut = CLIP_SET_KNOCKDOWN_SHOTGUN_BACK; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirRight: clipSetOut = CLIP_SET_KNOCKDOWN_SHOTGUN_RIGHT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirLeft: clipSetOut = CLIP_SET_KNOCKDOWN_SHOTGUN_LEFT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			default: return false;
			}
		}
		break;
	case kContextLegShot:
		{
			switch (dir)
			{
			case kDirFront: clipSetOut = CLIP_SET_KNOCKDOWN_LEGSHOT_FRONT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirBack: clipSetOut = CLIP_SET_KNOCKDOWN_LEGSHOT_BACK; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirRight: clipSetOut = CLIP_SET_KNOCKDOWN_LEGSHOT_RIGHT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			case kDirLeft: clipSetOut = CLIP_SET_KNOCKDOWN_LEGSHOT_LEFT; fwClipSetManager::PickRandomClip(clipSetOut, clipOut); return true;
			default: return false;
			}
		}
		break;
	default:
		{
			taskAssertf(0, "Invalid fall anim context!");
			return false;
		}
		break;
	}
}


bool CTaskFallOver::ControlPassingAllowed(CPed* UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	return (GetState() == State_LieOnGround);
}

CTaskInfo *CTaskFallOver::CreateQueriableState() const
{
	return rage_new CClonedFallOverInfo(m_clipSetId, m_clipId, m_fDownTimeTimer, GetClipPhase(), m_bKnockedIntoAir, m_bIncreasedRate, m_bCapsuleRestrictionsDisabled);
}

void CTaskFallOver::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_FALL_OVER );
	CClonedFallOverInfo *fallOverInfo = static_cast<CClonedFallOverInfo*>(pTaskInfo);

	m_clipSetId			= fallOverInfo->GetClipSetId();
	m_clipId			= fallOverInfo->GetClipId();
	m_fDownTimeTimer	= fallOverInfo->GetDownTime();
	m_fStartPhase		= fallOverInfo->GetStartPhase();
	m_bKnockedIntoAir	= fallOverInfo->GetKnockedIntoAir();
	m_bIncreasedRate	= fallOverInfo->GetIncreasedRate();
	m_bCapsuleRestrictionsDisabled = fallOverInfo->GetCapsuleRestrictionsDisabled();

	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

void CTaskFallOver::OnCloneTaskNoLongerRunningOnOwner()
{
	m_bFinishTask = true;
}


CTaskFallOver::FSM_Return CTaskFallOver::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
	FSM_State(State_Fall)
		FSM_OnEnter
			return StateFallOnEnter(pPed);
		FSM_OnUpdate
			return StateFallOnUpdate(pPed);
	FSM_State(State_LieOnGround)
		FSM_OnEnter
			return StateLieOnGroundOnEnter(pPed);
		FSM_OnUpdate
			return StateLieOnGroundOnUpdateClone(pPed);
	FSM_State(State_Quit)
		FSM_OnUpdate
			return FSM_Quit;
	FSM_End
}

CTaskFSMClone *CTaskFallOver::CreateTaskForClonePed(CPed *pPed)
{
	CTaskFallOver *newTask = NULL;

	// if a fall and get up clone task is running, it will create the fall over clone task
	if (!pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_FALL_AND_GET_UP))
	{
		newTask = rage_new CTaskFallOver(m_clipSetId, m_clipId, m_fDownTimeTimer, GetClipPhase(), m_fRootSlopeFixupPhase);
		newTask->SetCapsuleRestrictionsDisabled(m_bCapsuleRestrictionsDisabled);
	}
		
	return newTask;
}

CTaskFSMClone *CTaskFallOver::CreateTaskForLocalPed(CPed *pPed)
{
	// do the same as clone ped
	return CreateTaskForClonePed(pPed);
}

CTaskFallOver::FSM_Return CTaskFallOver::ProcessPreFSM()
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	// Notify as fallen down
	pPed->SetPedResetFlag( CPED_RESET_FLAG_FallenDown, true );

	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostPreRender, true);

	// Set the entities height immediately, to stop prone peds sinking through the ground from high impacts
	if (CTaskFall::GetIsHighFall(pPed, false, m_fFallTime, 0.0f, 0.0f, GetTimeStep()))
	{
		pPed->m_PedResetFlags.SetEntityZFromGround( 50 );
	}

	return FSM_Continue;
}

bool CTaskFallOver::ProcessPostPreRender()
{
	CPed *pPed = GetPed();

	const CRootSlopeFixupIkSolver* pSolver = static_cast<const CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
	const CBaseCapsuleInfo* pCapsuleInfo = pPed->GetCapsuleInfo(); 
	if (pSolver != NULL && pSolver->GetBlendRate() >= 0.0f && pCapsuleInfo != NULL)
	{
		pPed->OrientBoundToSpine();
		pPed->SetTaskCapsuleRadius(pCapsuleInfo->GetHalfWidth() * ms_fCapsuleRadiusMult);
		if (GetState() == State_LieOnGround)
		{
			pPed->OverrideCapsuleRadiusGrowSpeed(0.0f);

			// Always force pitch/offset to the maximum
			pPed->SetBoundPitch(-HALF_PI);
			pPed->SetDesiredBoundPitch(pPed->GetBoundPitch());
			if (pCapsuleInfo->GetBipedCapsuleInfo())
			{
				float fCapsuleRadiusDiff = pCapsuleInfo->GetHalfWidth() - (pCapsuleInfo->GetHalfWidth() * ms_fCapsuleRadiusMult);
				Vector3 vBoundOffset(0.0f, 0.0f, pCapsuleInfo->GetBipedCapsuleInfo()->GetCapsuleZOffset() - (fCapsuleRadiusDiff * 0.5f));
				pPed->SetBoundOffset(vBoundOffset);
				pPed->SetDesiredBoundOffset(pPed->GetBoundOffset());
			}
		}
		else
		{
			if (pCapsuleInfo->GetBipedCapsuleInfo())
			{
				pPed->OverrideCapsuleRadiusGrowSpeed(pCapsuleInfo->GetBipedCapsuleInfo()->GetRadiusGrowSpeed() * ms_fCapsuleRadiusGrowSpeedMult);
			}
		}
	}
	else
	{
		pPed->SetTaskCapsuleRadius(0.0f);
		pPed->OverrideCapsuleRadiusGrowSpeed(0.0f);
		pPed->ClearBound();
	}
	
	return true;
}

CTaskFallOver::FSM_Return CTaskFallOver::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	FSM_Begin
		FSM_State(State_Fall)
			FSM_OnEnter
				return StateFallOnEnter(pPed);
			FSM_OnUpdate
				return StateFallOnUpdate(pPed);
		FSM_State(State_LieOnGround)
			FSM_OnEnter
				return StateLieOnGroundOnEnter(pPed);
			FSM_OnUpdate
				return StateLieOnGroundOnUpdate(pPed);
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void CTaskFallOver::CleanUp()
{
	CPed* pPed = GetPed();

	// Clean up any flags
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir, false );
	
	// Ensure that the slope fixup IK solver is cleaned up!
	CRootSlopeFixupIkSolver* pSolver = static_cast<CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
	if (pSolver != NULL)
	{
		pSolver->SetBlendRate(SLOW_BLEND_OUT_DELTA);
	}

	pPed->SetTaskCapsuleRadius(0.0f);
	pPed->OverrideCapsuleRadiusGrowSpeed(0.0f);
	pPed->ClearBound();
}

bool CTaskFallOver::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if(CTask::ABORT_PRIORITY_IMMEDIATE == iPriority)
	{
		return CTask::ShouldAbort(iPriority, pEvent);
	}
	else if(CTask::ABORT_PRIORITY_URGENT == iPriority)
	{
		if(pEvent == NULL ||
			((CEvent*)pEvent)->RequiresAbortForRagdoll() || 
			(((CEvent*)pEvent)->GetEventType() == EVENT_DAMAGE && pPed->GetHealth() < 1.0f) ||
			((CEvent*)pEvent)->GetEventType() == EVENT_IN_WATER || 
			((CEvent*)pEvent)->GetEventPriority() == E_PRIORITY_SCRIPT_COMMAND_SP ||
			((CEvent*)pEvent)->GetEventType() == EVENT_STUCK_IN_AIR)
		{
			return CTask::ShouldAbort(iPriority, pEvent);
		}
	}

	return false;
}

#if !__FINAL
const char * CTaskFallOver::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Fall",
		"LieOnGround",
		"Quit",
	};

	return aStateNames[iState];
}
#endif // !__FINAL


CTaskFallOver::FSM_Return CTaskFallOver::StateFallOnEnter(CPed* pPed)
{
	// Don't want to knock the player down if they're attached
	if(pPed->GetIsAttached())
	{
		return FSM_Quit;
	}

	// quit if the clone is ragdolling, as it is most likely on the ground anyway. This can happen if we get an update from a ped that is not 
	// ragdolling.
	if (pPed->IsNetworkClone() && pPed->GetUsingRagdoll())
	{
		return FSM_Quit;
	}

	if(StartClipBySetAndClip(pPed, m_clipSetId, m_clipId, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnDelete))
	{
		static dev_float INCREASED_RATE_FROM_VEHICLE_COLLISIONS = 2.0f;

		if (m_bCapsuleRestrictionsDisabled)
		{
			// Free the mover capsule
			if (pPed->IsNetworkClone())
			{
				NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
			}
			pPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePedCapsuleControl, true );
		}

		// If hit by a vehicle and it is still above us, increase the rate
		if (pPed->IsNetworkClone())
		{
			if (m_bIncreasedRate)
			{
				GetClipHelper()->SetRate(INCREASED_RATE_FROM_VEHICLE_COLLISIONS);
			}
		}
		else if(pPed->GetNoCollisionEntity() && ((CPhysical *) pPed->GetNoCollisionEntity())->GetIsTypeVehicle())
		{
			GetClipHelper()->SetRate(INCREASED_RATE_FROM_VEHICLE_COLLISIONS);
			m_bIncreasedRate = true;
		}

		GetClipHelper()->SetPhase(m_fStartPhase);

		if (m_fRootSlopeFixupPhase < 0.0f)
		{
			static const atHashString ms_BlendInRootIkSlopeFixup("BlendInRootIkSlopeFixup",0xDBFC38CA);
			CClipEventTags::FindEventPhase<crPropertyAttributeHashString>(GetClipHelper()->GetClip(), CClipEventTags::MoveEvent, CClipEventTags::MoveEvent, ms_BlendInRootIkSlopeFixup, m_fRootSlopeFixupPhase);
		}

		m_fFallTime = 0.0f;

		return FSM_Continue;
	}
	else
	{
		return FSM_Quit;
	}
}

CTaskFallOver::FSM_Return CTaskFallOver::StateFallOnUpdate(CPed* pPed)
{
	// Match to slope
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);

	if(GetClipHelper())
	{
		float fCurrentPhase = GetClipHelper()->GetPhase();

		if (m_fRootSlopeFixupPhase >= 0.0f && fCurrentPhase >= m_fRootSlopeFixupPhase)
		{
			// If we haven't yet added IK fixup...
			const CRootSlopeFixupIkSolver* pSolver = static_cast<const CRootSlopeFixupIkSolver*>(pPed->GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeRootSlopeFixup));
			if (pSolver == NULL || pSolver->GetBlendRate() <= 0.0f)
			{
				// Start aligning the ped to the slope...
				pPed->GetIkManager().AddRootSlopeFixup(SLOW_BLEND_IN_DELTA);
			}
		}

		if (m_bCapsuleRestrictionsDisabled)
		{
			static float waitTime = 0.7f; 
			float downVel = pPed->GetVelocity().z;

			if (GetTimeInState() > waitTime)
			{
				// Disable impacts with vehicles
				pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableVehicleImpacts, true );
				
				const float fDownVelThreshold = NetworkInterface::IsGameInProgress() ? -0.001f : 0.0f;

				AI_LOG_WITH_ARGS("StateFallOnUpdate - Ped: [%s], DownVel: %f \n", AILogging::GetDynamicEntityNameSafe(pPed), downVel);

				if(downVel >= fDownVelThreshold || (pPed->GetIsSwimming() && fCurrentPhase == 1.0f))
				{
					SetState(State_LieOnGround);
				}
				else
				{
					// Free the mover capsule
					// disable ped capsule control so the blender can set the velocity directly on the ped
					if (pPed->IsNetworkClone())
					{
						NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
					}
					pPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePedCapsuleControl, true );
				}
			}
			else
			{
				// Free the mover capsule
				if (pPed->IsNetworkClone())
				{
					NetworkInterface::UseAnimatedRagdollFallbackBlending(*pPed);
				}
				pPed->SetPedResetFlag( CPED_RESET_FLAG_DisablePedCapsuleControl, true );
			}
		}
		else
		{
			if (fCurrentPhase == 1.0f)
			{
				SetState(State_LieOnGround);
			}
		}
		return FSM_Continue;
	}
	else
	{
		return FSM_Quit;
	}
}

CTaskFallOver::FSM_Return CTaskFallOver::StateLieOnGroundOnEnter(CPed* pPed)
{
	if( StartClipBySetAndClip(pPed, m_clipSetId, m_clipId, NORMAL_BLEND_IN_DELTA, CClipHelper::TerminationType_OnDelete) )
	{
		taskFatalAssertf(GetClipHelper(), "Clip pointer invalid!");
		GetClipHelper()->SetPhase(1.0f);
	}

	// Disabling vehicle impacts when on the ground might actually always be desirable - not just in the 'm_bCapsuleRestrictionsDisabled' case
	if (m_bCapsuleRestrictionsDisabled)
	{
		// Disable impacts with vehicles
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableVehicleImpacts, true );
	}

	// Vary the length of the downtime, so all reacting peds are not synchronised
	static dev_float MIN_MOD = 0.9f;
	static dev_float MAX_MOD = 1.1f;
	m_fDownTimeTimer = fwRandom::GetRandomNumberInRange(m_fDownTimeTimer * MIN_MOD, m_fDownTimeTimer * MAX_MOD);

	static dev_float MAX_PLAYER_DOWNTIME = 0.5f;
	if(pPed->IsLocalPlayer() && 
		m_fDownTimeTimer > MAX_PLAYER_DOWNTIME && 
		!pPed->GetIsArrested() && 
		!pPed->GetPlayerInfo()->AreControlsDisabled())
	{
		m_fDownTimeTimer = MAX_PLAYER_DOWNTIME;
	}

	return FSM_Continue;
}

CTaskFallOver::FSM_Return CTaskFallOver::StateLieOnGroundOnUpdate(CPed* pPed)
{
	if( GetClipHelper() == NULL )
	{
		SetState(State_Quit);
		return FSM_Continue;
	}

	// Disabling vehicle impacts when on the ground might actually always be desirable - not just in the 'm_bCapsuleRestrictionsDisabled' case
	if (m_bCapsuleRestrictionsDisabled)
	{
		// Disable impacts with vehicles
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableVehicleImpacts, true );
	}

	// Match to slope
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);

	if(m_fDownTimeTimer <= 0.0f)
	{
		return FSM_Quit;
	}

	// Tick down timer
	m_fDownTimeTimer -= fwTimer::GetTimeStep();

	return FSM_Continue;
}

CTaskFallOver::FSM_Return CTaskFallOver::StateLieOnGroundOnUpdateClone(CPed* pPed)
{
	// Disabling vehicle impacts when on the ground might actually always be desirable - not just in the 'm_bCapsuleRestrictionsDisabled' case
	if (m_bCapsuleRestrictionsDisabled)
	{
		// Disable impacts with vehicles
		pPed->SetPedResetFlag( CPED_RESET_FLAG_DisableVehicleImpacts, true );
	}

	// Match to slope
	pPed->GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);

	if (m_bFinishTask)
	{
		// Finished
		return FSM_Quit;
	}

	return FSM_Continue;
}

void CTaskFallOver::ApplyInitialVehicleImpact(CVehicle* pVehicle, CPed* pPed)
{
	// Apply an upward impulse relative to the car velocity
	if (pPed->GetCollider() && pVehicle)
	{
		// Freeze the current collider veloicty to avoid unwanted artifacts (like spin around the yaw axis)
		pPed->GetCollider()->Freeze();

		// I believe the logic below was tuned with human peds in mind.  Peds with less mass end up receiving too much force.
		// With that in mind we adjust the amount of force based on the mass of the ped using the human mass of roughly 100kg as a baseline
		ScalarV massRatio(V_ONE);
		static const atHashString sPedCapsule("STANDARD_MALE", 0x6EE4E672);
		const CBaseCapsuleInfo* pCapsuleInfo = CPedCapsuleInfoManager::GetInfo(sPedCapsule.GetHash());
		if (aiVerifyf(pCapsuleInfo != NULL, "CTaskFallOver::ApplyInitialVehicleImpact: Capsule info %s is invalid", sPedCapsule.GetCStr()) &&
			pCapsuleInfo->GetMass() > 0.0f && pPed->GetMass() < pCapsuleInfo->GetMass())
		{
			massRatio = ScalarVFromF32(pPed->GetMass() / pCapsuleInfo->GetMass());
		}

		// Front velocity
		static float maxVehSpeedConsidered = 20.0f;
		static float minForwardImpulse = 100.0f;
		static float maxForwardImpulse = 2000.0f;
		Vec3V vecApplyForce = RCC_VEC3V(pVehicle->GetVelocity());
		ScalarV vehVelMag = RangeClamp(Mag(RCC_VEC3V(pVehicle->GetVelocity())), ScalarV(V_ZERO), ScalarVFromF32(maxVehSpeedConsidered));
		ScalarV vecImpulse = Scale(Lerp(vehVelMag, ScalarVFromF32(minForwardImpulse), ScalarVFromF32(maxForwardImpulse)), massRatio);
		vecApplyForce = NormalizeSafe(vecApplyForce, Vec3V(V_ZERO));
		vecApplyForce = Scale(vecApplyForce, vecImpulse);

		// Up velocity
		static float minUpImpulse = 100.0f;
		static float maxUpImpulse = 200.0f;
		vecImpulse = Scale(Lerp(vehVelMag, ScalarVFromF32(minUpImpulse), ScalarVFromF32(maxUpImpulse)), massRatio);
		vecApplyForce.SetZ(vecImpulse);
		pPed->GetCollider()->ApplyImpulseCenterOfMassRigid(vecApplyForce.GetIntrin128ConstRef());

		// Side velocity
		static float minSideImpulse = 100.0f;
		static float maxSideImpulse = 250.0f;
		vecImpulse = Scale(Lerp(vehVelMag, ScalarVFromF32(minSideImpulse), ScalarVFromF32(maxSideImpulse)), massRatio);
		vecApplyForce = Scale(pVehicle->GetTransform().GetRight(), vecImpulse);
		Vec3V vCarToPed = Subtract(pPed->GetTransform().GetPosition(), pVehicle->GetTransform().GetPosition());
		ScalarV vDot = Dot(vCarToPed, pVehicle->GetTransform().GetRight());
		vecApplyForce = SelectFT(IsGreaterThan(vDot, ScalarV(V_ZERO)), -vecApplyForce, vecApplyForce);
		pPed->GetCollider()->ApplyImpulseCenterOfMassRigid(vecApplyForce.GetIntrin128ConstRef());
	}
}

//////////////////////////////////////////////////////////////////////////
// CClonedFallAndGetUpInfo
//////////////////////////////////////////////////////////////////////////
CTaskFSMClone *CClonedFallAndGetUpInfo::CreateCloneFSMTask()
{
	// clip and down time will be got from CClonedFallOverInfo
	return rage_new CTaskFallAndGetUp(CLIP_SET_ID_INVALID, CLIP_ID_INVALID, 0.0f);
}

//////////////////////////////////////////////////////////////////////////
// CTaskFallAndGetUp
//////////////////////////////////////////////////////////////////////////

CTaskFallAndGetUp::CTaskFallAndGetUp(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fDownTime, float fStartPhase)
: m_clipSetId(clipSetId)
, m_clipId(clipId)
, m_fDownTime(fDownTime)
, m_fStartPhase(fStartPhase)
, m_iFallDirection(-1)
, m_bCapsuleRestrictionsDisabled(false)
{
	SetInternalTaskType(CTaskTypes::TASK_FALL_AND_GET_UP);
}

CTaskFallAndGetUp::CTaskFallAndGetUp(s32 iDirection, float fDownTime, float fStartPhase)
: m_fDownTime(fDownTime)
, m_fStartPhase(fStartPhase)
, m_iFallDirection(iDirection)
, m_bCapsuleRestrictionsDisabled(false)
{
	SetInternalTaskType(CTaskTypes::TASK_FALL_AND_GET_UP);
}

CTaskFallAndGetUp::CTaskFallAndGetUp(const CTaskFallAndGetUp& other)
: m_clipSetId(other.m_clipSetId)
, m_clipId(other.m_clipId)
, m_fDownTime(other.m_fDownTime)
, m_fStartPhase(other.m_fStartPhase)
, m_iFallDirection(other.m_iFallDirection)
, m_bCapsuleRestrictionsDisabled(other.m_bCapsuleRestrictionsDisabled)
{
	SetInternalTaskType(CTaskTypes::TASK_FALL_AND_GET_UP);
}

CTaskInfo *CTaskFallAndGetUp::CreateQueriableState() const
{
	return rage_new CClonedFallAndGetUpInfo();
}

float CTaskFallAndGetUp::GetClipPhase()
{
	switch(GetState())
	{
	case State_Fall:
		{
			CTaskFallOver* pSubTask = static_cast<CTaskFallOver*>(FindSubTaskOfType(CTaskTypes::TASK_FALL_OVER));
			return pSubTask ? Max(0.0f, pSubTask->GetClipPhase()) : m_fStartPhase;
		}
	case State_GetUp:
		return 1.0f;
	default:
		return 0.0f;
	}
}

CTaskFallAndGetUp::FSM_Return CTaskFallAndGetUp::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed *pPed = GetPed(); //Get the ped ptr.

	if (IsRunningLocally())
		return UpdateFSM(iState, iEvent);

	FSM_Begin
		FSM_State(State_Fall)
			FSM_OnEnter	
				if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_FALL_OVER))
				{
					CTaskInfo* pInfo = pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_FALL_OVER, PED_TASK_PRIORITY_MAX);

					// there should be a corresponding fall task info 
					if (AssertVerify(pInfo))
					{
						CTask *pFallTask = pPed->GetPedIntelligence()->CreateCloneTaskFromInfo(pInfo);
						SetNewTask(pFallTask);
					}
				}
				// if the ped is parachuting, it means we've hit the deck, want to set off a TaskGetUp....
				else if(!pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE))
				{
					// the ped is already getting up so just quit the task
					return FSM_Quit;
				}
			FSM_OnUpdate
				if(!GetSubTask() || GetIsFlagSet(aiTaskFlags::SubTaskFinished))
				{
					SetState(State_GetUp);
				}
				else if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_GET_UP))
				{
					if (GetSubTask())
						GetSubTask()->MakeAbortable( CTask::ABORT_PRIORITY_IMMEDIATE, 0);

					SetState(State_GetUp);
				}
		FSM_State(State_GetUp)
			FSM_OnEnter
				CTaskGetUp* getupTask = rage_new CTaskGetUp();
				getupTask->SetLocallyCreated();
				SetNewTask(getupTask);
			FSM_OnUpdate
				if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
				{
					// Finished
					return FSM_Quit;
				}
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

CTaskFSMClone*	CTaskFallAndGetUp::CreateTaskForClonePed(CPed* pPed)
{
    CClonedFallOverInfo *pInfo = 0;

    if (pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_FALL_OVER))
    {
	    pInfo = static_cast<CClonedFallOverInfo*>(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_FALL_OVER, PED_TASK_PRIORITY_MAX));
    }

    CTaskFallAndGetUp *newTask = 0;

	if (pInfo)
	{
		float fStartPhase = pInfo->GetStartPhase();
		// If the clip matches then use the current local clip phase to just have the fall continue where it left off
		if (m_clipSetId == pInfo->GetClipSetId() && m_clipId == pInfo->GetClipId())
		{
			fStartPhase = GetClipPhase();
		}
		newTask = rage_new CTaskFallAndGetUp(pInfo->GetClipSetId(), pInfo->GetClipId(), pInfo->GetDownTime(), fStartPhase);
	}
	else
	{
		// clip and down time will be got from CClonedFallOverInfo
		newTask = rage_new CTaskFallAndGetUp(CLIP_SET_ID_INVALID, CLIP_ID_INVALID, 0.0f);

		newTask->SetState(State_GetUp);
	}

	return newTask;
}

CTaskFSMClone*	CTaskFallAndGetUp::CreateTaskForLocalPed(CPed* pPed)
{
	// do the same thing as clone ped
	return CreateTaskForClonePed(pPed);
}

void CTaskFallAndGetUp::GetClipSetIdAndClipIdFromFallDirection(s32 iDirection, fwMvClipSetId& clipSetId, fwMvClipId& clipId)
{
	CTaskFallOver::eFallDirection dir;
	switch(iDirection)
	{
	case CEntityBoundAI::FRONT:
		dir = CTaskFallOver::kDirFront;
		break;

	case CEntityBoundAI::REAR:
		dir = CTaskFallOver::kDirBack;
		break;

	case CEntityBoundAI::LEFT:
		dir = CTaskFallOver::kDirLeft;
		break;

	case CEntityBoundAI::RIGHT:
		dir = CTaskFallOver::kDirRight;
		break;                    

	default:
		dir = CTaskFallOver::kDirFront;
		break;
	}

	CTaskFallOver::PickFallAnimation(GetPed(), CTaskFallOver::kContextStandard, dir, clipSetId, clipId);
}

CTaskFallAndGetUp::FSM_Return CTaskFallAndGetUp::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Fall)
			FSM_OnEnter
				// Set the animation set id and animation id using GetClipSetIdAndClipIdFromFallDirection
				if (m_iFallDirection>=0)
				{
					GetClipSetIdAndClipIdFromFallDirection(m_iFallDirection, m_clipSetId, m_clipId);
				}

				CTaskFallOver* pFallTask = rage_new CTaskFallOver(m_clipSetId, m_clipId, m_fDownTime, m_fStartPhase);
				pFallTask->SetCapsuleRestrictionsDisabled(m_bCapsuleRestrictionsDisabled);
				pFallTask->SetRunningLocally(IsRunningLocally());

				SetNewTask(pFallTask);

			FSM_OnUpdate
				if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
				{
					SetState(State_GetUp);
				}
		FSM_State(State_GetUp)
			FSM_OnEnter
			{
				CTaskGetUp* pTask = rage_new CTaskGetUp();
				pTask->SetDisableVehicleImpacts(GetPed()->GetPedResetFlag(CPED_RESET_FLAG_DisableVehicleImpacts));
				SetNewTask(pTask);
			}
			FSM_OnUpdate
				if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
				{
					// Finished
					return FSM_Quit;
				}
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskFallAndGetUp::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"Fall",
		"GetUp",
		"Quit",
	};

	return aStateNames[iState];
}
#endif // !__FINAL

//////////////////////////////////////////////////////////////////////////
// CTaskGetUpAndStandStill
//////////////////////////////////////////////////////////////////////////

CTaskGetUpAndStandStill::CTaskGetUpAndStandStill()
{
	SetInternalTaskType(CTaskTypes::TASK_GET_UP_AND_STAND_STILL);
}

CTaskGetUpAndStandStill::FSM_Return CTaskGetUpAndStandStill::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_GetUp)
			FSM_OnEnter
				SetNewTask(rage_new CTaskGetUp());
			FSM_OnUpdate
				if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
				{
					SetState(State_StandStill);
				}
		FSM_State(State_StandStill)
			FSM_OnEnter
				SetNewTask(rage_new CTaskDoNothing(0));
			FSM_OnUpdate
				if(GetIsFlagSet(aiTaskFlags::SubTaskFinished))
				{
					// Finished
					return FSM_Quit;
				}
		FSM_State(State_Quit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

#if !__FINAL
const char * CTaskGetUpAndStandStill::GetStaticStateName( s32 iState )
{
	static const char* aStateNames[] = 
	{
		"GetUp",
		"StandStill",
		"Quit",
	};

	return aStateNames[iState];
}
#endif // !__FINAL
