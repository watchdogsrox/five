// Framework headers
#include "ai/task/taskchannel.h"			// For channel asserts and verifies
#include "math/angmath.h"

// Game headers
#include "Animation/moveped.h"
#include "Game/ModelIndices.h"
#include "Task/Combat/TaskReact.h"
#include "Event/EventReactions.h"			// For adding new reaction events
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"			// For accessing pedintelligence
#include "Peds/PedWeapons/PedTargeting.h"	// For LOS targetting of ped targer
#include "Peds/Rendering/PedVariationPack.h"
#include "streaming/streaming.h"			// For CStreaming::HasObjectLoaded(), etc.
#include "Task/Combat/TaskThreatResponse.h"
#include "Task/Default/TaskChat.h"
#include "Task/Default/Patrol/TaskPatrol.h"
#include "Task/Weapons/Gun/TaskGun.h"
#include "Task/Weapons/WeaponController.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Task/General/TaskBasic.h"
#include "Task/Motion/Locomotion/TaskMotionPed.h"
#include "Task/Motion/TaskMotionBase.h"
#include "Task/Movement/Jumping/TaskFallGetUp.h"
#include "Task/Movement/TaskCollisionResponse.h"

#if __BANK
#include "camera/CamInterface.h"
#include "Peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedFactory.h"
#include "script/script.h"
#include "fwsys/timer.h"
#include "Scene/World/GameWorld.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Movement/TaskNavMesh.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "Physics/Physics.h"
#include "debug/DebugScene.h"
#endif // __BANK

AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CTaskReactAimWeapon
//////////////////////////////////////////////////////////////////////////

fwMvClipSetVarId CTaskReactAimWeapon::sm_AdditionalAnimsClipSet("AimClipSet",0xF15A6053);
fwMvRequestId CTaskReactAimWeapon::ms_ReactBlendRequest("ReactBlend",0x59F3BE88);
fwMvRequestId CTaskReactAimWeapon::ms_AimRequest("Aim",0xB01E2F36);
fwMvBooleanId CTaskReactAimWeapon::ms_OnEnterReactBlend("OnEnterReactBlend",0x63124675);
fwMvBooleanId CTaskReactAimWeapon::ms_ReactBlendFinished("ReactBlendFinished",0xBE6492F4);
fwMvFloatId	CTaskReactAimWeapon::ms_ReactBlendDirection("ReactBlendDirection",0xD489B765);
fwMvFloatId	CTaskReactAimWeapon::ms_PitchId("Pitch",0x3F4BB8CC);
fwMvFloatId	CTaskReactAimWeapon::ms_ReactRateId("ReactRate",0xCF3F86FD);
fwMvFlagId	CTaskReactAimWeapon::ms_HasSixDirectionsId("HasSixDirections",0x5F9E7626);

CTaskReactAimWeapon::Tunables CTaskReactAimWeapon::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskReactAimWeapon, 0x24f8cd8e);

//--------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------

CTaskFSMClone* CClonedReactAimWeaponInfo::CreateCloneFSMTask(void)
{
	CAITarget target; 
	m_weaponTargetHelper.UpdateTarget(target);

	return rage_new CTaskReactAimWeapon(target, m_flags);
}

//--------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------

CTaskReactAimWeapon::CTaskReactAimWeapon(const CPed* pTarget, fwFlags8 uConfigFlags)
: m_target(pTarget)
, m_fReactBlendCurrentHeading(0.0f)
, m_fReactBlendReferenceHeading(0.0f)
, m_fCurrentPitch(-1.0f)
, m_BaseClipSetId(CLIP_SET_ID_INVALID)
, m_WeaponClipSetId(CLIP_SET_ID_INVALID)
, m_fRate(1.0f)
, m_uConfigFlags(uConfigFlags)
, m_bHasSixDirections(false)
, m_bHasCreateWeaponTag(false)
, m_bHasInterruptTag(false)
, m_bUsingOverrideClipSet(false)
, m_bNoLongerRunningOnOwner(false)
{
	SetInternalTaskType(CTaskTypes::TASK_REACT_AIM_WEAPON);
}

CTaskReactAimWeapon::CTaskReactAimWeapon(const CWeaponTarget& target, fwFlags8 uConfigFlags)
: m_target(target)
, m_fReactBlendCurrentHeading(0.0f)
, m_fReactBlendReferenceHeading(0.0f)
, m_fCurrentPitch(-1.0f)
, m_BaseClipSetId(CLIP_SET_ID_INVALID)
, m_WeaponClipSetId(CLIP_SET_ID_INVALID)
, m_fRate(1.0f)
, m_uConfigFlags(uConfigFlags)
, m_bHasSixDirections(false)
, m_bHasCreateWeaponTag(false)
, m_bHasInterruptTag(false)
, m_bNoLongerRunningOnOwner(false)
{
	SetInternalTaskType(CTaskTypes::TASK_REACT_AIM_WEAPON);
}

CTaskReactAimWeapon::~CTaskReactAimWeapon()
{
}

void CTaskReactAimWeapon::CleanUp()
{
	CPed* pPed = GetPed();
	if(pPed && m_moveNetworkHelper.IsNetworkActive())
	{
		TUNE_GROUP_FLOAT(AIM_REACT_TUNE, BLEND_OUT_DURATION, NORMAL_BLEND_DURATION, 0.0f, 1.0f, 0.01f);
		pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, BLEND_OUT_DURATION);
	}

	if (pPed->IsNetworkClone() && GetState() == State_PlayReaction)
	{
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_AimWeaponReactionRunning, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_SkipAimingIdleIntro, true);
	}
}

CTask::FSM_Return CTaskReactAimWeapon::ProcessPreFSM()
{
	if(!m_target.GetIsValid())
	{
		return FSM_Quit;
	}

	if(GetPed()->GetIsInVehicle())
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactAimWeapon::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_StreamAnims)
			FSM_OnUpdate
				return StreamAnims_OnUpdate();

		FSM_State(State_PlayReaction)
			FSM_OnEnter
				return PlayReaction_OnEnter();
			FSM_OnUpdate
				return PlayReaction_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

bool CTaskReactAimWeapon::ProcessMoveSignals()
{
	//Check the state.
	switch(GetState())
	{
		case State_PlayReaction:
		{
			return PlayReaction_OnProcessMoveSignals();
		}
		default:
		{
			break;
		}
	}

	return false;
}

void CTaskReactAimWeapon::Start_OnEnter()
{
	//Ensure we have a gun equipped.
	bool bIsGun2Handed = false;
	const CWeaponInfo* pEquippedGunInfo = GetEquippedGunInfo(bIsGun2Handed);
	if(!pEquippedGunInfo)
	{
		return;
	}

	//Choose the ability.
	Tunables::Ability* pAbility = (GetPed()->GetPedIntelligence()->GetCombatBehaviour().GetCombatAbility() == CCombatData::CA_Professional) ?
		&sm_Tunables.m_Professional : &sm_Tunables.m_NotProfessional;

	//Choose the situation.
	Tunables::Ability::Situation* pSituation = &pAbility->m_None;
	if(!GetPed()->GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_DisableEntryReactions))
	{
		if(m_uConfigFlags.IsFlagSet(CF_Flinch))
		{
			pSituation = &pAbility->m_Flinch;
		}
		else if(m_uConfigFlags.IsFlagSet(CF_Surprised))
		{
			pSituation = &pAbility->m_Surprised;
		}
		else if(m_uConfigFlags.IsFlagSet(CF_Sniper))
		{
			pSituation = &pAbility->m_Sniper;
		}
	}

	//Choose the weapon.
	Tunables::Ability::Situation::Weapon* pWeapon = NULL;
	if(!bIsGun2Handed)
	{
		if(pEquippedGunInfo->GetGroup() == WEAPONGROUP_SMG)
		{
			pWeapon = &pSituation->m_MicroSMG;
		}
		else
		{
			pWeapon = &pSituation->m_Pistol;
		}
	}
	else
	{
		pWeapon = &pSituation->m_Rifle;
	}

	float fRate = 1.0f;

	fwMvClipSetId overrideReactionClipSet = pEquippedGunInfo->GetCombatReactionOverride(*GetPed());
	if(overrideReactionClipSet != CLIP_SET_ID_INVALID)
	{
		m_BaseClipSetId = fwMvClipSetId(overrideReactionClipSet);
		m_bHasSixDirections = false;
		m_bHasCreateWeaponTag = true;
		m_bHasInterruptTag = true;
		m_bUsingOverrideClipSet = true;
	}
	else
	{
		m_BaseClipSetId = fwMvClipSetId(pWeapon->m_ClipSetId.GetHash());
	
		//Check if the clip has six directions.
		m_bHasSixDirections = pWeapon->m_HasSixDirections;

		//Calculate the rate.
		fRate = sm_Tunables.m_Rate * pWeapon->m_Rate;

		//Check if the clip has a create weapon tag.
		m_bHasCreateWeaponTag = pWeapon->m_HasCreateWeaponTag;

		//Check if the clip has an interrupt tag.
		m_bHasInterruptTag = pWeapon->m_HasInterruptTag;
	}


	if(!m_bHasCreateWeaponTag)
	{
		//Create the weapon.
		CreateWeapon();
	}

	//Apply the rate variance.
	float fMaxRateVariance = sm_Tunables.m_MaxRateVariance;
	float fMinRateMultiplier = 1.0f - fMaxRateVariance;
	float fMaxRateMultiplier = 1.0f + fMaxRateVariance;
	float fRateMultiplier = fwRandom::GetRandomNumberInRange(fMinRateMultiplier, fMaxRateMultiplier);
	fRate *= fRateMultiplier;

	//Set the rate.
	m_fRate = fRate;

	//If we can see our target then play the draw_gun line
	if(m_target.GetEntity())
	{
		CPedTargetting* pPedTargetting = GetPed()->GetPedIntelligence()->GetTargetting( false );
		if(pPedTargetting)
		{
			LosStatus losStatus = pPedTargetting->GetLosStatus(m_target.GetEntity(), false);
			if(losStatus == Los_clear || losStatus == Los_blockedByFriendly)
			{
				GetPed()->NewSay("DRAW_GUN");
			}
		}
	}
}

CTask::FSM_Return CTaskReactAimWeapon::Start_OnUpdate()
{
	// If for some reason we didn't get a valid clip set then quit the task
	if(m_BaseClipSetId == CLIP_SET_ID_INVALID)
	{
		return FSM_Quit;
	}

	// Find our weapon clip set and check if it is loaded
	const CWeaponInfo* pWeaponInfo = GetPed()->GetWeaponManager()->GetEquippedWeaponInfo();
	if(!pWeaponInfo)
	{
		return FSM_Quit;
	}

	// If our weapon doesn't have a valid clip set then we need to return
	m_WeaponClipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(GetPed());
	if(m_WeaponClipSetId == CLIP_SET_ID_INVALID)
	{
		return FSM_Quit;
	}

	SetState(State_StreamAnims);

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactAimWeapon::StreamAnims_OnUpdate()
{
	Assert(m_BaseClipSetId   != CLIP_SET_ID_INVALID);
	Assert(m_WeaponClipSetId != CLIP_SET_ID_INVALID);

	if(GetTimeInState() >= 0.5f)
	{
		return FSM_Quit;
	}

	// Check if our base clip set Id is loaded
	bool bBaseClipSetLoaded		= m_clipRequestHelper.Request(m_BaseClipSetId);
	bool bWeaponClipSetLoaded	= m_weaponClipRequestHelper.Request(m_WeaponClipSetId);;

	// Don't play a reaction unless both clip sets are loaded
	if(bBaseClipSetLoaded && bWeaponClipSetLoaded)
	{
		SetState(State_PlayReaction);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactAimWeapon::PlayReaction_OnEnter()
{
	CPed* pPed = GetPed();

	if (pPed->IsNetworkClone())
	{
		CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
		if (pWeapon && !pWeapon->GetIsVisible())
		{
			pWeapon->SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true, true);
		}
	}

	if (!m_moveNetworkHelper.IsNetworkActive())
	{
		taskAssertf(fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskReactAimWeapon), "React aim weapon network is not streamed in!");

		m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskReactAimWeapon);

		TUNE_GROUP_FLOAT(AIM_REACT_TUNE, BLEND_IN_DURATION, NORMAL_BLEND_DURATION, 0.0f, 1.0f, 0.01f);
		pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper.GetNetworkPlayer(), BLEND_IN_DURATION);
	}

	//Set the directions flag.
	m_moveNetworkHelper.SetFlag(m_bHasSixDirections, ms_HasSixDirectionsId);

	// Transition to the correct state
	m_moveNetworkHelper.SendRequest(ms_ReactBlendRequest);
	m_moveNetworkHelper.WaitForTargetState(ms_OnEnterReactBlend);

	// Set the correct anim set to use for the get up blend
	m_moveNetworkHelper.SetClipSet(m_BaseClipSetId);
	
	//Set the rate.
	m_moveNetworkHelper.SetFloat(ms_ReactRateId, m_fRate);

	// Set the aim clip set based on our weapon
	const CWeaponInfo* pWeaponInfo = GetPed()->GetWeaponManager()->GetEquippedWeaponInfo();
	if(pWeaponInfo && m_WeaponClipSetId != CLIP_SET_ID_INVALID)
	{
		m_moveNetworkHelper.SetClipSet(m_WeaponClipSetId, sm_AdditionalAnimsClipSet);
	}
	else
	{
		return FSM_Quit;
	}

	// Set our initial react blend heading
	m_fReactBlendReferenceHeading = pPed->GetCurrentHeading();

	m_fReactBlendCurrentHeading = CalculateDirection();

	RequestProcessMoveSignalCalls();
	m_bMoveCreateWeapon			= false;
	m_bMoveInterrupt			= false;
	m_bMoveReactBlendFinished	= false;

	return FSM_Continue;
}

CTask::FSM_Return CTaskReactAimWeapon::PlayReaction_OnUpdate()
{
	CPed* pPed = GetPed();

	pPed->GetMotionData()->SetExtraHeadingChangeThisFrame(0.0f);

	float fTemp = CalculateDirection();

	// Don't wrap
	float fDiff = m_fReactBlendCurrentHeading - fTemp;
	if(Abs(fDiff) > 0.5f)
	{
		if(fDiff < 0.0f)
		{
			fTemp = 0.0f;
		}
		else
		{
			fTemp = 1.0f;
		}
	}

	float fCurrentReactBlend = m_fReactBlendCurrentHeading;
	CTaskMotionBase::InterpValue(fCurrentReactBlend, fTemp, 1.5f);

	// Ensure we don't flip between -0 / 0 anims
	fCurrentReactBlend = m_fReactBlendCurrentHeading <= 0.5f ? rage::Clamp(fCurrentReactBlend, 0.0f, 0.5f) : rage::Clamp(fCurrentReactBlend, 0.500001f, 1.0f);
	m_fReactBlendCurrentHeading = fCurrentReactBlend;

	m_moveNetworkHelper.SetFloat(ms_ReactBlendDirection, m_fReactBlendCurrentHeading);

	// Calculate the pitch we want to be aiming at
	const CWeaponInfo* pWeaponInfo = GetPed()->GetWeaponManager()->GetEquippedWeaponInfo();
	if (pWeaponInfo)
	{
		const CAimingInfo* pAimingInfo = pWeaponInfo->GetAimingInfo();
		if(!pAimingInfo)
		{
			return FSM_Quit;
		}

		// Get the sweep ranges from the weapon info
		const float fSweepPitchMin = pAimingInfo->GetSweepPitchMin() * DtoR;
		const float fSweepPitchMax = pAimingInfo->GetSweepPitchMax() * DtoR;

		// Calculate the aim vector (this determines the heading and pitch angles to point the clips in the correct direction)
		Vector3 vStart(pPed->GetBonePositionCached((eAnimBoneTag)BONETAG_L_HAND));
		Vector3 vEnd;
		m_target.GetPosition(vEnd);

		// Compute desired pitch angle value
		float fDesiredPitch = CTaskAimGun::CalculateDesiredPitch(pPed, vStart, vEnd);

		// Map the angle to the range 0.0->1.0
		m_fCurrentPitch = CTaskAimGun::ConvertRadianToSignal(fDesiredPitch, fSweepPitchMin, fSweepPitchMax, false);

		m_moveNetworkHelper.SetFloat(ms_PitchId, m_fCurrentPitch);
	}

	if (!m_moveNetworkHelper.IsInTargetState())
		return FSM_Continue;

	//Check if we have a create weapon tag.
	if(m_bHasCreateWeaponTag && m_bMoveCreateWeapon && (!GetPed()->IsNetworkClone()))
	{
		CreateWeapon();
	}

	//Check if we have an interrupt tag.
	if(m_bHasInterruptTag && m_bMoveInterrupt)
	{
		m_moveNetworkHelper.SendRequest(ms_AimRequest);
	}
	else if(!m_bUsingOverrideClipSet)
	{
		float fTimeToAim = 1.15f * m_fRate;
		if (GetTimeInState() >= fTimeToAim)
		{
			m_moveNetworkHelper.SendRequest(ms_AimRequest);
		}
	}

	if(m_bMoveReactBlendFinished && (!GetPed()->IsNetworkClone() || m_bNoLongerRunningOnOwner))
	{	
		pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_Aiming);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_AimWeaponReactionRunning, true);
		pPed->SetPedResetFlag(CPED_RESET_FLAG_SkipAimingIdleIntro, true);

		SetState(State_Finish);
	}

	if(m_target.GetEntity() && m_target.GetEntity()->GetIsTypePed())
	{
		CIkRequestBodyLook lookRequest( m_target.GetEntity(), BONETAG_HEAD );
		lookRequest.SetRefDirHead( LOOKIK_REF_DIR_FACING );
		lookRequest.SetRefDirNeck( LOOKIK_REF_DIR_FACING );
		lookRequest.SetRotLimHead( LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_OFF );
		lookRequest.SetRotLimHead( LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_WIDE );
		lookRequest.SetRotLimNeck( LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_OFF );
		lookRequest.SetRotLimNeck( LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_WIDE );
		lookRequest.SetFlags( LF_WHILE_NOT_IN_FOV );

		if(pPed->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodTimesliceIntelligenceUpdate))
		{
			lookRequest.SetHoldTime( 1000 );
		}
		else
		{
			lookRequest.SetHoldTime( 100 );
		}

		pPed->GetIkManager().Request( lookRequest );
	}

	return FSM_Continue;
}

bool CTaskReactAimWeapon::PlayReaction_OnProcessMoveSignals()
{
	static fwMvBooleanId s_CreateWeaponId("CreateWeapon",0x68C5D794);
	m_bMoveCreateWeapon	|= m_moveNetworkHelper.GetBoolean(s_CreateWeaponId);

	static fwMvBooleanId s_InterruptId("Interrupt",0x6D6BC7B7);
	m_bMoveInterrupt |= m_moveNetworkHelper.GetBoolean(s_InterruptId);

	m_bMoveReactBlendFinished |= m_moveNetworkHelper.GetBoolean(ms_ReactBlendFinished);

	return true;
}

float CTaskReactAimWeapon::CalculateDirection()
{
	CPed* pPed = GetPed();
	Vector3 vTargetPos;
	m_target.GetPosition(vTargetPos);
	
	if(!pPed->IsNetworkClone())
	{
		pPed->SetDesiredHeading(vTargetPos);
	}

	float fOriginalHeading = fwAngle::LimitRadianAngleSafe(m_fReactBlendReferenceHeading);
	float fDesiredHeading = fwAngle::LimitRadianAngleSafe(pPed->GetDesiredHeading());

	TUNE_GROUP_BOOL(ALTERNATE_WEAPONS_ANIMS, bUseSameHeading, false);
	if(bUseSameHeading)
	{
		fDesiredHeading=fOriginalHeading;
	}

	float	fHeadingDelta = SubtractAngleShorter(fDesiredHeading, fOriginalHeading);
	fHeadingDelta = Clamp( fHeadingDelta/PI, -1.0f, 1.0f);

	return ((-fHeadingDelta * 0.5f) + 0.5f);
}

void CTaskReactAimWeapon::CreateWeapon()
{
	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}

	//Create the weapon.
	pWeaponManager->CreateEquippedWeaponObject();
}

const CWeaponInfo* CTaskReactAimWeapon::GetEquippedGunInfo(bool& bIs2Handed) const
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = GetPed()->GetWeaponManager();
	if(!pWeaponManager)
	{
		return NULL;
	}

	//Ensure the weapon info is valid.
	const CWeaponInfo* pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
	if(!pWeaponInfo)
	{
		return NULL;
	}

	//Ensure the weapon is a gun.
	if(!pWeaponInfo->GetIsGun())
	{
		return NULL;
	}

	//Set the 2-handed state.
	bIs2Handed = pWeaponInfo->GetIsGun2Handed();

	return pWeaponInfo;
}

// Clone task implementation
void CTaskReactAimWeapon::OnCloneTaskNoLongerRunningOnOwner()
{
	m_bNoLongerRunningOnOwner = true;
}

// Since we're playing a really quick anim, don't allow migration....
bool CTaskReactAimWeapon::ControlPassingAllowed(CPed *pPed, const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	if (pPed && pPed->IsNetworkClone() && !IsInScope(pPed))
	{
		return true;
	}

	return false;
}


bool CTaskReactAimWeapon::IsInScope(const CPed* pPed)
{
	bool inScope = CTaskFSMClone::IsInScope(pPed);
	if  (inScope)
	{
		const CWeaponInfo* pWeaponInfo = pPed->GetWeaponManager()->GetEquippedWeaponInfo();
		if (pWeaponInfo)
		{
			// The weapon info could not be synchronized yet. We wait until we have the right one
			inScope = (pWeaponInfo->GetAimingInfo() != NULL);
		}
	}

	return inScope;
}


CTaskInfo* CTaskReactAimWeapon::CreateQueriableState() const
{
	CClonedReactAimWeaponInfo::InitParams params(GetState(), m_BaseClipSetId, m_WeaponClipSetId, m_uConfigFlags.GetAllFlags(), &m_target, m_fRate, m_bHasSixDirections, m_bHasCreateWeaponTag, m_bHasInterruptTag);

	return rage_new CClonedReactAimWeaponInfo(params);
}

void CTaskReactAimWeapon::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_REACT_AIM_WEAPON );
    CClonedReactAimWeaponInfo *reactAimWeaponInfo = static_cast<CClonedReactAimWeaponInfo*>(pTaskInfo);

	reactAimWeaponInfo->GetWeaponTargetHelper().ReadQueriableState(m_target);
	m_BaseClipSetId			= reactAimWeaponInfo->GetClipSetId();
	m_WeaponClipSetId		= reactAimWeaponInfo->GetWeaponClipSetId();
	m_fRate					= reactAimWeaponInfo->GetRate();
	m_bHasSixDirections		= reactAimWeaponInfo->GetSixDirections();
	m_bHasCreateWeaponTag	= reactAimWeaponInfo->GetCreateWeapon();
	m_bHasInterruptTag		= reactAimWeaponInfo->GetInterrupt();

	m_uConfigFlags.SetAllFlags(reactAimWeaponInfo->GetFlags());
		
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

CTask::FSM_Return CTaskReactAimWeapon::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if(State_Finish != iState)
	{
		if(iEvent == OnUpdate)
		{
			if(GetStateFromNetwork() != -1)
			{
				// We only want to move onto streaming the anims when we know the info
				// has been set on the owner. 
				if((GetStateFromNetwork() > State_Start) && (GetState() == State_Start))
				{
					SetState(State_StreamAnims);
				}
			}
		}
	}

	FSM_Begin

		FSM_State(State_StreamAnims)
			FSM_OnUpdate
				return StreamAnims_OnUpdate();

		FSM_State(State_PlayReaction)
			FSM_OnEnter
				return PlayReaction_OnEnter();
			FSM_OnUpdate
				return PlayReaction_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

		FSM_State(State_Start)

	FSM_End
}

CTaskFSMClone* CTaskReactAimWeapon::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskReactAimWeapon(m_target, m_uConfigFlags);
}

CTaskFSMClone* CTaskReactAimWeapon::CreateTaskForLocalPed(CPed* UNUSED_PARAM(pPed))
{
	return rage_new CTaskReactAimWeapon(m_target, m_uConfigFlags);
}

#if DEBUG_DRAW
void CTaskReactAimWeapon::DebugRenderClonedInfo(void)
{
	if(GetPed())
	{
		char buffer[256];
		sprintf(buffer, "clipSetId %s\nweaponClipSetId %s\nflags %d\nrate %f\nsixDirections %d\ncreateWeaponTag %d\nhasInterruptTag %d",
		m_BaseClipSetId.GetCStr(),
		m_WeaponClipSetId.GetCStr(),
		m_uConfigFlags.GetAllFlags(),
		m_fRate,
		m_bHasSixDirections,
		m_bHasCreateWeaponTag,
		m_bHasInterruptTag);

		Vector3 pos = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());
		grcDebugDraw::Text(pos, Color_white, buffer, false);
	}
}
#endif /* DEBUG_DRAW */

#if !__FINAL
const char * CTaskReactAimWeapon::GetStaticStateName( s32 iState )
{
	taskAssert(iState >= State_Start && iState <= State_Finish);

	switch (iState)
	{
		case State_Start:				return "State_Start";
		case State_StreamAnims:			return "State_StreamAnims";
		case State_PlayReaction:		return "State_PlayReaction";
		case State_Finish:				return "State_Finish";
		default: taskAssert(0); 	
	}

	return "State_Invalid";
}
#endif // !__FINAL
