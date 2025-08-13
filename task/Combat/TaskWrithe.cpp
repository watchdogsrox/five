#include "task/Combat/TaskWrithe.h"

#include "Animation/MovePed.h"
#include "audio/speechaudioentity.h"
#include "Event/EventDamage.h"
#include "Grcore/debugdraw.h"
#include "Peds/Ped.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Task/Combat/Subtasks/TaskAimFromGround.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task/Physics/TaskNMRelax.h"
#include "Task/Physics/TaskNMHighFall.h"
#include "Weapons/WeaponDamage.h"
#include "Network/NetworkInterface.h"
#include "vfx\misc\Fire.h"

const fwMvClipId CTaskWrithe::ms_ToWritheClipId("ToWritheClip",0xC1AA8B7D);
const fwMvClipId CTaskWrithe::ms_LoopClipId("LoopClip",0x164F9B6B);
const fwMvClipId CTaskWrithe::ms_DieClipId("DieClip",0x3445FC98);

const fwMvFloatId CTaskWrithe::ms_AnimRate("AnimRate",0x77F5D0B5);
const fwMvFloatId CTaskWrithe::ms_Phase("Phase",0xA27F482B);

const fwMvBooleanId CTaskWrithe::ms_AnimFinished("AnimFinished",0xD901FB32);
const fwMvBooleanId CTaskWrithe::ms_OnEnterLoop("OnEnterLoop",0xE959E9B4);
const fwMvBooleanId CTaskWrithe::ms_OnEnterDie("OnEnterDie",0xC1C3F5C9);
const fwMvBooleanId CTaskWrithe::ms_OnEnterToShootFromGround("OnEnterToShootFromGround",0xE597EF86);
const fwMvBooleanId CTaskWrithe::ms_UseHeadIK("UseHeadIK",0x4D6DA622);

const fwMvRequestId CTaskWrithe::ms_ToLoop("ToLoop",0x1DC6623E);
const fwMvRequestId CTaskWrithe::ms_ToDie("ToDie",0xFBEFC41E);
const fwMvRequestId CTaskWrithe::ms_ToShootFromGround("ToShootFromGround",0x9319F7C5);

const fwMvClipSetId CTaskWrithe::m_ClipSetIdToWrithe("To_Writhe_Pistol_Default",0x7ACAE96B);
const fwMvClipSetId CTaskWrithe::m_ClipSetIdLoop("Writhe_Default",0xBA7A26B);
const fwMvClipSetId CTaskWrithe::m_ClipSetIdLoopVar1("Writhe_Var1",0xB4CB086A);
const fwMvClipSetId CTaskWrithe::m_ClipSetIdLoopVar2("Writhe_Var2",0xADF0FB0E);
const fwMvClipSetId CTaskWrithe::m_ClipSetIdWritheTransition("get_up@aim_from_ground@pistol@back",0x522F816C);
const fwMvClipSetId	CTaskWrithe::ms_ClipSetIdDie("Writhe_Die_Default",0xAA8D48F0);

const fwMvFlagId CTaskWrithe::ms_FlagFromGetUp("FromGetUp",0xDDED4B4A);
const fwMvFlagId CTaskWrithe::ms_FlagFromShootFromGround("FromShootFromGround",0xB007450F);

static const int RAGDOLL_FRAME_ENABLE_LAG = 3;
static const float RAGDOLL_ALLOWED_PENETRATION = 0.075f;

static const int MAX_FIRE_LOOP = 3;
static const int MAX_FIRE_LOOP_SCRIPT = 0;

const float CTaskWrithe::MIN_ANIM_RATE = 0.9f;
const float CTaskWrithe::MAX_ANIM_RATE = 2.1f;
const u32 CTaskWrithe::INVALID_CLIP_HASH = 0;

fwMvClipId CClonedWritheInfo::m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall;

CWritheClipSetRequestHelper CWritheClipSetRequestHelperPool::ms_WritheClipSetHelpers[ms_iPoolSize];
bool CWritheClipSetRequestHelperPool::ms_WritheClipSetHelperSlotUsed[CWritheClipSetRequestHelperPool::ms_iPoolSize] = {};

AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

CTaskWrithe::CTaskWrithe(const CWeaponTarget& Target, bool bFromGetUp, bool bEnableRagdollOnCollision, bool bForceShootFromGround)
	: m_Target(Target)
	, m_nFrameDisableRagdollState(0)
	, m_nMinFireloops(0)
	, m_nMaxFireloops(MAX_FIRE_LOOP)
	, m_nFireLoops(0)
	, m_fAnimRate(0.f)
	, m_fPhase(0.f)
	, m_fPhaseStartShooting(0.f)
	, m_shootFromGroundTimer(-1)
	, m_idleClipSetId(CLIP_SET_ID_INVALID)
	, m_idleClipHash(INVALID_CLIP_HASH)
	, m_startClipHash(INVALID_CLIP_HASH)
	, m_uLastTimeBeggingAudioPlayed(0)
	, m_TargetOutOfScopeId_Clone(NETWORK_INVALID_OBJECT_ID)
	, m_DamageEntity(NETWORK_INVALID_OBJECT_ID)
	, m_bFromGetUp(bFromGetUp)
	, m_bFromShootFromGround(false)
	, m_bDieInLoop(false)
	, m_bAnimFinished(false)
	, m_bEnableRagdollOnCollision(bEnableRagdollOnCollision)
	, m_bStartedByScript(false)
	, m_bFetchAsyncTargetTest(false)
	, m_bLeaveMoveNetwork(false)
	, m_bGoStraightToLoop(false)
	, m_bOldInfiniteAmmo(false)
	, m_bOldInfiniteClip(false)
	, m_bForceShootFromGround(bForceShootFromGround)
{	
	SetInternalTaskType(CTaskTypes::TASK_WRITHE);
}

CTaskWrithe::~CTaskWrithe()
{

}

void CTaskWrithe::CleanUp()
{
#if !__FINAL
	if (CPed::ms_bActivateRagdollOnCollision)
#endif
	{
		GetPed()->SetActivateRagdollOnCollision(false);
		GetPed()->ClearActivateRagdollOnCollisionEvent();
	}

	if (m_bLeaveMoveNetwork)
	{
		m_NetworkHelper.ReleaseNetworkPlayer();
	}
	else
	{
		GetPed()->GetMovePed().ClearTaskNetwork(m_NetworkHelper, REALLY_SLOW_BLEND_DURATION);
	}

	if(GetPed()->IsPlayer())
	{
		//! Tidy flags up when done. Ensures if we abort the task, we don't get left in a bad state.
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_DieWhenRagdoll, false);
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_HasHurtStarted, false);
		GetPed()->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceStandardBumpReactionThresholds, false);
		GetPed()->GetInventory()->GetAmmoRepository().SetUsingInfiniteAmmo(m_bOldInfiniteAmmo);
		GetPed()->GetInventory()->GetAmmoRepository().SetUsingInfiniteClips(m_bOldInfiniteClip);
	}
}

#if !__FINAL
const char * CTaskWrithe::GetStaticStateName(s32 _iState)
{
	if(_iState == -1)
	{
		return "INVALID STATE";
	}

	Assert(_iState>=0 && _iState <= State_Finish);
	static const char* lNames[] = 
	{
		"State_Start",
		"State_ToWrithe",
		"State_FromShootFromGround",
		"State_ToShootFromGround",
		"State_Loop",
		"State_StreamGroundClips",
		"State_ShootFromGround",
		"State_StreamDeathAnims",
		"State_Die",
		"State_Finish",
		"State_Invalid",
	};

	return lNames[_iState];
}
#endif // !__FINAL

CTask::FSM_Return CTaskWrithe::ProcessPreFSM()
{	
	if (m_bForceShootFromGround)
	{
		UpdateTarget();
	}

	if(GetState() < State_ToShootFromGround || GetState() == State_Loop)
	{
		static dev_float s_fMinTimeSinceLastAimedAtForAudio = .25f;
		static dev_u32 s_uMinTimeBetweenAimedAtAudioTriggers = 2000;
		if( GetPed()->GetPedIntelligence()->GetTimeSinceLastAimedAtByPlayer() < s_fMinTimeSinceLastAimedAtForAudio &&
			CTimeHelpers::GetTimeSince(m_uLastTimeBeggingAudioPlayed) > s_uMinTimeBetweenAimedAtAudioTriggers )
		{
			m_uLastTimeBeggingAudioPlayed = fwTimer::GetTimeInMilliseconds();
			GetPed()->NewSay("DYING_PLEASE");
		}
	}

	return FSM_Continue;
}
 
CTask::FSM_Return CTaskWrithe::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	CPed* pPed = GetPed();
	pPed->SetPedResetFlag(CPED_RESET_FLAG_IsInWrithe, true);

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();		

		FSM_State(State_ToWrithe)
			FSM_OnEnter
				ToWrithe_OnEnter();
			FSM_OnUpdate
				return ToWrithe_OnUpdate();	

		FSM_State(State_FromShootFromGround)
			FSM_OnEnter
				FromShootFromGround_OnEnter();
			FSM_OnUpdate
				return FromShootFromGround_OnUpdate();	

		FSM_State(State_ToShootFromGround)
			FSM_OnEnter
				ToShootFromGround_OnEnter();
			FSM_OnUpdate
				return ToShootFromGround_OnUpdate();	

		FSM_State(State_Loop)
			FSM_OnEnter
				Loop_OnEnter(pPed);
			FSM_OnUpdate
				return Loop_OnUpdate();	

		FSM_State(State_StreamGroundClips)
			FSM_OnUpdate
				return StreamGroundClips_OnUpdate();	

		FSM_State(State_ShootFromGround)
			FSM_OnEnter
				ShootFromGround_OnEnter();
			FSM_OnUpdate
				return ShootFromGround_OnUpdate();	
			FSM_OnExit
				ShootFromGround_OnExit();	

		FSM_State(State_StreamDeathAnims)
			FSM_OnUpdate
				return StreamDeathAnims_OnUpdate();	

		FSM_State(State_Die)
			FSM_OnEnter
				Die_OnEnter();
			FSM_OnUpdate
				return Die_OnUpdate();	

		FSM_State(State_Finish)
			FSM_OnEnter
				Finish_OnEnter();
			FSM_OnUpdate
				return Finish_OnUpdate();

	FSM_End
}

bool CTaskWrithe::ProcessMoveSignals()
{
	switch(GetState())
	{
		case State_Loop:
			return Loop_OnProcessMoveSignals();
		case State_ShootFromGround:
			return ShootFromGround_OnProcessMoveSignals();
		case State_FromShootFromGround:
			return FromShootFromGround_OnProcessMoveSignals();
		case State_ToShootFromGround:
			return ToShootFromGround_OnProcessMoveSignals();
		case State_ToWrithe:
			return ToWrithe_OnProcessMoveSignals();
		default:
			break;
	}
	return false;
}

bool CTaskWrithe::GetDieClipSetAndRandClip(fwMvClipSetId& ClipSetId, fwMvClipId& ClipId)
{
	const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(ms_ClipSetIdDie);
	taskAssertf(pClipSet, "Ehh not valid clipset in TaskWrithe::GetDieClipSetAndRandClip()");
	if (pClipSet && pClipSet->IsStreamedIn_DEPRECATED())
	{
		ClipSetId = ms_ClipSetIdDie;
		if (pClipSet->GetClipItemCount() <= 0)
		{
			crClipDictionary* pClipDictionary = pClipSet->GetClipDictionary();
			if (pClipDictionary && pClipDictionary->GetNumClips() > 0)
			{
				int iClip = fwRandom::GetRandomNumberInRange(0,pClipDictionary->GetNumClips() - 1);
				ClipId = (fwMvClipId)pClipDictionary->FindKeyByIndex(iClip);
				return true;
			}
		}

		taskAssertf(pClipSet->GetClipItemCount(), "No Clips in Clipset! TaskWrithe::GetDieClipSetAndRandClip()");
		int iClip = fwRandom::GetRandomNumberInRange(0,pClipSet->GetClipItemCount() - 1);
		ClipId = pClipSet->GetClipItemIdByIndex(iClip);
		return true;
	}

	return false;
}

bool CTaskWrithe::StreamReqResourcesIn(CWritheClipSetRequestHelper* pClipSetRequestHelper, CPed* pPed, int iNextState)
{
	taskAssertf(pClipSetRequestHelper, "Ehh no pClipSetRequestHelper for CTaskWrithe::StreamReqResourcesIn()");

	if (iNextState & NEXT_STATE_COLLAPSE)
		pClipSetRequestHelper->AddClipSetRequest(m_ClipSetIdToWrithe);
	if (iNextState & NEXT_STATE_WRITHE)
	{
		taskAssertf(pPed->GetHurtEndTime(), "No hurt endtime set, might get a pop! CTaskWrithe::StreamReqResourcesIn()");
		pClipSetRequestHelper->AddClipSetRequest(((pPed->GetHurtEndTime() & 1) ? m_ClipSetIdLoopVar1 : m_ClipSetIdLoopVar2));
	}
	if (iNextState & NEXT_STATE_TRANSITION)
		pClipSetRequestHelper->AddClipSetRequest(m_ClipSetIdWritheTransition);
	if (iNextState & NEXT_STATE_DIE)
		pClipSetRequestHelper->AddClipSetRequest(ms_ClipSetIdDie);

	return pClipSetRequestHelper->RequestAllClipSets();
}

bool CTaskWrithe::StreamReqResourcesInOldAndBad(CPed* pPed, int iNextState)
{
	bool bAllLoaded = true;
	atFixedArray<fwClipSet*, 4> ClipSetToLoad;	// Note, arraysize is defined here, more states bigger array

	// Ped might contain metadata for this later on
	if (iNextState & NEXT_STATE_COLLAPSE)
		ClipSetToLoad.Push(fwClipSetManager::GetClipSet(m_ClipSetIdToWrithe));
	if (iNextState & NEXT_STATE_WRITHE)
		ClipSetToLoad.Push(fwClipSetManager::GetClipSet(((pPed->GetHurtEndTime() & 1) ? m_ClipSetIdLoopVar1 : m_ClipSetIdLoopVar2)));
	if (iNextState & NEXT_STATE_TRANSITION)
		ClipSetToLoad.Push(fwClipSetManager::GetClipSet(m_ClipSetIdWritheTransition));
	if (iNextState & NEXT_STATE_DIE)
		ClipSetToLoad.Push(fwClipSetManager::GetClipSet(ms_ClipSetIdDie));

	int nClipset = ClipSetToLoad.GetCount();
	for (int i = 0; i < nClipset; ++i)
	{
		taskAssert(ClipSetToLoad[i]);
		if (!ClipSetToLoad[i]->Request_DEPRECATED())
			bAllLoaded = false;
	}

	return bAllLoaded;
}

void CTaskWrithe::Start_OnEnter()
{
	CPed* pPed = GetPed();
	m_NetworkHelper.RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskWrithe);

	ASSERT_ONLY( pPed->SpewDamageHistory(); )

	if(!pPed->IsNetworkClone() && m_nFireLoops == 0) // We can go back here from when we were on the ground shooting
	{
		pPed->NewSay("DYING_HELP");

		if (pPed->GetHurtEndTime() <= 0)
			pPed->SetHurtEndTime(fwRandom::GetRandomNumberInRange(1, 10));	// Just to consider this started and randomize clips

		// In case we have been scripted to run this we must kickstart stuff
		if (pPed->IsBodyPartDamaged(CPed::DAMAGED_GOTOWRITHE))
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasHurtStarted, true);

			m_nMinFireloops = 1;
			pPed->ClearBodyPartDamage(CPed::DAMAGED_GOTOWRITHE);
		}
		else if (!pPed->HasHurtStarted())
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasHurtStarted, true);
			m_bStartedByScript = true;
			m_nMaxFireloops = MAX_FIRE_LOOP_SCRIPT;
		}
		else if (!m_bGoStraightToLoop && !m_bForceShootFromGround)
		{
			TUNE_GROUP_INT(WRITHE_TWEAK, ChanceToDieBeforeLoop, 80, 0, 100, 1);
			if (fwRandom::GetRandomNumberInRange(1, 100) <= ChanceToDieBeforeLoop && CTaskNMHighFall::CanUseRagdoll(pPed, RAGDOLL_TRIGGER_DIE))
			{
				// Block pain from playing, otherwise it comes across as a late triggered death and sounds bad.
				if(pPed->GetSpeechAudioEntity())
					pPed->GetSpeechAudioEntity()->DisablePainAudio(true);
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DieWhenRagdoll, true);
				CEventSwitch2NM event(10000, rage_new CTaskNMHighFall(1000, NULL, CTaskNMHighFall::HIGHFALL_SLOPE_SLIDE));
				pPed->SwitchToRagdoll(event);
			}
		}

		pPed->SetHealthLostScript(0.f);	// Reset this yes
		if (pPed->GetHealth() > pPed->GetHurtHealthThreshold())
			pPed->SetHealth(pPed->GetHurtHealthThreshold() - 1.0f); //ensure actually in hurt threshold

		// Equip a gun? Or let script do that?
	}

	if (!pPed->IsNetworkClone())
	{
		CEntity* pEntity = pPed->GetWeaponDamageEntityPedSafe();
		if (pEntity && pEntity->GetIsTypePed())
		{
			if (((CPed*)pEntity)->GetNetworkObject())
			{
				m_DamageEntity = ((CPed*)pEntity)->GetNetworkObject()->GetObjectID();
			}
		}
	}
}

CTask::FSM_Return CTaskWrithe::Start_OnUpdate()
{
	CPed* pPed = GetPed();

	//! Immediately quit out of writhe if a player. This should never happen.
	m_bOldInfiniteAmmo = pPed->GetInventory()->GetAmmoRepository().GetIsUsingInfiniteAmmo();
	m_bOldInfiniteClip = pPed->GetInventory()->GetAmmoRepository().GetIsUsingInfiniteClips();
	if(!pPed->IsPlayer())
	{
		pPed->GetInventory()->GetAmmoRepository().SetUsingInfiniteAmmo(true);
		pPed->GetInventory()->GetAmmoRepository().SetUsingInfiniteClips(true);
	}

	pPed->SetIsStrafing(true);
	
	// Make sure we don't timeslice the ai / animation this frame.
	pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);

	// Don't want to start the move network if we've just started a relax behaviour in the on enter.
	// We'll be aborted by the event next frame anyway
	if (!pPed->GetUsingRagdoll() && InitiateMoveNetwork())
	{
		CMovePed& Move = pPed->GetMovePed();
		Move.SetTaskNetwork(m_NetworkHelper, SLOW_BLEND_DURATION);

		if(!pPed->IsNetworkClone())
		{
			if(m_bEnableRagdollOnCollision)
			{
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DieWhenRagdoll, true);
				pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceStandardBumpReactionThresholds, true);

#if !__FINAL
				if (CPed::ms_bActivateRagdollOnCollision)
#endif
				{
					if (!pPed->GetActivateRagdollOnCollision())
					{
						CEventSwitch2NM event(10000, rage_new CTaskNMHighFall(1000, NULL, CTaskNMHighFall::HIGHFALL_SLOPE_SLIDE));
						pPed->SetActivateRagdollOnCollisionEvent(event);
						pPed->SetActivateRagdollOnCollision(true);
						pPed->SetRagdollOnCollisionAllowedPenetration(RAGDOLL_ALLOWED_PENETRATION);
					}
				}
			}
			m_fAnimRate = fwRandom::GetRandomNumberInRange(MIN_ANIM_RATE, MAX_ANIM_RATE); 
		}
		else
		{
			Assert(m_fAnimRate != 0.0f);
		}

		m_NetworkHelper.SetFloat(ms_AnimRate, m_fAnimRate);
		m_NetworkHelper.SetFlag(m_bFromGetUp, ms_FlagFromGetUp);
		m_NetworkHelper.SetFlag(m_bFromShootFromGround, ms_FlagFromShootFromGround);
	
		// if we're a clone, hang on for a bit until the owner has moved on....
		if(pPed->IsNetworkClone())
		{
			if(GetStateFromNetwork() == State_Start)
			{
				return FSM_Continue;
			}
		}

		if (m_bFromShootFromGround)
			TaskSetState(State_FromShootFromGround);
		else if (m_bFromGetUp || m_bGoStraightToLoop || m_bForceShootFromGround)
			TaskSetState(State_Loop);
		else
		{
			if (!pPed->IsNetworkClone() && m_bEnableRagdollOnCollision)
				GetPed()->SetActivateRagdollOnCollision(false);

			TaskSetState(State_ToWrithe);
		}
	}

	return FSM_Continue;
}

void CTaskWrithe::ToWrithe_OnEnter()
{
	if (GetPed() && !GetPed()->IsNetworkClone() && m_bEnableRagdollOnCollision)
	{
		m_nFrameDisableRagdollState = 0;
		GetPed()->SetActivateRagdollOnCollision(false);
	}
	RequestProcessMoveSignalCalls();
}

CTask::FSM_Return CTaskWrithe::ToWrithe_OnUpdate()
{
	CPed* pPed = GetPed();
	if (pPed && !pPed->IsNetworkClone())
	{
		++m_nFrameDisableRagdollState;
		if (m_nFrameDisableRagdollState == RAGDOLL_FRAME_ENABLE_LAG && m_bEnableRagdollOnCollision)
		{
			//@@: location CTASKWRITEH_TOWRITEH_ONUPDATE_SET_rAGDOLL_COLLISION
			GetPed()->SetActivateRagdollOnCollision(true);
			GetPed()->SetRagdollOnCollisionAllowedPenetration(RAGDOLL_ALLOWED_PENETRATION);
		}

		m_idleClipSetId = GetIdleClipSet(pPed);
	}

	Assert(m_idleClipSetId != CLIP_SET_ID_INVALID);

	m_ClipSetRequestHelper.Request(m_idleClipSetId);
	if (m_ClipSetRequestHelper.IsLoaded())
	{
		if(m_bAnimFinished)
		{
			// if we're a network clone, we want to hang tight until the owner has moved on....
			if(pPed->IsNetworkClone())
			{
				if(GetStateFromNetwork() == State_ToWrithe)
				{
					return FSM_Continue;
				}
			}

			m_bAnimFinished = false;
			TaskSetState(State_Loop);
			return FSM_Continue;
		}
	}

	return FSM_Continue;
}

bool CTaskWrithe::ToWrithe_OnProcessMoveSignals()
{
	CPed* pPed = GetPed();
	pPed->SetDesiredHeading(GetPed()->GetCurrentHeading());	// To prevent weird stuttering in animation
	pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);

	if (m_NetworkHelper.GetBoolean(ms_AnimFinished))
		m_bAnimFinished = true;

	return true;
}

void CTaskWrithe::FromShootFromGround_OnEnter()
{
	m_NetworkHelper.SetClipSet(m_ClipSetIdWritheTransition);
	RequestProcessMoveSignalCalls();
}

CTask::FSM_Return CTaskWrithe::FromShootFromGround_OnUpdate()
{
	CPed* pPed = GetPed();
	
	if (!pPed->IsNetworkClone())
		m_idleClipSetId = GetIdleClipSet(pPed);

	Assert(m_idleClipSetId != CLIP_SET_ID_INVALID);

	m_ClipSetRequestHelper.Request(m_idleClipSetId);
	if (m_ClipSetRequestHelper.IsLoaded() && m_bAnimFinished)
	{
		// if we're a network clone, we want to hang tight until the owner has moved on....
		if(pPed->IsNetworkClone())
		{
			if(GetStateFromNetwork() == State_FromShootFromGround)
			{
				return FSM_Continue;
			}
		}

		m_bAnimFinished = false;
		TaskSetState(State_Loop);
		return FSM_Continue;
	}

	ActivateTimeslicing();

	return FSM_Continue;
}

bool CTaskWrithe::FromShootFromGround_OnProcessMoveSignals()
{
	CPed* pPed = GetPed();
	pPed->SetDesiredHeading(pPed->GetCurrentHeading());
	pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);

	if (m_NetworkHelper.GetBoolean(ms_AnimFinished))
	{
		m_bAnimFinished = true;
	}

	return true;
}

void CTaskWrithe::ToShootFromGround_OnEnter()
{
	m_NetworkHelper.SetClipSet(m_ClipSetIdWritheTransition);
	m_NetworkHelper.SendRequest(ms_ToShootFromGround);
	m_NetworkHelper.WaitForTargetState(ms_OnEnterToShootFromGround);
	RequestProcessMoveSignalCalls();
}

CTask::FSM_Return CTaskWrithe::ToShootFromGround_OnUpdate()
{
	CPed* pPed = GetPed();

	if (!m_NetworkHelper.IsInTargetState())
		return FSM_Continue;

	if(m_bAnimFinished)
	{
		// if we're a network clone, we want to hang tight until the owner has moved on....
		if(pPed && pPed->IsNetworkClone())
		{
			if(GetStateFromNetwork() == State_ToShootFromGround)
			{
				return FSM_Continue;
			}
		}

		m_bAnimFinished = false;
		TaskSetState(State_ShootFromGround);
		return FSM_Continue;
	}

	//B*2008299 - This causes bad timing issues
	//ActivateTimeslicing();

	return FSM_Continue;
}

bool CTaskWrithe::ToShootFromGround_OnProcessMoveSignals()
{
	CPed* pPed = GetPed();
	pPed->SetDesiredHeading(GetPed()->GetCurrentHeading());	// To prevent weird stuttering in animation
	pPed->ForceMotionStateThisFrame(CPedMotionStates::MotionState_AnimatedVelocity);

	if (m_NetworkHelper.GetBoolean(ms_UseHeadIK) && m_Target.GetEntity())
	{
		TUNE_GROUP_INT(WRITHE_TWEAK, UseHeadIKTime, 300, 0, 1000, 1);

		pPed->GetIkManager().LookAt(0, m_Target.GetEntity(), UseHeadIKTime, BONETAG_HEAD, NULL, LF_WIDE_PITCH_LIMIT | LF_WIDEST_YAW_LIMIT);
	}

	if(m_NetworkHelper.GetBoolean(ms_AnimFinished))
	{
		m_bAnimFinished = true;
	}

	return true;
}

void CTaskWrithe::Loop_OnEnter(CPed* pPed)
{
	TUNE_GROUP_INT(WRITHE_TWEAK, ChanceToDieInLoop, 50, 0, 100, 1);

	m_NetworkHelper.SendRequest(ms_ToLoop);
	m_NetworkHelper.WaitForTargetState(ms_OnEnterLoop);

	const crClip* clip = NULL;

	if (!pPed->IsNetworkClone())
	{
		m_idleClipSetId = GetIdleClipSet(pPed);
		clip = GetRandClipFromClipSet(m_idleClipSetId, m_idleClipHash);
	}
	else
	{
		clip = GetClipFromClipSet(m_idleClipSetId, m_idleClipHash);
	}

	Assert(m_idleClipSetId != CLIP_SET_ID_INVALID);
	Assert(m_idleClipHash != INVALID_CLIP_HASH);
	Assert(clip != NULL);

	m_NetworkHelper.SetClip(clip, ms_LoopClipId);

	m_fPhase = -1.f;
	m_bFetchAsyncTargetTest = false;

	if(!pPed->IsNetworkClone())
	{
		if (pPed->GetGroundPhysical())
		{
			m_bDieInLoop = true;
			m_bAnimFinished = true;
		}
		else if (pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_DisableWritheShootFromGround))
		{
			m_bDieInLoop = true;
		}
		else if (m_nMinFireloops > 0 && m_nFireLoops <= m_nMinFireloops)
		{
			m_bDieInLoop = false;
			m_fPhaseStartShooting = fwRandom::GetRandomNumberInRange(0.1f, 0.5f);
		}
		else if (!m_bForceShootFromGround && m_nMaxFireloops > 0 && m_nFireLoops >= m_nMaxFireloops)
		{
			m_bDieInLoop = true;
			m_bAnimFinished = true;
		}
		else
		{
			m_bDieInLoop = ((fwRandom::GetRandomNumberInRange(1, 100) <= ChanceToDieInLoop) || !m_Target.GetIsValid()) && !m_bForceShootFromGround;
			m_fPhaseStartShooting = fwRandom::GetRandomNumberInRange(0.5f, 1.0f);
		}
	}

	RequestProcessMoveSignalCalls();
}

CTask::FSM_Return CTaskWrithe::Loop_OnUpdate()
{
	CPed* pPed = GetPed();

	if (!m_NetworkHelper.IsInTargetState())
		return FSM_Continue;

	ActivateTimeslicing();

	if(pPed && pPed->GetSpeechAudioEntity() && !pPed->GetSpeechAudioEntity()->IsPainPlaying())
	{
		audDamageStats damageStats;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DYING_MOAN;
		pPed->GetSpeechAudioEntity()->InflictPain(damageStats);
	}

	if(m_bAnimFinished)
	{		
		bool bCanTransitionToShooting = true;
		
		if (m_bForceShootFromGround)
		{
			if (!m_Target.GetEntity())
				bCanTransitionToShooting = false;

			if (pPed->GetWeaponManager() && !pPed->GetWeaponManager()->GetEquippedWeapon())
				bCanTransitionToShooting = false;
		}

		if(m_ClipSetRequestHelper.IsLoaded() && bCanTransitionToShooting)
		{
			if(pPed && pPed->IsNetworkClone())
			{
				if(GetStateFromNetwork() == State_Loop)
				{
					return FSM_Continue;
				}
			}

			m_bAnimFinished = false;

			if (m_bDieInLoop)
			{
				TaskSetState(State_Die);
				return FSM_Continue;
			}

			static const int nMaxShootingFromGroundPeds = 4;

			if(GetPed() && GetPed()->IsNetworkClone())
			{
				// m_bDieInLoop is synced so if we don't move to shooting, we'll move to dieing properly anyway...
				TaskSetState(State_ToShootFromGround);
				return FSM_Continue;
			}
			else if (CTaskGetUp::ms_NumPedsAimingFromGround < nMaxShootingFromGroundPeds || m_bForceShootFromGround)
			{
				TaskSetState(State_ToShootFromGround);
				return FSM_Continue;
			}
			else
			{
				// Do nothing and we die next loop instead
				m_bDieInLoop = true;
				return FSM_Continue;
			}
		}
	}

	return FSM_Continue;
}

bool CTaskWrithe::Loop_OnProcessMoveSignals()
{
	CPed* pPed = GetPed();

	if (!m_NetworkHelper.IsInTargetState())
		return true;

	// This can be true especially in multiplayer so if our target is gone we should just die
	if (!m_Target.GetEntity())
	{
		m_bDieInLoop = true;
		m_bAnimFinished = true;
	}
	else if (m_Target.GetEntity()->GetIsTypePed())
	{
		CPed* pTargetPed = (CPed*)m_Target.GetEntity();
		if (pTargetPed->IsDead())
			m_bDieInLoop = true;
	}

	// Kill writhing peds when they catch fire.
	// We need to do this as writhe intentionally blocks on fire behaviours.
	if (g_fireMan.IsEntityOnFire(pPed))
	{
		m_bDieInLoop = true;
		m_bAnimFinished = true;
	}

	// See if we pass the phase where we will see if we should just die instead
	// Possibly move this logic out
	static const float fEvaluateDeathAtPhase = 0.75f;
	if (!m_bDieInLoop && !m_bStartedByScript && m_nFireLoops >= m_nMinFireloops)
	{
		int iTargetFlags = TargetOption_IgnoreTargetsCover | TargetOption_UseTargetableDistance;
		if ((m_NetworkHelper.GetFloat(ms_Phase) > fEvaluateDeathAtPhase && m_fPhase <= fEvaluateDeathAtPhase) || m_fPhase == -1.f)
		{
			static const float fWritheMinDistanceToTagetSqr = 25.f * 25.f;
			static const float fWritheMinDistanceZToTaget = 2.f; // Target is x meters above us
			Vector3 OurPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			Vector3 TargetPos = VEC3V_TO_VECTOR3(m_Target.GetEntity()->GetTransform().GetPosition());
			
			if ((TargetPos.z - OurPos.z) > fWritheMinDistanceZToTaget)
				m_bDieInLoop = true;
			else if (OurPos.Dist2(TargetPos) > fWritheMinDistanceToTagetSqr)
				m_bDieInLoop = true;
			else if (m_Target.GetEntity()->GetIsTypePed())
			{
				ECanTargetResult eRet = CPedGeometryAnalyser::CanPedTargetPedAsync(*pPed, *(CPed*)m_Target.GetEntity(), iTargetFlags, true, CPedGeometryAnalyser::ms_fCanTargetCacheMaxPosChangeSqr, CAN_TARGET_CACHE_TIMEOUT);
				if (eRet == ECanNotTarget)
					m_bDieInLoop = true;
				else if (eRet == ENotSureYet)
					m_bFetchAsyncTargetTest = true;
			}
		}

		if (m_bFetchAsyncTargetTest)
		{
			ECanTargetResult eRet = CPedGeometryAnalyser::CanPedTargetPedAsync(*pPed, *(CPed*)m_Target.GetEntity(), iTargetFlags, false, CPedGeometryAnalyser::ms_fCanTargetCacheMaxPosChangeSqr, CAN_TARGET_CACHE_TIMEOUT);
			if (eRet == ECanNotTarget)
				m_bDieInLoop = true;
			else if (eRet == ECanTarget)
				m_bFetchAsyncTargetTest = false;	// Ok we are done with this async stuff

		}

		if (m_bDieInLoop)
			m_bAnimFinished = true;
	}

	m_fPhase = m_NetworkHelper.GetFloat(ms_Phase);

	if(m_NetworkHelper.GetBoolean(ms_AnimFinished) || (!pPed->IsNetworkClone() && !m_bDieInLoop && m_fPhaseStartShooting < m_fPhase))
		m_bAnimFinished = true;

	if (m_nFireLoops >= m_nMinFireloops && !m_bDieInLoop && (m_fPhaseStartShooting < m_fPhase - 0.1f || m_bAnimFinished))	// Give the ped some time to stream the anim in also
	{
		const Vector3 vOurPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
		const Vector3 vOurFwd = VEC3V_TO_VECTOR3(pPed->GetTransform().GetForward());
		const Vector3 vTargetPos = VEC3V_TO_VECTOR3(m_Target.GetEntity()->GetTransform().GetPosition());
		Vector3 vToTarget = vTargetPos - vOurPos;
		vToTarget.z = 0.f;
		vToTarget.Normalize();

		if (vToTarget.Dot(vOurFwd) < -0.5f)	// 
			m_bDieInLoop = true;

	}

	if (m_bForceShootFromGround)
		m_bDieInLoop = false;

	if (m_bDieInLoop)
		m_ClipSetRequestHelper.Request(ms_ClipSetIdDie);
	else
		m_ClipSetRequestHelper.Request(m_ClipSetIdWritheTransition);

	return true;
}

CTask::FSM_Return CTaskWrithe::StreamGroundClips_OnUpdate()
{
	m_ClipSetRequestHelper.Request(m_ClipSetIdWritheTransition);
	if (!m_ClipSetRequestHelper.IsLoaded())
		return FSM_Continue;

	// Possible stream the shoot from ground anims also

	TaskSetState(State_ToShootFromGround);
	return FSM_Continue;
}

void CTaskWrithe::ShootFromGround_OnEnter()
{
	if (m_Target.GetIsValid())
	{
		++m_nFireLoops;
		if(GetPed() && !GetPed()->IsNetworkClone())
		{
			m_nFrameDisableRagdollState = 0;
			if(m_bEnableRagdollOnCollision)
				GetPed()->SetActivateRagdollOnCollision(false);

			const u32 ScriptedGunTaskHash = ATSTRINGHASH("SCRIPTED_GUN_TASK_WRITHE", 0x0dc9295ad);
			CTaskAimFromGround* pTask = rage_new CTaskAimFromGround(ScriptedGunTaskHash, 0, m_Target, m_bForceShootFromGround);
			SetNewTask(pTask);

			bool bUseCustomTimer = m_bForceShootFromGround && m_shootFromGroundTimer != -1;
			if (!bUseCustomTimer)
			{
				TUNE_GROUP_INT(WRITHE_TWEAK, MinTimeInShootFromGround, 3000, 0, 10000, 100);
				TUNE_GROUP_INT(WRITHE_TWEAK, MaxTimeInShootFromGround, 5000, 0, 15000, 100);

				m_shootFromGroundTimer = fwRandom::GetRandomNumberInRange(MinTimeInShootFromGround, MaxTimeInShootFromGround);
			}
		}
		else
		{
			Assert(m_shootFromGroundTimer != -1);
		}

		m_Timer.Set(fwTimer::GetTimeInMilliseconds(), m_shootFromGroundTimer);

		CTaskGetUp::ms_LastAimFromGroundStartTime = fwTimer::GetTimeInMilliseconds();

		if(GetPed() && !GetPed()->IsNetworkClone())
		{
			CTaskGetUp::ms_NumPedsAimingFromGround++;
		}
	}

	m_ClipSetRequestHelper.Request(m_ClipSetIdWritheTransition);
	RequestProcessMoveSignalCalls();

}

CTask::FSM_Return CTaskWrithe::ShootFromGround_OnUpdate()
{
	CTask* pSubTask = GetSubTask();

	// Could cause a glitch I guess if clipset is not streamed in if the subtask quit
	if(m_ClipSetRequestHelper.IsLoaded() && (GetIsFlagSet(aiTaskFlags::SubTaskFinished) || !pSubTask || m_Timer.IsOutOfTime() || (m_bForceShootFromGround && !GetIsTargetValid(m_Target.GetEntity()))))
	{
		// if we're a network clone, we want to hang tight until the owner has moved on....
		if(GetPed() && GetPed()->IsNetworkClone())
		{
			if(GetStateFromNetwork() == State_ShootFromGround)
			{
				return FSM_Continue;
			}
		}

		m_bFromShootFromGround = true;

		TaskSetState(State_Start);
		return FSM_Continue;
	}

	//B*2008299 - This causes bad timing issues
	//ActivateTimeslicing();

	return FSM_Continue;
}

void CTaskWrithe::ShootFromGround_OnExit()
{
	if(GetPed() && !GetPed()->IsNetworkClone())
	{
		if (m_Target.GetIsValid())
			CTaskGetUp::ms_NumPedsAimingFromGround--;
	}
}

bool CTaskWrithe::ShootFromGround_OnProcessMoveSignals()
{
	if(GetPed() && !GetPed()->IsNetworkClone())
	{
		++m_nFrameDisableRagdollState;
		if (m_nFrameDisableRagdollState == RAGDOLL_FRAME_ENABLE_LAG && m_bEnableRagdollOnCollision)
		{
			GetPed()->SetActivateRagdollOnCollision(true);
			GetPed()->SetRagdollOnCollisionAllowedPenetration(RAGDOLL_ALLOWED_PENETRATION);
		}
	}

	m_ClipSetRequestHelper.Request(m_ClipSetIdWritheTransition);

	return true;
}

CTask::FSM_Return CTaskWrithe::StreamDeathAnims_OnUpdate()
{
	m_ClipSetRequestHelper.Request(ms_ClipSetIdDie);
	if (!m_ClipSetRequestHelper.IsLoaded())
		return FSM_Continue;

	TaskSetState(State_Die);
	return FSM_Continue;
}

void CTaskWrithe::Die_OnEnter()
{
	//m_NetworkHelper.SendRequest(ms_ToDie);
	//m_NetworkHelper.WaitForTargetState(ms_OnEnterDie);
	//m_NetworkHelper.SetClip(GetRandDieClip(), ms_DieClipId);

	CPed* pPed = GetPed();

	ASSERT_ONLY( pPed->SpewDamageHistory(); )

	if (!pPed->IsNetworkClone() && m_bEnableRagdollOnCollision)
		pPed->SetActivateRagdollOnCollision(false);

	// Always fetch the last damager for correct kill reward
	CEntity* inflictor = pPed->GetWeaponDamageEntityPedSafe();
	if (!inflictor || !inflictor->GetIsTypePed())
	{
		if (m_DamageEntity != NETWORK_INVALID_OBJECT_ID)
			inflictor = NetworkUtils::GetPedFromNetworkObject(NetworkInterface::GetNetworkObject(m_DamageEntity));

		if (!inflictor || !inflictor->GetIsTypePed())
			inflictor = (CEntity*)m_Target.GetEntity();
	}

	// This can also happen in singleplayer and is valid
//	aiAssertf(!NetworkInterface::IsGameInProgress() || inflictor, "Ped %s is dying from bleeding and doesn't have a killer.", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "");

	CEventDamage event(inflictor, fwTimer::GetTimeInMilliseconds(), WEAPONTYPE_BLEEDING);
	if(!pPed->GetPedIntelligence()->HasEventOfType(&event))
	{
		// If the damage (or task) happened before we cleared the wanted level then we don't want to pass in the inflictor
		bool bKillPriorToClearedWantedLevel = CPedDamageCalculator::WasDamageFromEntityBeforeWantedLevelCleared(inflictor, pPed);

		fwFlags32 damageFlags = bKillPriorToClearedWantedLevel ? CPedDamageCalculator::DF_KillPriorToClearedWantedLevel : CPedDamageCalculator::DF_None;
#if __ASSERT
		damageFlags.SetFlag(CPedDamageCalculator::DF_DontAssertOnNullInflictor);
#endif // __ASSERT
		
		// Should we check last hit here and mark as killed by melee?
		if (pPed->GetWeaponDamagedMeleeHit())
			damageFlags.SetFlag(CPedDamageCalculator::DF_MeleeDamage);

		CPedDamageCalculator damageCalculator(inflictor, pPed->GetHealth(), WEAPONTYPE_BLEEDING, 0, false);
		damageCalculator.ApplyDamageAndComputeResponse(pPed, event.GetDamageResponseData(), damageFlags);
		if ((event.GetDamageResponseData().m_bKilled || event.GetDamageResponseData().m_bInjured))
		{
			CWeaponDamage::GeneratePedDamageEvent(inflictor, pPed, WEAPONTYPE_BLEEDING, 10.0f, VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()), NULL, damageFlags);
		}
	}
}

CTask::FSM_Return CTaskWrithe::Die_OnUpdate()
{
	// Don't quit out as we need to keep the death anims streamed in

//	TaskSetState(State_Finish);
	return FSM_Continue;
}

void CTaskWrithe::Finish_OnEnter()
{

}

CTask::FSM_Return CTaskWrithe::Finish_OnUpdate()
{
#if !__FINAL
	if (CPed::ms_bActivateRagdollOnCollision)
#endif
	{
		GetPed()->SetActivateRagdollOnCollision(false);
		GetPed()->ClearActivateRagdollOnCollisionEvent();
	}

	return FSM_Quit;
}

bool CTaskWrithe::ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	if (pEvent && ((CEvent*)pEvent)->GetEventPriority() < E_PRIORITY_WRITHE)
		return false;

	// If go to Ragdoll, kill?
	return CTask::ShouldAbort(iPriority, pEvent);
}

void CTaskWrithe::DoAbort(const AbortPriority iPriority, const aiEvent* pEvent)
{
	CTask::DoAbort(iPriority, pEvent);
}

void CTaskWrithe::ActivateTimeslicing()
{
	GetPed()->GetPedAiLod().ClearBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate);
}

const crClip* CTaskWrithe::GetRandClipFromClipSet(fwMvClipSetId ClipSet, s32& clipHash)
{
	const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(ClipSet);
	taskAssertf(pClipSet, "Ehh not valid clipset in WritheTask");

	if (pClipSet)
	{
		if (pClipSet->GetClipItemCount() <= 0)
		{
			crClipDictionary* pClipDictionary = pClipSet->GetClipDictionary();
			if (pClipDictionary)
			{
				int iClip = fwRandom::GetRandomNumberInRange(0,pClipDictionary->GetNumClips() - 1);
				clipHash = pClipDictionary->FindKeyByIndex(iClip);
				crClip *pClip = pClipDictionary->GetClip(clipHash);

				return pClip;
			}
		}

		taskAssertf(pClipSet->GetClipItemCount(), "No Clips in Clipset!");
		int iClip = fwRandom::GetRandomNumberInRange(0,pClipSet->GetClipItemCount() - 1);
		clipHash = pClipSet->GetClipItemIdByIndex(iClip);
		const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(ClipSet, fwMvClipId(clipHash));
		return pClip;
	}

	return NULL;
}

const crClip* CTaskWrithe::GetClipFromClipSet(fwMvClipSetId const& clipSetId, s32 const clipHash)
{
	const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	taskAssertf(pClipSet, "Ehh not valid clipset in WritheTask");

	if (pClipSet)
	{
		if (pClipSet->GetClipItemCount() <= 0)
		{
			crClipDictionary* pClipDictionary = pClipSet->GetClipDictionary();
			if (pClipDictionary)
			{
				crClip *pClip = pClipDictionary->GetClip(clipHash);
				return pClip;
			}
		}
	}

	return NULL;
}

bool CTaskWrithe::InitiateMoveNetwork()
{
	CPed* pPed = GetPed();

	int iNextState = 0;
	if (m_bFromShootFromGround)
	{
		m_ClipSetRequestHelper.Request(m_ClipSetIdWritheTransition);
	}
	else if (m_bFromGetUp)
	{
		if (pPed && !pPed->IsNetworkClone())
		{
			m_idleClipSetId = GetIdleClipSet(pPed);
		}
		else
		{
			Assert(m_idleClipSetId != CLIP_SET_ID_INVALID);
		}

		m_ClipSetRequestHelper.Request(m_idleClipSetId);
	}
	else
	{
		m_ClipSetRequestHelper.Request(m_ClipSetIdToWrithe);
		iNextState = NEXT_STATE_COLLAPSE;
	}

	if (m_ClipSetRequestHelper.IsLoaded() && m_NetworkHelper.IsNetworkDefStreamedIn(CClipNetworkMoveInfo::ms_NetworkTaskWrithe))
	{
		m_NetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskWrithe);
		if (iNextState == NEXT_STATE_COLLAPSE)
		{
			m_NetworkHelper.SetClipSet(m_ClipSetIdToWrithe, fwMvClipSetVarId("ToWritheClipset",0x2586DD59));

			if(!pPed->IsNetworkClone())
			{
				m_NetworkHelper.SetClip(GetRandClipFromClipSet(m_ClipSetIdToWrithe, m_startClipHash), ms_ToWritheClipId);	
			}
			else
			{
				Assert(m_startClipHash != INVALID_CLIP_HASH);
				m_NetworkHelper.SetClip(GetClipFromClipSet(m_ClipSetIdToWrithe, m_startClipHash), ms_ToWritheClipId);	
			}
		}

		return true;
	}

	return false;
}

void CTaskWrithe::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Finish);
}

CTaskInfo* CTaskWrithe::CreateQueriableState() const
{
	CClonedWritheInfo::InitParams initParams(GetState(), &m_Target, m_bFromGetUp, m_bDieInLoop, m_bFromShootFromGround, m_bEnableRagdollOnCollision, 
		m_fAnimRate, m_DamageEntity, (s16)m_shootFromGroundTimer, m_idleClipSetId, m_idleClipHash, m_startClipHash, m_bForceShootFromGround);
	return rage_new CClonedWritheInfo(initParams);
}

void CTaskWrithe::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	Assert( pTaskInfo && pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_WRITHE );
    CClonedWritheInfo *info = static_cast<CClonedWritheInfo*>(pTaskInfo);

	//---

	m_bFromGetUp			= info->GetFromGetUp();
	m_bDieInLoop			= info->GetDieInLoop();
	m_bFromShootFromGround  = info->GetFromShootFromGround();
	m_bEnableRagdollOnCollision = info->GetEnableRagdollFromCollision();
	m_fAnimRate				= info->GetAnimRate();
	m_shootFromGroundTimer	= info->GetShootFromGroundTime();
	m_bForceShootFromGround = info->GetForceShootFromGround();

	m_DamageEntity			= info->GetDamageEntity();
	m_idleClipSetId			= info->GetClipSetId();
	m_idleClipHash			= info->GetClipHash();

	m_startClipHash			= info->GetStartClipHash();

	//---

	info->GetWeaponTargetHelper().ReadQueriableState(m_Target);
	if(info->GetWeaponTargetHelper().HasTargetEntity())
	{
		SetTargetOutOfScopeID(info->GetWeaponTargetHelper().GetTargetEntityId());
	}

	//---

	CTaskFSMClone::ReadQueriableState(pTaskInfo);

	return;
}

bool CTaskWrithe::IsInScope(const CPed* pPed)
{
	Assert(pPed->IsNetworkClone());

	// Players' don't send over target data....
	if(!pPed->IsPlayer())
	{
		ObjectId targetId = GetTargetOutOfScopeID();
		CNetObjPed::UpdateNonPlayerCloneTaskTarget(pPed, m_Target, targetId);
		SetTargetOutOfScopeID(targetId);
	}
	else
	{
		return CTaskFSMClone::IsInScope(pPed);
	}

	return true;
}

bool CTaskWrithe::IsInCombat() const
{
	switch(GetState())
	{
	case(State_ToShootFromGround):
	case(State_ShootFromGround):
		return true;
	}

	return false;
}

dev_float CTaskWrithe::ms_fMaxDistToValidTarget = 20.0f;
void CTaskWrithe::UpdateTarget()
{
	// Dont run on clones, m_target object is synced and everything should be handled on the local ped
	if (GetPed()->IsNetworkClone())
		return;

	if (!GetIsTargetValid(m_Target.GetEntity()))
		m_Target.Clear();

	if (!m_Target.GetIsValid())
	{
		CPedTargetting* pPedTargetting = GetPed()->GetPedIntelligence()->GetTargetting();
		if (pPedTargetting)
		{
			pPedTargetting->SetInUse();

			FindAndRegisterThreats(GetPed(), pPedTargetting);

			if (GetIsTargetValid(pPedTargetting->GetBestTarget()))
				m_Target.SetEntity(pPedTargetting->GetBestTarget());
			else if (GetIsTargetValid(pPedTargetting->GetSecondBestTarget()))
				m_Target.SetEntity(pPedTargetting->GetSecondBestTarget());
		}
	}
}

bool CTaskWrithe::GetIsTargetValid(const CEntity* pTarget) const
{	
	if (!pTarget)
		return false;

	// Check if target is too far away
	Vector3 vTargetPos = VEC3V_TO_VECTOR3(pTarget->GetTransform().GetPosition());
	float fDist = vTargetPos.Dist(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()));

	if (fDist > ms_fMaxDistToValidTarget)
		return false;

	CPedTargetting* pPedTargetting = GetPed()->GetPedIntelligence()->GetTargetting();

	// Check if we have clear LOS to the target
	if( pPedTargetting)
	{
		CSingleTargetInfo* pTargetInfo =  pPedTargetting->FindTarget(pTarget);
		if( pTargetInfo )
		{
			LosStatus losStatus = pTargetInfo->GetLosStatus();
			if( ( losStatus == Los_blocked ) || ( losStatus == Los_unchecked ) || (losStatus == Los_blockedByFriendly) )
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool SortArrayThreats(const CSpatialArray::FindResult& a, const CSpatialArray::FindResult& b)
{
	return PositiveFloatLessThan_Unsafe(a.m_DistanceSq, b.m_DistanceSq);
}

void CTaskWrithe::FindAndRegisterThreats(CPed* pPed, CPedTargetting* pPedTargeting)
{
	if(pPed)
	{
		// Use the spatial array to get our peds within the radius and then sort them by distance
		const int maxNumPeds = 32;
		CSpatialArray::FindResult results[maxNumPeds];

		int numFound = CPed::ms_spatialArray->FindInSphere(pPed->GetTransform().GetPosition(), ms_fMaxDistToValidTarget, &results[0], maxNumPeds);
		if(numFound)
		{
			std::sort(results, results + numFound, SortArrayThreats);
		}

		for(u32 i = 0; i < numFound; i++)
		{
			CPed* pTargetPed = CPed::GetPedFromSpatialArrayNode(results[i].m_Node);
			if (pTargetPed != pPed)
			{
				TUNE_GROUP_BOOL(__WRITHEBUG, bThreatened, true);
				if( (pPed->GetPedIntelligence()->IsThreatenedBy(*pTargetPed) || bThreatened) && 
					pPed->GetPedIntelligence()->CanAttackPed(pTargetPed))
				{
					pPedTargeting->RegisterThreat(pTargetPed, true, true);
				}
			}
		}
	}
}

void CTaskWrithe::TaskSetState(u32 const iState)
{
	aiTask::SetState(iState);
}

void CTaskWrithe::UpdateClonedSubTasks(CPed* pPed, int const iState)
{
	CTaskTypes::eTaskType expectedTaskType = CTaskTypes::TASK_INVALID_ID;
	switch(iState)
	{
		case State_ShootFromGround:
			{
				// if the ped is unarmed, the task_gun will bail immediately 
				// on the owner so won't exist in the QI so the clone will assert
				// if we try to clone the task so we just don't bother setting it off.
				if(pPed && pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetIsArmed())
				{
					expectedTaskType = CTaskTypes::TASK_GUN; 
				}
			}
			break;
		default:
			return;
	}

	if(expectedTaskType != CTaskTypes::TASK_INVALID_ID)
	{
		 /*CTaskFSMClone::*/CreateSubTaskFromClone(pPed, expectedTaskType);
	}
}

#if DEBUG_DRAW
void CTaskWrithe::DebugCloneTaskInformation(void)
{
	if(GetPed())
	{
		char buffer[2056] = "\0";

		sprintf(buffer, "non-cloned: AnimFinished = %d\nstate %d\ntarget %p\nfrom get up %d\nforce shoot from ground %d\ndie in loop %d\nfrom shoot from ground %d\nanim rate %f\nshoot from ground timer %d\nidleClipSetId %s\nidleclipHash %d\nstart clip hash %d\n",
		m_bAnimFinished,
		GetState(),
		m_Target.GetEntity(),
		m_bFromGetUp,
		m_bForceShootFromGround,
		m_bDieInLoop,
		m_bFromShootFromGround,
		m_fAnimRate,
		m_shootFromGroundTimer,
		m_idleClipSetId.GetCStr(),
		m_idleClipHash,
		m_startClipHash
		);

		grcDebugDraw::Text(VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition()), Color_white, buffer, false);
	}
}
#endif /* DEBUG_DRAW */

CTask::FSM_Return CTaskWrithe::UpdateClonedFSM(const s32 iState, const FSM_Event iEvent)
{
	if(iEvent == OnUpdate)
	{
		// wait for the network to get slightly ahead of us so we've got the synced data we need to use....
		if((GetStateFromNetwork() == State_Start) && (GetState() == State_Start))
		{
			return FSM_Continue;
		}

		// the owner / clone can get out of sync based on when a ped comes into scope so to sync shooting, 
		// we break in if the owner is shooting and we're in an appropriate state to do so...
		if(((GetStateFromNetwork() == State_ShootFromGround) || (GetStateFromNetwork() == State_ToShootFromGround)) && (GetState() == State_Loop))
		{
			// Stream in the clips first in case we've just been created by coming into scope...
			TaskSetState(State_StreamGroundClips);
			return FSM_Continue;
		}

		if((GetStateFromNetwork() == State_Finish) && (GetState() != State_Finish))
		{
			TaskSetState(State_Finish);
			return FSM_Continue;
		}

		UpdateClonedSubTasks(GetPed(), iState);
	}

	return UpdateFSM(iState, iEvent);
}

CTaskFSMClone* CTaskWrithe::CreateTaskForClonePed(CPed* UNUSED_PARAM(pPed))
{
	// Create a new task but set it so it starts from get up / lying on the floor position - Writhe should never start from standing a second time
	CTaskWrithe* newTask		= rage_new CTaskWrithe(m_Target, true, m_bEnableRagdollOnCollision);

	newTask->m_idleClipHash			= m_idleClipHash;
	newTask->m_idleClipSetId		= m_idleClipSetId;
	newTask->m_fAnimRate			= m_fAnimRate;
	newTask->m_DamageEntity			= m_DamageEntity;
	newTask->m_bGoStraightToLoop	= GetState() == State_Loop;

	m_bLeaveMoveNetwork = true;

	return newTask;
}

CTaskFSMClone* CTaskWrithe::CreateTaskForLocalPed(CPed* pPed)
{
	return CreateTaskForClonePed(pPed);
}

bool  CTaskWrithe::ControlPassingAllowed(CPed* pPed, const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	if (pPed && pPed->IsNetworkClone() && !m_cloneTaskInScope)
	{
		return true;
	}

	// only pass control if we're on the ground hunched up
	return (GetState() == State_Loop);
}

void  CTaskWrithe::HandleCloneToLocalSwitch(CPed* pPed)
{
	if (m_bEnableRagdollOnCollision)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DieWhenRagdoll, true);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceStandardBumpReactionThresholds, true);

#if !__FINAL
		if (CPed::ms_bActivateRagdollOnCollision)
#endif
		{
			if (!pPed->GetUsingRagdoll() && !pPed->GetActivateRagdollOnCollision())
			{
				CEventSwitch2NM event(10000, rage_new CTaskNMHighFall(1000, NULL, CTaskNMHighFall::HIGHFALL_SLOPE_SLIDE));
				pPed->SetActivateRagdollOnCollisionEvent(event);
				pPed->SetRagdollOnCollisionAllowedPenetration(RAGDOLL_ALLOWED_PENETRATION);

				if (GetState() > State_ToWrithe)
				{
					pPed->SetActivateRagdollOnCollision(true);
				}
			}
		}
	}
}

void  CTaskWrithe::HandleLocalToCloneSwitch(CPed* pPed)
{
	if (m_bEnableRagdollOnCollision)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DieWhenRagdoll, false);
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_ForceStandardBumpReactionThresholds, false);

#if !__FINAL
		if (CPed::ms_bActivateRagdollOnCollision)
#endif
		{
			if (pPed->GetActivateRagdollOnCollision())
			{
				pPed->SetActivateRagdollOnCollision(false);
			}
		}
	}
}

CClonedWritheInfo::InitParams::InitParams(u32 state, CWeaponTarget const* weaponTarget, bool bFromGetUp, bool bDieInLoop, bool bFromShootFromGround, bool bEnableRagdollFromCollision, 
										  float fAnimRate, ObjectId damageEntity, s16 shootFromGroundTimer, fwMvClipSetId loopClipSetId, s32 loopClipHash, s32 startClipHash, bool bForceShootFromGround)	
:
	m_state(state),
	m_weaponTarget(weaponTarget),
	m_bDieInLoop(bDieInLoop),
	m_bFromShootFromGround(bFromShootFromGround),
	m_bFromGetUp(bFromGetUp),
	m_bEnableRagdollFromCollision(bEnableRagdollFromCollision),
	m_fAnimRate(fAnimRate),
	m_damageEntity(damageEntity),
	m_shootFromGroundTimer(shootFromGroundTimer),
	m_idleClipSetId(loopClipSetId),
	m_idleClipHash(loopClipHash),
	m_startClipHash(startClipHash),
	m_bForceShootFromGround(bForceShootFromGround)
{}

CClonedWritheInfo::CClonedWritheInfo(InitParams const& initParams)
:
  m_bFromGetUp(initParams.m_bFromGetUp)
, m_bDieInLoop(initParams.m_bDieInLoop)
, m_bFromShootFromGround(initParams.m_bFromShootFromGround)
, m_bEnableRagdollOnCollision(initParams.m_bEnableRagdollFromCollision)
, m_fAnimRate(initParams.m_fAnimRate)
, m_damageEntity(initParams.m_damageEntity)
, m_shootFromGroundTimer(initParams.m_shootFromGroundTimer)
, m_idleClipSetId(initParams.m_idleClipSetId)
, m_idleClipHash(initParams.m_idleClipHash)
, m_startClipHash(initParams.m_startClipHash)
, m_weaponTargetHelper(initParams.m_weaponTarget)
, m_bForceShootFromGround(initParams.m_bForceShootFromGround)
{
	SetStatusFromMainTaskState(initParams.m_state);
}

CClonedWritheInfo::CClonedWritheInfo()
: 
	m_bFromGetUp(false),
	m_bDieInLoop(false),
	m_bFromShootFromGround(false),
	m_bEnableRagdollOnCollision(true),
	m_fAnimRate(0.0f),
	m_damageEntity(NETWORK_INVALID_OBJECT_ID),
	m_shootFromGroundTimer(-1),
	m_idleClipSetId(CLIP_SET_ID_INVALID),
	m_idleClipHash(CLIP_ID_INVALID),
	m_startClipHash(CLIP_ID_INVALID),
	m_bForceShootFromGround(false)
{}

CTaskFSMClone* CClonedWritheInfo::CreateCloneFSMTask()
{
	CAITarget target; 
	m_weaponTargetHelper.UpdateTarget(target);

	CTaskWrithe* task = rage_new CTaskWrithe(target, m_bFromGetUp, m_bEnableRagdollOnCollision, m_bForceShootFromGround);
	if(task)
	{
		if(m_weaponTargetHelper.HasTargetEntity())
		{
			task->SetTargetOutOfScopeID(m_weaponTargetHelper.GetTargetEntityId());
		}

		if(m_damageEntity != NETWORK_INVALID_OBJECT_ID)
		{
			task->SetInflictorID(m_damageEntity);
		}
	}

	return task;
}






