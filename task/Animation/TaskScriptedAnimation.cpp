
#include "TaskScriptedAnimation.h"

//	rage includes

//	game includes
#include "animation/debug/AnimViewer.h"
#include "animation/MovePed.h"
#include "animation/MoveObject.h"
#include "animation/MoveVehicle.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/CamInterface.h"
#include "control/replay/replay.h"
#include "event/event.h"
#include "event/EventDamage.h"
#include "ik/solvers/ArmIkSolver.h"
#include "network/objects/entities/NetObjGame.h"
#include "peds/PedFlagsMeta.h"
#include "peds/PedIntelligence.h"
#include "peds/Ped.h"
#include "renderer/HierarchyIds.h"
#include "task/Default/TaskPlayer.h"
#include "task/motion/locomotion/taskmotionped.h"
#include "task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "vehicleAi/VehicleAILodManager.h"
#include "vehicles/door.h"
#include "weapons/info/weaponinfo.h"
#include "system/appcontent.h"

AI_OPTIMISATIONS()

static const float	TASK_SCRIPTED_ANIMATION_MAX_CLONE_BLEND_IN_DURATION = 0.125f;
static const u32	TASK_SCRIPTED_ANIMATION_KEEP_INIT_DATA_SLOT	= 0;

const eScriptedAnimFlagsBitSet CTaskScriptedAnimation::AF_NONE;
const eIkControlFlagsBitSet CTaskScriptedAnimation::AIK_NONE;

//#include "system/findsize.h"
//FindSize(CClonedScriptedAnimationInfo);  //160

CTaskScriptedAnimation::CTaskScriptedAnimation( float blendInTime, float blendOutTime )
: m_blendInDuration(blendInTime)
, m_blendOutDuration(blendOutTime)
, m_dict((u32)0)
, m_clip((u32)0)
, m_fpsDict((u32)0)
, m_fpsClip((u32)0)
, m_cloneLeaveMoveNetwork(false)
, m_bAllowOverrideCloneUpdate(false)
, m_initialPosition(Vector3::ZeroType)
, m_initialOrientation(Quaternion::IdentityType)
, m_physicsSettings(0)
, m_firstUpdate(false)
, m_exitNextUpdate(false)
, m_physicsStateChangesApplied(false)
, m_hasSentDeathEvent(false)
, m_animateJoints(false)
, m_startPhase(0.0f)
, m_recreateWeapon(false)
, m_bForceResendSyncData(false)
, m_bMPHasStoredInitSlotData(false)
, m_bTaskSecondary(false)
, m_bUsingFpsClip(false)
, m_bClipsAreLoading(false)
, m_bOneShotFacialBlocked(false)
, m_owningAnimScene(ANIM_SCENE_INST_INVALID)
{
#if FPS_MODE_SUPPORTED
	m_apFirstPersonIkHelper[0] = m_apFirstPersonIkHelper[1] = NULL;
#endif // FPS_MODE_SUPPORTED

	SetInternalTaskType(CTaskTypes::TASK_SCRIPTED_ANIMATION);
}

CTaskScriptedAnimation::CTaskScriptedAnimation( InitSlotData& priorityLow, InitSlotData& priorityMid, InitSlotData& priorityHigh, float blendInTime, float blendOutTime, bool clonedTask )
: m_blendInDuration(blendInTime)
, m_blendOutDuration(blendOutTime)
, m_dict((u32)0)
, m_clip((u32)0)
, m_fpsDict((u32)0)
, m_fpsClip((u32)0)
, m_cloneLeaveMoveNetwork(false)
, m_bAllowOverrideCloneUpdate(false)
, m_initialPosition(Vector3::ZeroType)
, m_initialOrientation(Quaternion::IdentityType)
, m_physicsSettings(0)
, m_firstUpdate(false)
, m_exitNextUpdate(false)
, m_physicsStateChangesApplied(false)
, m_hasSentDeathEvent(false)
, m_animateJoints(false)
, m_startPhase(0.0f)
, m_recreateWeapon(false)
, m_bForceResendSyncData(false)
, m_bMPHasStoredInitSlotData(false)
, m_bTaskSecondary(false)
, m_bUsingFpsClip(false)
, m_bClipsAreLoading(false)
, m_bOneShotFacialBlocked(false)
, m_owningAnimScene(ANIM_SCENE_INST_INVALID)
{
#if FPS_MODE_SUPPORTED
	m_apFirstPersonIkHelper[0] = m_apFirstPersonIkHelper[1] = NULL;
#endif // FPS_MODE_SUPPORTED

	if (priorityLow.state!=kStateEmpty)
	{
		if(clonedTask)
		{
			u32 dictHashSlot[3];

			for(int i=0;i<MAX_CLIPS_PER_PRIORITY;i++)
			{
				dictHashSlot[i] = priorityLow.clipData[i].dict.GetHash();
			}
			CheckAndRequestSyncDictionary(dictHashSlot,MAX_CLIPS_PER_PRIORITY);
			GetSlot(kPriorityLow).CacheInitDataWithoutRef(priorityLow);  //Directly set pointer as reference is handled by CheckAndRequestSyncDictionary now.
		}
		else
		{
			GetSlot(kPriorityLow).CacheInitData(priorityLow);
		}
	}
	if (priorityMid.state!=kStateEmpty)
	{
		taskAssertf(!clonedTask,"Don't expect mid priority slot to be used on clones");
		GetSlot(kPriorityMid).CacheInitData(priorityMid);
	}
	if (priorityHigh.state!=kStateEmpty)
	{
		taskAssertf(!clonedTask,"Don't expect high priority slot to be used on clones");
		GetSlot(kPriorityHigh).CacheInitData(priorityHigh);
	}
	SetInternalTaskType(CTaskTypes::TASK_SCRIPTED_ANIMATION);

	InitClonesStartPhaseFromBlendedClipsData();
}

CTaskScriptedAnimation::CTaskScriptedAnimation( ScriptInitSlotData& priorityLow, ScriptInitSlotData& priorityMid, ScriptInitSlotData& priorityHigh, float blendInTime, float blendOutTime )
: m_blendInDuration(blendInTime)
, m_blendOutDuration(blendOutTime)
, m_dict((u32)0)
, m_clip((u32)0)
, m_fpsDict((u32)0)
, m_fpsClip((u32)0)
, m_cloneLeaveMoveNetwork(false)
, m_bAllowOverrideCloneUpdate(false)
, m_initialPosition(Vector3::ZeroType)
, m_initialOrientation(Quaternion::IdentityType)
, m_physicsSettings(0)
, m_firstUpdate(false)
, m_exitNextUpdate(false)
, m_physicsStateChangesApplied(false)
, m_hasSentDeathEvent(false)
, m_animateJoints(false)
, m_startPhase(0.0f)
, m_recreateWeapon(false)
, m_bForceResendSyncData(false)
, m_bMPHasStoredInitSlotData(false)
, m_bTaskSecondary(false)
, m_bUsingFpsClip(false)
, m_bClipsAreLoading(false)
, m_bOneShotFacialBlocked(false)
, m_owningAnimScene(ANIM_SCENE_INST_INVALID)
{
#if FPS_MODE_SUPPORTED
	m_apFirstPersonIkHelper[0] = m_apFirstPersonIkHelper[1] = NULL;
#endif // FPS_MODE_SUPPORTED

	if (priorityLow.state.Int!=kStateEmpty)
	{
		GetSlot(kPriorityLow).CacheInitData(priorityLow);
	}
	if (priorityMid.state.Int!=kStateEmpty)
	{
		GetSlot(kPriorityMid).CacheInitData(priorityMid);
	}
	if (priorityHigh.state.Int!=kStateEmpty)
	{
		GetSlot(kPriorityHigh).CacheInitData(priorityHigh);
	}
	SetInternalTaskType(CTaskTypes::TASK_SCRIPTED_ANIMATION);

	InitClonesStartPhaseFromBlendedClipsData();
}

CTaskScriptedAnimation::CTaskScriptedAnimation
(
	atHashWithStringNotFinal clipDictName, 
	atHashWithStringNotFinal clipName, 
	ePlaybackPriority priority, 
	atHashWithStringNotFinal filter, 
	float blendInDuration, 
	float blendOutDuration,
	s32 timeToPlay, 
	eScriptedAnimFlagsBitSet flags, 
	float startPhase,
	bool clonedTask,
	bool phaseControlled,
	eIkControlFlagsBitSet ikFlags,
	bool bAllowOverrideCloneUpdate,
	bool bPartOfASequence

)
:	m_blendInDuration(blendInDuration)
,	m_blendOutDuration(blendOutDuration)
, m_dict((u32)0)
, m_clip((u32)0)
, m_fpsDict((u32)0)
, m_fpsClip((u32)0)
, m_CloneData(startPhase, phaseControlled)
, m_cloneLeaveMoveNetwork(false)
, m_bAllowOverrideCloneUpdate(bAllowOverrideCloneUpdate)
, m_initialPosition(Vector3::ZeroType)
, m_initialOrientation(Quaternion::IdentityType)
, m_physicsSettings(0)
, m_firstUpdate(false)
, m_exitNextUpdate(false)
, m_physicsStateChangesApplied(false)
, m_hasSentDeathEvent(false)
, m_animateJoints(false)
, m_startPhase(startPhase)
, m_recreateWeapon(false)
, m_bForceResendSyncData(false)
, m_bMPHasStoredInitSlotData(false)
, m_bTaskSecondary(false)
, m_bUsingFpsClip(false)
, m_bClipsAreLoading(false)
, m_bOneShotFacialBlocked(false)
, m_owningAnimScene(ANIM_SCENE_INST_INVALID)
{
	taskAssertf(!NetworkInterface::IsGameInProgress() || m_CloneData.m_cloneStartClipPhase<=1.0f,"m_CloneData.m_cloneStartClipPhase  %f out of range",m_CloneData.m_cloneStartClipPhase);

#if FPS_MODE_SUPPORTED
	m_apFirstPersonIkHelper[0] = m_apFirstPersonIkHelper[1] = NULL;
#endif // FPS_MODE_SUPPORTED

	// grab the clip and cache it in the slot data
	InitSlotData data;
	m_dict = clipDictName;
	m_clip = clipName;

	data.clipData[0].dict = clipDictName;
	data.clipData[0].clip = clipName;
	data.clipData[0].phase = startPhase;

	if(clonedTask && phaseControlled)
	{
		data.clipData[0].rate = 0.0f;
	}

	data.blendInDuration = blendInDuration;
	data.blendOutDuration = blendOutDuration;
	data.flags = flags;
	data.ikFlags = ikFlags;
	data.timeToPlay = timeToPlay;
	data.filter = filter;
	data.state = kStateSingleClip;

	if(clonedTask)
	{
		taskAssertf(priority==TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT,"Dont expect clones to use %d priority",priority);
		m_bMPHasStoredInitSlotData = true;
		u32 dictHash = clipDictName.GetHash();
		CheckAndRequestSyncDictionary(&dictHash,1);
		GetSlot(priority).CacheInitDataWithoutRef(data);  //Directly set pointer as reference is handled by CheckAndRequestSyncDictionary now.
	}
	else
	{
		if(NetworkInterface::IsGameInProgress() && bPartOfASequence )
		{
			GetSlot(priority).CacheInitDataWithoutRef(data);
		}
		else
		{
			GetSlot(priority).CacheInitData(data);
		}
	}

	SetInternalTaskType(CTaskTypes::TASK_SCRIPTED_ANIMATION);
}

CTaskScriptedAnimation::CTaskScriptedAnimation
(
 atHashWithStringNotFinal clipDictName, 
 atHashWithStringNotFinal clipName, 
 ePlaybackPriority priority, 
 atHashWithStringNotFinal filter, 
 float blendInDuration, 
 float blendOutDuration,
 s32 timeToPlay, 
 eScriptedAnimFlagsBitSet flags, 
 float startPhase,
 const Vector3 &vInitialPosition,
 const Quaternion &InitialOrientationQuaternion,
 bool clonedTask,
 bool phaseControlled,
 eIkControlFlagsBitSet ikFlags,
 bool bAllowOverrideCloneUpdate,
 bool bPartOfASequence
 )
 :	m_blendInDuration(blendInDuration)
 ,	m_blendOutDuration(blendOutDuration)
 , m_dict((u32)0)
 , m_clip((u32)0)
 , m_fpsDict((u32)0)
 , m_fpsClip((u32)0)
 , m_CloneData(startPhase, phaseControlled)
 , m_cloneLeaveMoveNetwork(false)
 , m_bAllowOverrideCloneUpdate(bAllowOverrideCloneUpdate)
 , m_initialPosition(vInitialPosition)
 , m_initialOrientation(InitialOrientationQuaternion)
 , m_physicsSettings(PS_APPLY_REORIENTATION | PS_APPLY_REPOSITION)
 , m_firstUpdate(false)
 , m_exitNextUpdate(false)
 , m_physicsStateChangesApplied(false)
 , m_hasSentDeathEvent(false)
 , m_animateJoints(false)
 , m_startPhase(startPhase)
  , m_recreateWeapon(false)
  , m_bForceResendSyncData(false)
  , m_bMPHasStoredInitSlotData(false)
  , m_bTaskSecondary(false)
  , m_bUsingFpsClip(false)
  , m_bClipsAreLoading(false)
  , m_bOneShotFacialBlocked(false)
  , m_owningAnimScene(ANIM_SCENE_INST_INVALID)
{
	taskAssertf(!NetworkInterface::IsGameInProgress() || m_CloneData.m_cloneStartClipPhase<=1.0f,"m_CloneData.m_cloneStartClipPhase %f out of range",m_CloneData.m_cloneStartClipPhase);

#if FPS_MODE_SUPPORTED
	m_apFirstPersonIkHelper[0] = m_apFirstPersonIkHelper[1] = NULL;
#endif // FPS_MODE_SUPPORTED

	// grab the clip and cache it in the slot data
	InitSlotData data;
	m_dict = clipDictName;
	m_clip = clipName;

	data.clipData[0].dict = clipDictName;
	data.clipData[0].clip = clipName;
	data.clipData[0].phase = startPhase;

	if(clonedTask && phaseControlled)
	{
		data.clipData[0].rate = 0.0f;
	}

	data.blendInDuration = blendInDuration;
	data.blendOutDuration = blendOutDuration;
	data.flags=flags;
	data.ikFlags=ikFlags;
	data.timeToPlay = timeToPlay;
	data.filter = filter;
	data.state = kStateSingleClip;

	if(clonedTask)
	{
		taskAssertf(priority==TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT,"Dont expect clones to use %d priority",priority);
		m_bMPHasStoredInitSlotData = true;
		u32 dictHash = clipDictName.GetHash();
		CheckAndRequestSyncDictionary(&dictHash,1);
		GetSlot(priority).CacheInitDataWithoutRef(data);  //Directly set pointer as reference is handled by CheckAndRequestSyncDictionary now.
	}
	else
	{
		if(NetworkInterface::IsGameInProgress() && bPartOfASequence )
		{
			GetSlot(priority).CacheInitDataWithoutRef(data);
		}
		else
		{
			GetSlot(priority).CacheInitData(data);
		}
	}

	SetInternalTaskType(CTaskTypes::TASK_SCRIPTED_ANIMATION);
}


CTaskScriptedAnimation::~CTaskScriptedAnimation()
{
	if(m_bCloneDictRefAdded)
	{
		const InitSlotData* pData = GetSlot(TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT).GetInitData();

		if(pData)
		{
			if(pData->state==kStateSingleClip)
			{
				strLocalIndex clipDictIndex = fwAnimManager::FindSlotFromHashKey(m_dict.GetHash());
				if(clipDictIndex.Get() >-1)
				{
					fwAnimManager::RemoveRefWithoutDelete(clipDictIndex);
				}	
			}
			else if(pData->state==kStateBlend)
			{
				for(int i=0;i<MAX_CLIPS_PER_PRIORITY;i++)
				{
					strLocalIndex clipDictIndex = fwAnimManager::FindSlotFromHashKey(pData->clipData[i].dict.GetHash());
					if(clipDictIndex.Get() >-1)
					{
						fwAnimManager::RemoveRefWithoutDelete(clipDictIndex);
					}	
				}
			}
		}
	}

#if FPS_MODE_SUPPORTED
	for (int helper = 0; helper < 2; ++helper)
	{
		if (m_apFirstPersonIkHelper[helper])
		{
			delete m_apFirstPersonIkHelper[helper];
			m_apFirstPersonIkHelper[helper] = NULL;
		}
	}
#endif // FPS_MODE_SUPPORTED
}


CTask::FSM_Return CTaskScriptedAnimation::ProcessPreFSM()
{
	if (GetPhysical())
	{
		if (GetPhysical()->GetIsTypePed())
		{
			CPed* pPed = GetPed();

			// don't allow ai timeslice lodding during scripted animations.
			pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);
			pPed->GetPedAiLod().SetBlockedLodFlag(CPedAILod::AL_LodTimesliceIntelligenceUpdate | CPedAILod::AL_LodTimesliceAnimUpdate);

			pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPostMovement, true);

			if (IsFlagSet(AF_NOT_INTERRUPTABLE))
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockAnimatedWeaponReactions, true);
			}

			if (IsFlagSet(AF_ABORT_ON_PED_MOVEMENT))
			{
				// if this is the primary task, and we're using the abort on ped movement
				// flag, kick off a player control task to get movement intentions
				if (pPed->IsPlayer())
				{
					CTask* pTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
					if (pTask && pTask->GetTaskType()==CTaskTypes::TASK_MOVE_PLAYER)
					{
						// Mark the movement task as still in use this frame
						CTaskMoveInterface* pMoveInterface = pTask->GetMoveInterface();
						if (pMoveInterface)
						{
							pMoveInterface->SetCheckedThisFrame(true);
	#if !__FINAL
							pMoveInterface->SetOwner(this);
	#endif
						}
					}
				}


			}
		}
		else if (GetPhysical()->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = GetVehicle();

			// don't allow ai timeslice lodding during scripted animations.
			CVehicleAILodManager::DisableTimeslicingImmediately(*pVehicle);
		}

	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskScriptedAnimation::ProcessPostFSM( void )
{
	if(m_bForceResendSyncData)
	{
		if( GetEntity() && 
			(GetEntity()->GetType()==ENTITY_TYPE_PED) &&
			!GetPed()->IsNetworkClone() &&
			GetPed()->GetNetworkObject() &&
			GetPed()->IsPlayer() &&
			GetPed()->GetNetworkObject()->GetSyncTree())
		{
			if (GetPed()->GetNetworkObject()->HasSyncData())
			{
				CPlayerSyncTree *playerSyncTree = (CPlayerSyncTree *)GetPed()->GetNetworkObject()->GetSyncTree();
				playerSyncTree->ForceSendOfNodeData(SERIALISEMODE_UPDATE, GetPed()->GetNetworkObject()->GetActivationFlags(), GetPed()->GetNetworkObject(), *playerSyncTree->GetPlayerAppearanceNode());
			}
		}		

		m_bForceResendSyncData = false;
	}

	if (GetEntity() && GetEntity()->GetType()!=ENTITY_TYPE_PED && !GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		// run over the slots and handle mover extraction for non peds
		// we need to do this here as objects and vehicles don't get ProcessPostMovement calls
		for (u8 i=0; i<kNumPriorities; i++)
		{
			if (GetSlot(i).GetState()!=kStateEmpty && GetSlot(i).GetData().IsFlagSet(AF_USE_MOVER_EXTRACTION))
			{
				ApplyMoverTrackFixup(GetPhysical(), (ePlaybackPriority)i);
			}
		}
	}

	return FSM_Continue;
}

bool CTaskScriptedAnimation::ProcessPostMovement()
{
	if (!GetIsFlagSet(aiTaskFlags::IsAborted))
	{
		// run over the 
		for (u8 i=0; i<kNumPriorities; i++)
		{
			if (GetSlot(i).GetState()!=kStateEmpty && GetSlot(i).GetData().IsFlagSet(AF_USE_MOVER_EXTRACTION))
			{
				ApplyMoverTrackFixup(GetPhysical(), (ePlaybackPriority)i);
				return true;
			}
		}
	}
	
	return false;
}

void CTaskScriptedAnimation::ProcessPreRender2()
{
	if (GetPhysical() && GetPhysical()->GetIsTypePed())
	{
		CPed* pPed = GetPed();
		for (u8 i=0; i<kNumPriorities; i++)
		{
			if (GetSlot(i).GetState()!=kStateEmpty)
			{
				CPriorityData& data = GetSlot(i).GetData();
				ProcessIkControlFlagsPreRender(*pPed, data.GetIkFlags());

			#if FPS_MODE_SUPPORTED
				if (GetState() == State_Running)
				{
					ProcessFirstPersonIk(data.GetIkFlags());
			}
			#endif // FPS_MODE_SUPPORTED
		}
	}
	}
}


CTask::FSM_Return CTaskScriptedAnimation::UpdateFSM( const s32 iState, const FSM_Event iEvent )
{
	FSM_Begin
		FSM_State(State_Start)
			FSM_OnEnter
				Start_OnEnter();
			FSM_OnUpdate
				return Start_OnUpdate();

		FSM_State(State_Running)
			FSM_OnEnter
				Running_OnEnter();
			FSM_OnUpdate
				return Running_OnUpdate();
			FSM_OnExit
				Running_OnExit();

		FSM_State(State_Exit)
			FSM_OnUpdate
				return FSM_Quit;
	FSM_End
}

void CTaskScriptedAnimation::DoAbort(const AbortPriority UNUSED_PARAM(priority), const aiEvent* UNUSED_PARAM(pEvent))
{
	// don't abort if we were about to exit anyway
	if (!m_exitNextUpdate)
	{
		RemoveEntityFromControllingAnimScene();
	}	
}

bool CTaskScriptedAnimation::ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent)
{
	if(pEvent && IsFlagSet(AF_ABORT_ON_WEAPON_DAMAGE))
	{
		switch (static_cast< const CEvent * >(pEvent)->GetEventType())
		{
		case EVENT_DAMAGE:
		case EVENT_ON_FIRE:
			{
				animTaskDebugf(this, "Aborting for weapon damage - event: %s", pEvent ? ((CEvent*)pEvent)->GetName() : "none");
				return true;
			}
			break;
		}
	}

	// special clip that has been set by the script, and can only be aborted by the script
	// or by itself (or directly by another task, so not any events is what I mean, ok.)
	if(!(CTask::ABORT_PRIORITY_IMMEDIATE==iPriority) 
		&& IsFlagSet(AF_NOT_INTERRUPTABLE) 
		&& pEvent 
		&& !((CEvent*)pEvent)->RequiresAbortForRagdoll() 
		&& ((CEvent*)pEvent)->GetEventType()!= EVENT_SCRIPT_COMMAND
		&& ((CEvent*)pEvent)->GetEventType() != EVENT_GIVE_PED_TASK
		&& !((((CEvent*)pEvent)->GetEventType() == EVENT_DEATH) && m_hasSentDeathEvent)
		)
		// stealth kills use the give ped task, this ensures peds who are playing clips will abort correctly
	{
		return false;
	}

	if (GetSubTask() && !GetSubTask()->MakeAbortable(iPriority, pEvent))
	{
		return false;
	}

	if(CTask::ABORT_PRIORITY_URGENT==iPriority || CTask::ABORT_PRIORITY_IMMEDIATE==iPriority)
	{
		// Base class
		return CTask::ShouldAbort(iPriority, pEvent);
	}

	return false;
}

void CTaskScriptedAnimation::StartPlayback(CTaskScriptedAnimation::InitSlotData& data, ePlaybackPriority priority)
{
	switch (data.state)
	{
	case kStateSingleClip:
		// grab the clip from the hashes
		{
			StartSingleClip(
				data.clipData[0].dict, 
				data.clipData[0].clip, 
				priority, 
				data.blendInDuration,
				data.blendOutDuration,
				data.timeToPlay,
				data.flags,
				data.ikFlags
				);

			if(NetworkInterface::IsGameInProgress() && m_CloneData.m_clonePostMigrateStartClipPhase >=0.0f )
			{
				SetPhase(m_CloneData.m_clonePostMigrateStartClipPhase, priority, 0);
			}
			else
			{
				SetPhase(data.clipData[0].phase, priority, 0);
			}

			SetRate(data.clipData[0].rate, priority, 0);
			SetBlend(data.clipData[0].weight, priority, 0);

			if (data.ikFlags.BitSet().IsSet(AIK_LINKED_FACIAL))
			{
				if (BANK_ONLY(CAnimViewer::m_pBank || ) animVerifyf(NetworkInterface::IsGameInProgress(), "Linked facial is only supported in network games!"))
				{
					if (CPed* pPed = (GetPhysical() && GetPhysical()->GetIsTypePed() ? static_cast<CPed*>(GetPhysical()) : nullptr))
					{
						const crClip* pBodyClip = GetSlot(priority).GetData().GetClip(0);
						if (pBodyClip)
						{
							// In DEV builds, it's possible that the clip is being loaded individually via RAG banks or Clip Editor etc,
							// and therefore doesn't have a dictionary owner, so check for that situation here.
							if (pBodyClip->GetDictionary())
							{
								atString facialClipName(pBodyClip->GetName());
								facialClipName.Replace("pack:/", "");
								facialClipName.Replace(".clip", "");
								facialClipName += "_facial";

								const crClip* pLinkedFacialClip = pBodyClip->GetDictionary()->GetClip(facialClipName.c_str());
								if (pLinkedFacialClip)
								{
									CFacialDataComponent *pFacialDataComponent = pPed->GetFacialData();
									if (pFacialDataComponent)
									{
										if (m_bOneShotFacialBlocked)
										{
											pPed->GetMovePed().ClearOneShotFacialBlock();

											m_bOneShotFacialBlocked = false;
										}

										pPed->GetMovePed().PlayOneShotFacialClip(pLinkedFacialClip, m_blendInDuration, true);

										m_bOneShotFacialBlocked = true;
									}
								}
							}
						}
					}
				}
			}

			if (data.filter)
			{
				SetFilter(data.filter.GetHash(), priority);
			}
			else if (data.flags.BitSet().IsSet(AF_UPPERBODY))
			{
				SetFilter((u32)BONEMASK_UPPERONLY, priority);
			}
		}
		break;
	case kStateBlend:
		{
			// start the blend
			StartBlend(
				priority, 
				data.blendInDuration,
				data.blendOutDuration,
				data.timeToPlay,
				data.flags,
				data.ikFlags
				);

			// set the required clips
			for (u8 i=0; i<MAX_CLIPS_PER_PRIORITY; i++)
			{
				if (data.clipData[i].clip.IsNotNull())
				{
					SetClip(data.clipData[i].dict, data.clipData[i].clip, priority, i);
					SetPhase(data.clipData[i].phase, priority, i);
					SetRate(data.clipData[i].rate, priority, i);
					SetBlend(data.clipData[i].weight, priority, i);
				}
			}

			if (data.filter)
			{
				SetFilter(data.filter.GetHash(), priority);
			}
			else if (data.flags.BitSet().IsSet(AF_UPPERBODY))
			{
				SetFilter((u32)BONEMASK_UPPERONLY, priority);
			}
		}
		break;
	case kStateEmpty:
		break;
	}
}

void CTaskScriptedAnimation::ApplyMoverTrackFixup(CPhysical *pPhysical, ePlaybackPriority priority)
{
	if (!pPhysical)
		return;

	// locate the clip to use for the fixup
	const crClip *pClip = NULL;
	
	// mover extraction is currently only supported on single clip playback levels
	switch(GetSlot(priority).GetState())
	{
	case kStateSingleClip:
		{
			pClip = GetSlot(priority).GetData().GetClip(0);
		}
		break;
	default:
		{
			return;
		}
	}

	if (!pClip)
		return;

	float phase = m_firstUpdate ? m_CloneData.m_cloneClipPhase : GetPhase(priority);

	//	fix up the mover position on the dynamic entity
	Matrix34 deltaMatrix(M34_IDENTITY);
	Matrix34 startMatrix(M34_IDENTITY);

	startMatrix.d = m_initialPosition;
	startMatrix.FromQuaternion(m_initialOrientation);

	if (GetSlot(priority).GetData().IsFlagSet(AF_EXTRACT_INITIAL_OFFSET))
	{
		// apply the initial offset from the clip
		fwAnimHelpers::ApplyInitialOffsetFromClip(*pClip, startMatrix);
	}

	// Get the calculated mover track offset from phase 0.0 to the new phase
	fwAnimHelpers::GetMoverTrackMatrixDelta(*pClip, 0.f, phase, deltaMatrix);

	// Transform by the mover offset
	startMatrix.DotFromLeft(deltaMatrix);

	if (m_firstUpdate)
	{	
		pPhysical->GetPortalTracker()->RequestRescanNextUpdate();
		pPhysical->GetPortalTracker()->Update(startMatrix.d);
	}

	// apply to entity
	pPhysical->SetMatrix(startMatrix);

	m_firstUpdate = false;
};

void CTaskScriptedAnimation::ApplyPhysicsStateChanges(CPhysical *pPhysical, const crClip *pClip)
{
	CPed * pPed = pPhysical->GetIsTypePed() ? static_cast<CPed*>(pPhysical) : NULL;
	if(pPhysical->GetIsAttached())
	{
		// Don't effect the entity if it is attached
		// Shouldn't be running an clip with these flags on attached entities
		taskAssertf(!IsFlagSet(AF_OVERRIDE_PHYSICS), "CTaskClip: Do not run with OVERRIDE_PHYSICS on attached entities!");
		ClearFlag(AF_OVERRIDE_PHYSICS);
	}

	// Store the current physics settings for the entity 
	// and apply any changes requested by the task flags

	if (IsFlagSet(AF_OVERRIDE_PHYSICS) || IsFlagSet(AF_USE_MOVER_EXTRACTION))
	{
		if (IsFlagSet(AF_USE_MOVER_EXTRACTION))
		{
			//if a start position has not been provided by TASK_PLAY_CLIP_ADVANCED, get the current position
			if ( !(m_physicsSettings & PS_APPLY_REPOSITION)) 
			{
				m_initialPosition = VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition());
			}
			//if a start orientation has not been provided by TASK_PLAY_CLIP_ADVANCED, get the current orientation
			if ( !(m_physicsSettings & PS_APPLY_REORIENTATION)) 
			{
				m_initialOrientation.FromMatrix34(MAT34V_TO_MATRIX34(pPhysical->GetMatrix()));
			}
		}
	}
	else
	{
		// Force a physics update through to make sure ped ground position is up to date on start unless disable via this flag
		if (pPed && !IsFlagSet(AF_DISABLE_FORCED_PHYSICS_UPDATE))
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ForceProcessPhysicsUpdateEachSimStep, true);
		}
	}

	//reposition and reorient the entity if requested by the task constructor
	if (m_physicsSettings & PS_APPLY_REPOSITION || m_physicsSettings & PS_APPLY_REORIENTATION)
	{
		Matrix34 entityMatrix(M34_IDENTITY);

		Vector3 initialPosition = m_initialPosition;
		Quaternion initialOrientation = m_initialOrientation;

		if (IsFlagSet(AF_EXTRACT_INITIAL_OFFSET))
		{
			// apply the authored offset from the clip
			if(pClip)
			{
				fwAnimHelpers::ApplyInitialOffsetFromClip(*pClip, initialPosition, initialOrientation);
				if (m_startPhase>0.0f)
				{
					Matrix34 mat(M34_IDENTITY);
					Matrix34 deltaMatrix(M34_IDENTITY);
					mat.d = initialPosition;
					mat.FromQuaternion(initialOrientation);
					// Get the calculated mover track offset from phase 0.0 to the new phase
					fwAnimHelpers::GetMoverTrackMatrixDelta(*pClip, 0.f, m_startPhase, deltaMatrix);

					// Transform by the mover offset
					mat.DotFromLeft(deltaMatrix);

					initialPosition = mat.d;
					mat.ToQuaternion(initialOrientation);
				}
			}
		}

		if ( m_physicsSettings & PS_APPLY_REPOSITION )
		{
			entityMatrix.d = initialPosition;
		}
		if ( m_physicsSettings & PS_APPLY_REORIENTATION )
		{
			entityMatrix.FromQuaternion(initialOrientation);
		}

		pPhysical->SetMatrix(entityMatrix);
		pPhysical->GetPortalTracker()->RequestRescanNextUpdate();
	}

	// Object and vehicles - Disable gravity
	if (IsFlagSet(AF_USE_KINEMATIC_PHYSICS) && IsFlagSet(AF_USE_MOVER_EXTRACTION))
	{
		if (pPhysical->GetCurrentPhysicsInst() && (pPhysical->GetIsTypeObject() || pPhysical->GetIsTypeVehicle()))
		{
			pPhysical->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NO_GRAVITY, true);
		}
	}

	m_physicsStateChangesApplied = true;
}

void CTaskScriptedAnimation::RestorePhysicsState(CPhysical* pPhysical)
{
	taskAssert(pPhysical);

	CPed * pPed = pPhysical->GetIsTypePed() ? static_cast<CPed*>(pPhysical) : NULL;

	if (IsFlagSet(AF_OVERRIDE_PHYSICS) && (!pPed || pPed->GetRagdollState()!= RAGDOLL_STATE_PHYS))
	{
		//we need to update the heading of the entity to stop it rotating needlessly at the end of the clip
		Vector3 eulerRotation;
		Matrix34 m = MAT34V_TO_MATRIX34(pPed ? pPed->GetMatrix() : pPhysical->GetMatrix());
		m.ToEulersXYZ(eulerRotation);

		pPhysical->SetHeading(eulerRotation.z);
		if (pPed)
		{
			pPed->SetDesiredHeading(eulerRotation.z);

			pPed->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, false);
			pPed->ResetStandData();
		}
	}

	// Object and vehicles - Re-enable gravity
	if (IsFlagSet(AF_USE_KINEMATIC_PHYSICS) && IsFlagSet(AF_USE_MOVER_EXTRACTION))
	{
		if (pPhysical->GetCurrentPhysicsInst() && (pPhysical->GetIsTypeObject() || pPhysical->GetIsTypeVehicle()))
		{
			pPhysical->GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NO_GRAVITY, false);
		}
	}
}

bool CTaskScriptedAnimation::StartNetworkPlayer()
{
	if (!m_moveNetworkHelper.IsNetworkActive())
	{
		CPhysical * pPhys = GetPhysical();
		if(pPhys && pPhys->GetIsTypeObject() && NetworkInterface::IsGameInProgress() && pPhys->IsNetworkClone())
		{
			CObject* object = SafeCast(CObject, pPhys);
			if(!object->GetDrawable())
			{
				object->InitAnimLazy();
				if(!object->GetDrawable())
					return false;
			}
		}

		if (fwAnimDirector::RequestNetworkDef(CClipNetworkMoveInfo::ms_NetworkTaskScriptedAnimation))
		{
			m_moveNetworkHelper.CreateNetworkPlayer(pPhys, CClipNetworkMoveInfo::ms_NetworkTaskScriptedAnimation);
			return true;
		}
		return false;
	}
	else
	{
		return true;
	}
}

void CTaskScriptedAnimation::Start_OnEnter()
{
	ENABLE_FRAG_OPTIMIZATION_ONLY(animAssert(GetPhysical() && (!GetPhysical()->GetIsTypeVehicle() || GetVehicle()->GetHasFragCacheEntry())));

	if (m_bClipsAreLoading) // this may have been set when the task was created from a clone task, which was still loading the clip. We need to keep doing this.
	{
		RequestCloneClips();
	}
}

CTask::FSM_Return CTaskScriptedAnimation::Start_OnUpdate()
{
	CPhysical* pPhys = GetPhysical();

	// load assets and start the move network

	if(m_bClipsAreLoading)
	{
		if(pPhys->GetIsTypePed() && GetTimeInState() >2.0f)
		{
#if __ASSERT
			const InitSlotData* pData = GetSlot(TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT).GetInitData();

			ePlayBackState playbackState =  kStateEmpty;
			atHashWithStringNotFinal dict;

			if(pData)
			{
				playbackState = pData->state;
				dict = pData->clipData[0].dict;
			}

			taskAssertf(0,"%s: too long waiting for network or clips load at %0.2f Secs. HaveClipsBeenLoaded() %s, StartNetworkPlayer %s, GetCurrentlyRequiredDictIndex %d InitSlotData state %d, clipData[0].dict %s:0x%x",
				GetPed()->GetDebugName(),
				GetTimeInState(),
				m_clipRequest[0].HaveClipsBeenLoaded()?"TRUE":"FALSE",
				StartNetworkPlayer()?"TRUE":"FALSE",
				m_clipRequest[0].GetCurrentlyRequiredDictIndex(),
				playbackState,
				dict.GetCStr(),
				dict.GetHash());

#endif
			return FSM_Quit;
		}

		if(!CloneClipsLoaded())
		{
			return FSM_Continue;
		}

		m_bClipsAreLoading = false;
	}

	if (StartNetworkPlayer())
	{
		if (pPhys->GetIsTypeObject() && pPhys->GetAnimDirector())
		{
			static_cast<CObject*>(pPhys)->GetMoveObject().SetNetwork(m_moveNetworkHelper.GetNetworkPlayer(), m_blendInDuration);
		}
		else if (pPhys->GetIsTypeVehicle())
		{
			CVehicle *pVehicle = static_cast<CVehicle*>(pPhys);
			pVehicle->GetMoveVehicle().SetNetwork(m_moveNetworkHelper.GetNetworkPlayer(), m_blendInDuration);
			m_animateJoints = pVehicle->m_nVehicleFlags.bAnimateJoints;
			pVehicle->m_nVehicleFlags.bAnimateJoints = IsFlagSet(AF_USE_KINEMATIC_PHYSICS);
		}
		else if (pPhys->GetIsTypePed())
		{
			if (IsFlagSet(AF_ADDITIVE))
			{
				static_cast<CPed*>(pPhys)->GetMovePed().SetAdditiveNetwork(m_moveNetworkHelper, m_blendInDuration);
			}
			else if (IsFlagSet(AF_SECONDARY))
			{
				m_bForceResendSyncData = true;
				m_bTaskSecondary = true;

				CMovePed::SecondarySlotFlags flags(CMovePed::Secondary_None);

				if (IsFlagSet(AF_TAG_SYNC_IN))
				{
					flags = CMovePed::Secondary_TagSyncTransition;
				}
				if (IsFlagSet(AF_TAG_SYNC_CONTINUOUS))
				{
					flags = CMovePed::Secondary_TagSyncContinuous;
				}

				static_cast<CPed*>(pPhys)->GetMovePed().SetSecondaryTaskNetwork(m_moveNetworkHelper, m_blendInDuration, flags);
			}
			else
			{
				CMovePed::TaskSlotFlags flags(CMovePed::Task_None);

				if (IsFlagSet(AF_TAG_SYNC_IN))
				{
					flags = CMovePed::Task_TagSyncTransition;
				}
				if (IsFlagSet(AF_TAG_SYNC_CONTINUOUS))
				{
					flags = CMovePed::Task_TagSyncContinuous;
				}

				float blendInDuration = m_blendInDuration;

				if(pPhys->IsNetworkClone() && m_blendInDuration > TASK_SCRIPTED_ANIMATION_MAX_CLONE_BLEND_IN_DURATION)
				{
					blendInDuration = TASK_SCRIPTED_ANIMATION_MAX_CLONE_BLEND_IN_DURATION;
				}

				CPed* pPed = static_cast<CPed*>(pPhys);
				
				// if this is the primary task, and we're using the abort on ped movement
				// flag, kick off a player control task to get movement intentions
				if (pPed->IsLocalPlayer() && IsFlagSet(AF_ABORT_ON_PED_MOVEMENT))
				{
					CTaskComplexControlMovement* pTaskControlMovement = static_cast<CTaskComplexControlMovement*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT));

					if (pTaskControlMovement)
					{
						if (pTaskControlMovement->GetMoveTaskType()!=CTaskTypes::TASK_MOVE_PLAYER)
						{
							pTaskControlMovement->SetNewMoveTask(rage_new CTaskMovePlayer());
						}
					}
					else
					{
						CTask* pTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
						if (!pTask || pTask->GetTaskType()!=CTaskTypes::TASK_MOVE_PLAYER)
						{
							pPed->GetPedIntelligence()->AddTaskMovement( rage_new CTaskMovePlayer());
						}

						// Mark the movement task as still in use this frame
						pTask = pPed->GetPedIntelligence()->GetGeneralMovementTask();
						taskAssertf(pTask, "Unable to create ped movement task for AF_ABORT_ON_PED_MOVEMENT!");
						CTaskMoveInterface* pMoveInterface = pTask->GetMoveInterface();
						if (pMoveInterface)
						{
							pMoveInterface->SetCheckedThisFrame(true);
#if !__FINAL
							pMoveInterface->SetOwner(this);
#endif
						}
					}
				}

				pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, blendInDuration, flags);
			}

			// Hide Weapon
			CPed* pPed = static_cast<CPed*>(pPhys);
			if (pPed && IsFlagSet(AF_HIDE_WEAPON))
			{
				if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject()!=NULL)
				{
					CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
					if (pWeapon->GetIsVisible())
					{
						pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, true);
						pPed->GetWeaponManager()->DestroyEquippedWeaponObject(false);
						m_recreateWeapon = true;
					}
				}
			}

			if(pPed && IsFlagSet(AF_EXPAND_PED_CAPSULE_FROM_SKELETON))
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_ExpandPedCapsuleFromSkeleton, true);
			}
		}
		m_bMPHasStoredInitSlotData = false; //set false here and let the network check below decide if it is should be set true again so all tasks, SP or MP, will start with this true until this point.

		if (IsFlagSet(AF_USE_ALTERNATIVE_FP_ANIM) && CanPlayFpsClip(kPriorityLow))
		{
			// Check if the provided fps clip hash exists in the dictionary
			// if it doesn't, clear out the hash so we don't try to play it.
			if (m_dict.GetHash()==0)
			{
				// check the low slot for a valid dictionary, and store it for later use
				InitSlotData* pData = GetSlot(kPriorityLow).GetInitData();
				if (pData && pData->clipData[0].dict.GetHash()!=0)
				{
					m_fpsDict = pData->clipData[0].dict;
				}
			}
			else
			{
				m_fpsDict = m_dict;
			}

			const crClip* pClip = GetFpsClip();
			if (pClip==NULL)
			{
				m_fpsClip.SetHash(0);
				m_fpsDict.SetHash(0);
			}
		}

		for (u8 i=0; i<kNumPriorities; i++)
		{				
			InitSlotData* pData = GetSlot(i).GetInitData();
			
			if (pData)
			{
				bool bKeepInitData =	NetworkInterface::IsNetworkOpen() &&
										TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT == i;

				StartPlayback(*pData, (ePlaybackPriority)i);

				if(!bKeepInitData)
				{
					GetSlot(i).ClearInitData();
				}
				else
				{
					m_bMPHasStoredInitSlotData = true;
				}
			}
		}

		SetTaskState(State_Running);
	}

	return FSM_Continue;
}

CTask::FSM_Return CTaskScriptedAnimation::RequestCloneClips()
{
	const InitSlotData* pData = GetSlot(TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT).GetInitData();

	if(taskVerifyf(pData,"No init slot data at TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT slot") )
	{
		if(pData->state==kStateSingleClip)
		{
#if __ASSERT
			taskAssert(pData->clipData[0].dict.GetHash());
			taskAssertf(m_dict.GetHash()==pData->clipData[0].dict.GetHash(),
				"%s, this [ %p ] has clip data different to what it was created with. Created with - m_dict.GetHash() (0x%x) %s. Now has - pData->clipData[0].dict.GetHash() (0x%x) %s",
				GetAssertNetEntityName(),
				this,
				m_dict.GetHash(),
				m_dict.GetCStr(),
				pData->clipData[0].dict.GetHash(),
				pData->clipData[0].dict.GetCStr());
#endif
			int clipDictIndex = fwAnimManager::FindSlotFromHashKey(m_dict.GetHash()).Get();

			if(clipDictIndex>-1 )
			{
				m_clipRequest[0].RequestClipsByIndex(clipDictIndex);
				m_bClipsAreLoading = true;
				return FSM_Continue;
			}
		}
		else if(pData->state==kStateBlend)
		{
			//ref any dictionaries in case there's a delay before the task starts
			for (s32 i=0; i<MAX_CLIPS_PER_PRIORITY; i++)
			{
				s32 clipDictIndex = fwAnimManager::FindSlotFromHashKey(pData->clipData[i].dict).Get();
				if (clipDictIndex>-1 )
				{
					m_clipRequest[i].RequestClipsByIndex(clipDictIndex);
					m_bClipsAreLoading = true;
				}
				else
				{
					taskAssertf(false,"Failed to find slot from Clip %d, Dict %s",i,pData->clipData[i].dict.GetCStr());
					return FSM_Quit;
				}
			}
			return FSM_Continue;
		}
		else
		{
			taskAssertf(false,"unexpected state %d", pData->state);
		}
	}

	taskAssert(false);
	return FSM_Quit;
}

bool CTaskScriptedAnimation::CloneClipsLoaded()
{
	const InitSlotData* pData = GetSlot(TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT).GetInitData();

    if(taskVerifyf(pData,"No init slot data at TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT slot") )
	{
		if(pData->state==kStateSingleClip)
		{
			if(!m_clipRequest[0].HaveClipsBeenLoaded())
			{
				return false;
			}
		}
		else if(pData->state==kStateBlend)
		{
			for (s32 j=0; j<MAX_CLIPS_PER_PRIORITY; j++)
			{
				if(!m_clipRequest[j].HaveClipsBeenLoaded())
				{
					return false;
				}
			}
		}
		else
		{
			taskAssertf(0,"unexpected slot state %d",pData->state);//no clips expected but empty state not expected on clones so assert and continue
		}
	}

	return true;
}

void CTaskScriptedAnimation::Running_OnEnter()
{
	m_firstUpdate = true;

	// Apply any requested changes to the physics state
	CPhysical *pPhysical = GetPhysical();
	taskAssert(pPhysical);
	if(pPhysical)
	{

#if GTA_REPLAY
		if(!m_initialPosition.IsZero())
		{
			CReplayMgr::RecordWarpedEntity(pPhysical); 
		}
#endif

		// the flags AF_TURN_OFF_COLLISION and AF_IGNORE_GRAVITY now map to AF_OVERRIDE_PHYSICS
		for (u8 i=0;i<kNumPriorities;i++)
		{
			if (GetSlot(i).GetData().IsFlagSet(AF_IGNORE_GRAVITY) || GetSlot(i).GetData().IsFlagSet(AF_TURN_OFF_COLLISION))
			{
				GetSlot(i).GetData().SetFlag(AF_OVERRIDE_PHYSICS, true);
				GetSlot(i).GetData().SetFlag(AF_USE_MOVER_EXTRACTION, true); // temp fix for B* 1368574, until we can figure out why override physics doesn't move the ped anymore.
			}
		}
		ClearFlag(AF_IGNORE_GRAVITY);
		ClearFlag(AF_TURN_OFF_COLLISION);

		if(m_dict.IsNotNull() && m_clip.IsNotNull())
		{
			s32 iDictIndex = fwAnimManager::FindSlotFromHashKey(m_dict.GetHash()).Get();

			const crClip *pClip = NULL;

			if (ShouldPlayFpsClip())
			{
				if ( m_fpsClip.IsNotNull())
				{
					pClip = GetFpsClip();
					taskAssertf(pClip, "Missing firstperson clip (clip dict index, clip hash key): %d, %s %u\n", iDictIndex, m_fpsClip.TryGetCStr() ? m_fpsClip.TryGetCStr() : "(null)", m_fpsClip.GetHash());
				}
			}
			else
			{
				pClip = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, m_clip.GetHash());
				taskAssertf(pClip, "Missing clip (clip dict index, clip hash key): %d, %s %u\n", iDictIndex, m_clip.TryGetCStr() ? m_clip.TryGetCStr() : "(null)", m_clip.GetHash());
			}

			if(pClip)
			{
				ApplyPhysicsStateChanges(pPhysical, pClip);
			}
		}

		if (pPhysical->GetIsTypePed() && ((CPed*)pPhysical)->GetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAIUpdate))
		{
			// If doing a post camera ai update on a ped and we're using mover extraction we need to do an
			// early update of the mover here. Otherwise, it won't get updated on the first frame.
			for (u8 i=0; i<kNumPriorities; i++)
			{
				if (GetSlot(i).GetState()!=kStateEmpty && GetSlot(i).GetData().IsFlagSet(AF_USE_MOVER_EXTRACTION))
				{
					ApplyMoverTrackFixup(GetPhysical(), (ePlaybackPriority)i);
				}
			}
		}

		if (IsFlagSet(AF_PROCESS_ATTACHMENTS_ON_START))
		{
			pPhysical->ProcessAllAttachments();
		}

		if (pPhysical->GetIsTypePed() && IsFlagSet((u32)AF_ACTIVATE_RAGDOLL_ON_COLLISION))
		{
			TUNE_GROUP_FLOAT(TASK_SCRIPTED_ANIMATION, fRagdollOnCollisionAllowedSlope, -0.999f, -10.0f, 10.0f, 0.0001f);
			TUNE_GROUP_FLOAT(TASK_SCRIPTED_ANIMATION, fRagdollOnCollisionAllowedPenetration, 0.100f, 0.0f, 100.0f, 0.0001f);

			CTask* pTaskNM = NULL;

			pTaskNM = rage_new CTaskNMJumpRollFromRoadVehicle(1000, 10000, false);

			CEventSwitch2NM event(10000, pTaskNM);
			GetPed()->SetActivateRagdollOnCollisionEvent(event);
			GetPed()->SetRagdollOnCollisionAllowedSlope(fRagdollOnCollisionAllowedSlope);
			GetPed()->SetRagdollOnCollisionAllowedPenetration(fRagdollOnCollisionAllowedPenetration);
		}
	}
}

bool EnablePhysicsDrivenJointAttributeCB(const crPropertyAttribute &attribute, void *pData)
{
	CVehicle *pVehicle = reinterpret_cast< CVehicle * >(pData);
	if(pVehicle)
	{
	  if(attribute.GetType() == crPropertyAttribute::kTypeInt)
	  {
		  const crPropertyAttributeInt &attributeInt = static_cast< const crPropertyAttributeInt & >(attribute);
  
		  CCarDoor *pDoor = pVehicle->GetDoorFromId(static_cast< eHierarchyId >(attributeInt.GetValue()));
		  if(Verifyf(pDoor, "Couldn't find door specified by a EnablePhysicsDrivenJoint tag"))
		  {
			  pDoor->SetFlag(CCarDoor::DRIVEN_BY_PHYSICS);
		  }
	  }
	}

	return true;
}

CTask::FSM_Return CTaskScriptedAnimation::Running_OnUpdate()
{
	bool continueTask = false;

	if ( IsFlagSet(AF_SECONDARY) && !IsFlagSet(AF_UPPERBODY) && GetBoneMaskHash() == FILTER_ID_INVALID )
	{
		CEntity *pEntity = static_cast<CEntity *>(GetEntity());
		if (pEntity && pEntity->GetIsTypePed())
		{
			CPed *pPed = GetPed();
			if( pPed )
			{
				pPed->SetPedResetFlag(CPED_RESET_FLAG_DisableIdleExtraHeadingChange, true);
			}
		}
	}

	if(IsFlagSet(AF_EXPAND_PED_CAPSULE_FROM_SKELETON))
	{
		CPed *pPed = GetPed();

		if(pPed)
		{
			pPed->SetPedResetFlag(CPED_RESET_FLAG_ExpandPedCapsuleFromSkeleton, true);
		}
	}
	//do process logic for each priority
	for (u8 i=0; i<kNumPriorities; i++)
	{
		bool continueState = false;
		switch(GetSlot(i).GetState())
		{
		case kStateEmpty:
			break;
		case kStateBlend:
			continueState = ProcessBlend3((ePlaybackPriority)i);
			break;
		case kStateSingleClip:
			continueState = ProcessSingleClip((ePlaybackPriority)i);
			break;
		default:
			taskAssert(0);
		}

		if (!continueState && GetSlot(i).GetState()!=kStateEmpty)
		{
			float blendOutDuration = GetSlot(i).GetData().GetBlendOutDuration();
			eScriptedAnimFlagsBitSet flags = GetSlot(i).GetData().GetFlags();
			GetSlot(i).SetState(kStateEmpty);
			// set the flags in case we need them on shutdown (they will be overwritten if the slot is reused)
			GetSlot(i).GetData().GetFlags() = flags;
			m_moveNetworkHelper.SendRequest(GetEmptyTransitionId((ePlaybackPriority)i));
			m_moveNetworkHelper.SetFloat(GetDurationId((ePlaybackPriority)i), blendOutDuration);
		}

		continueTask |= continueState;
	}

	// if we've been externally flagged to exit on the next update, do so now.
	if (m_exitNextUpdate && !m_hasSentDeathEvent)
	{
		continueTask = false;
	}

	// By default, end secondary tasks when the ped dies
	if ( GetEntity() && (GetEntity()->GetType()==ENTITY_TYPE_PED) && GetPed()->GetIsDeadOrDying() 
		&& IsFlagSet(AF_SECONDARY) && !IsFlagSet(AF_DONT_EXIT_ON_DEATH) )
	{
		continueTask = false;
	}

	if (!continueTask || m_moveNetworkHelper.HasNetworkExpired())
	{
		SetTaskState(State_Exit);
		return FSM_Continue;
	}

	CPhysical *pPhysical = GetPhysical();

	pPhysical->RequestUpdateInWorld();

	if(pPhysical->IsNetworkClone())
	{
		if(GetStateFromNetwork() == State_Exit)
		{
			SetTaskState(State_Exit);
			return FSM_Continue;
		}
	}

	// B*1896995: Hide weapon if we've equipped one (ie through a pickup) whilst we are still playing the scripted anim.
	if (pPhysical->GetIsTypePed() && IsFlagSet(AF_HIDE_WEAPON))
	{
		CPed* pPed = static_cast<CPed*>(pPhysical);
		if (pPed)
		{
			if (pPed->GetWeaponManager() && pPed->GetWeaponManager()->GetEquippedWeaponObject()!=NULL)
			{
				CObject* pWeapon = pPed->GetWeaponManager()->GetEquippedWeaponObject();
				if (pWeapon->GetIsVisible())
				{
					pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, true);
					pPed->GetWeaponManager()->DestroyEquippedWeaponObject(false);
					m_recreateWeapon = true;
				}
			}
		}
	}

	if(IsFlagSet(AF_USE_KINEMATIC_PHYSICS))
	{
		CVehicle *pVehicle = pPhysical->GetIsTypeVehicle() ? static_cast< CVehicle * >(pPhysical) : NULL;
		if(pVehicle)
		{
			fwMoveNetworkPlayer *pMoveNetworkPlayer = m_moveNetworkHelper.GetNetworkPlayer();
			if(pMoveNetworkPlayer)
			{
				static const crProperty::Key enablePhysicsDrivenJointKey = crProperty::CalcKey("EnablePhysicsDrivenJoint", 0x78c21fd4);

				mvParameterBuffer::Iterator it;
				for(mvKeyedParameter* pKeyedParameter = it.Begin(pMoveNetworkPlayer->GetOutputBuffer(), mvParameter::kParameterProperty, enablePhysicsDrivenJointKey); pKeyedParameter != NULL; pKeyedParameter = it.Next())
				{
					const crProperty *pProp = pKeyedParameter->Parameter.GetProperty();
					if(pProp)
					{
						pProp->ForAllAttributes(EnablePhysicsDrivenJointAttributeCB, pVehicle);
					}
				}
			}
		}
	}

	// Object and vehicle velocity setting
	if (IsFlagSet(AF_USE_KINEMATIC_PHYSICS) && IsFlagSet(AF_USE_MOVER_EXTRACTION))
	{
		if (pPhysical->GetIsTypeObject() || pPhysical->GetIsTypeVehicle())
		{
			Vector3 animVel = pPhysical->GetAnimatedVelocity();
			float animAngVel = pPhysical->GetAnimatedAngularVelocity();

			animVel.RotateZ(pPhysical->GetTransform().GetHeading() + (animAngVel * fwTimer::GetTimeStep()));

			pPhysical->SetVelocity(animVel);
			pPhysical->SetAngVelocity(Vector3(0.0f, 0.0f, animAngVel));
		}
	}

	// Keep update the desired heading for a smoother exit transition after the animation is done as it might rotate
	if (IsFlagSet(AF_TAG_SYNC_OUT) && !IsFlagSet(AF_ABORT_ON_PED_MOVEMENT))
	{
		CPed* pPed = GetPed();
		pPed->SetDesiredHeading(pPed->GetCurrentHeading());
	}

	if (GetPhysical()->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = GetVehicle();
		if (pVehicle)
		{
			pVehicle->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
		}
	}
	else if (GetPhysical()->GetIsTypeObject())
	{
		CObject* pObject = GetObject();
		if (pObject)
		{
			pObject->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
		}
	}

	return FSM_Continue;
}

bool CTaskScriptedAnimation::ProcessSingleClip(ePlaybackPriority priority)
{
	if (CanPlayFpsClip(priority) && ShouldPlayFpsClip())
	{
		if (!m_bUsingFpsClip)
		{
			// switch to the fps clip
			if (m_moveNetworkHelper.IsNetworkActive())
			{
				m_moveNetworkHelper.SetClip( GetFpsClip(), GetClipId(priority));
			}
			GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
			GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
			m_bUsingFpsClip = true;
		}
	}
	else
	{
		if (m_bUsingFpsClip)
		{
			if (m_moveNetworkHelper.IsNetworkActive())
			{
				m_moveNetworkHelper.SetClip( GetSlot(priority).GetData().GetClip(0), GetClipId(priority));
			}
			GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ForcePostCameraAnimUpdate, true);
			GetPed()->SetPedResetFlag(CPED_RESET_FLAG_ePostCameraAnimUpdateUseZeroTimestep, true);
			m_bUsingFpsClip = false;
		}
	}

	return ProcessCommon(priority);
}

bool CTaskScriptedAnimation::ProcessBlend3(ePlaybackPriority priority)
{
	return ProcessCommon(priority);
}

bool CTaskScriptedAnimation::ProcessCommon(ePlaybackPriority priority)
{
	CPriorityData& data = GetSlot(priority).GetData();
	CPhysical * pPhys = GetPhysical();

	if (data.GetTimer().IsSet() && data.GetTimer().IsOutOfTime())
	{
		return false;
	}

	if (pPhys->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pPhys);

		if (data.IsFlagSet(AF_UPPERBODY))
		{
			//activate the ik
			m_moveNetworkHelper.SetFlag(true, GetUpperBodyIkFlagId(priority));
		}
		else
		{
			//deactivate the ik
			m_moveNetworkHelper.SetFlag(false, GetUpperBodyIkFlagId(priority));
			pPed->SetPedResetFlag(CPED_RESET_FLAG_PreserveAnimatedAngularVelocity, true);
		}

		if (data.IsFlagSet(AF_ACTIVATE_RAGDOLL_ON_COLLISION))
		{		
			 // set the ragdoll to activate on collision with geometry
			pPed->SetActivateRagdollOnCollision(true);
		}

		bool bUseMoverExtraction = data.IsFlagSet(AF_USE_MOVER_EXTRACTION);
		bool bUseKinematicPhysics = data.IsFlagSet(AF_USE_KINEMATIC_PHYSICS);

		if (bUseMoverExtraction)
		{
			// Disable the constraint, otherwise the ped capsule orientation gets changed during the
			// physics update, potentially leading to odd collisions during scenes where the mover capsule
			// is being externally animated.
			pPed->SetPedResetFlag(CPED_RESET_FLAG_UsingMoverExtraction, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedConstraints, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_DisablePedCapsuleMapCollision, true);
			pPed->SetPedResetFlag(CPED_RESET_FLAG_BlockRagdollFromVehicleFallOff, true);
		}

		// Stop the physics world from affecting clip playback on the mover
		if (data.IsFlagSet(AF_TURN_OFF_COLLISION) || data.IsFlagSet(AF_OVERRIDE_PHYSICS ))
		{		// Force low lod physics by default (this removes collision and 
			pPed->SetPedResetFlag(CPED_RESET_FLAG_OverridePhysics, true);
		}
		else if (bUseKinematicPhysics)
		{
			pPed->SetShouldUseKinematicPhysics(TRUE);
		}

		ProcessIkControlFlags(*pPed, data.GetIkFlags());

		if(pPhys->IsNetworkClone()) 
		{
			if(GetSlot(priority).GetState()==kStateBlend)
			{
				taskAssertf(TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT==priority,"Unexpected clone blend priority %d. Only handling blend on priority %d",priority,TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT);

				for(u8 i=0;i<MAX_CLIPS_PER_PRIORITY;i++)
				{
					if( m_CloneData.m_clonePhaseControlled )
					{
						SetPhase(m_CloneData.m_cloneClipPhase, priority,i);
					}
				}
				SetRate(m_CloneData.m_cloneClipRate, priority,0);
				SetRate(m_CloneData.m_cloneClipRate2, priority,1);
			}
			else
			{
				if( m_CloneData.m_clonePhaseControlled )
				{
					SetPhase(m_CloneData.m_cloneClipPhase, priority);
				}

				SetRate(m_CloneData.m_cloneClipRate, priority);
			}
		}

		// Handle abort conditions related to ped movement
		if (data.IsFlagSet(AF_ABORT_ON_PED_MOVEMENT))
		{
			// Abort if ped is specifically moved via player input
			if (pPed->GetPedResetFlag(CPED_RESET_FLAG_OnlyAbortScriptedAnimOnMovementByInput) && pPed->IsLocalPlayer())
			{
				const CControl* pControl = pPed->GetControlFromPlayer();
				// Check these inputs are currently enabled
				if (pControl && pControl->GetPedWalkLeftRight().IsEnabled() && pControl->GetPedWalkUpDown().IsEnabled())
				{
					const Vector2 vecStickInput(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());
					const float fInputMag = vecStickInput.Mag2();
					if (fInputMag > rage::square(0.5f))
					{
						return false;
					}
				}
			}
			else // Abort if ped is moved by any means, may include mover rotation being changed by animation
			{
				bool bWantsToMove = abs(pPed->GetMotionData()->GetDesiredMbrY()) > 0.001f || abs(pPed->GetMotionData()->GetDesiredMbrX()) > 0.001f;
				bool bWantsToTurn = !AreNearlyEqual(pPed->GetCurrentHeading(), pPed->GetDesiredHeading(), 0.001f);
				if (bWantsToMove || bWantsToTurn)
				{
					return false;
				}
			}
		}

	}
	else
	{
		if (data.IsFlagSet(AF_USE_KINEMATIC_PHYSICS))
		{
			pPhys->SetShouldUseKinematicPhysics(TRUE);
		}
	}

	// Disable map collision if using mover extraction.
	if (data.IsFlagSet(AF_USE_MOVER_EXTRACTION))
	{
		if (pPhys->GetIsTypeObject())
		{
			// Reset flag, similar to ped's CPED_RESET_FLAG_DisablePedCapsuleMapCollision.
			CObject* pObject = static_cast<CObject*>(pPhys);
			pObject->SetDisableObjectMapCollision(true);
		}
		else if (pPhys->GetIsTypeVehicle())
		{
			// Reset flag, similar to ped's CPED_RESET_FLAG_DisablePedCapsuleMapCollision.
			CVehicle* pVeh = static_cast<CVehicle*>(pPhys);
			pVeh->m_nVehicleFlags.bDisableVehicleMapCollision = true;
		}
	}

	if (m_moveNetworkHelper.GetBoolean(GetClipEndedId(priority)) || IsTimeToStartBlendOut(priority) || m_exitNextUpdate)
	{
		if (pPhys->GetIsTypePed() && data.IsFlagSet(AF_ENDS_IN_DEAD_POSE))
		{
			// kill the ped, but keep running the task so we don't start blending the anim out before the dead task starts
			CEventDeath deathEvent(false, false);
			deathEvent.SetUseRagdollFrame(true);
			((CPed*)pPhys)->GetPedIntelligence()->AddEvent(deathEvent);
			m_hasSentDeathEvent = true;
			return true;
		}
		else if (data.IsFlagSet(AF_HOLD_LAST_FRAME))
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool CTaskScriptedAnimation::IsTimeToStartBlendOut(ePlaybackPriority priority)
{
	CPriorityData& data = GetSlot(priority).GetData();
	if (data.IsFlagSet(AF_BLENDOUT_WRT_LAST_FRAME) && !data.IsFlagSet(AF_LOOPING))
	{
		float phase = GetPhase(priority);
		const crClip* pClip = data.GetClip(0);

		if (pClip)
		{
			float timeRemaining = pClip->ConvertPhaseToTime(1.0f - phase);
			return timeRemaining<=m_blendOutDuration;
		}
	}

	return false;
}

const crClip* CTaskScriptedAnimation::GetFpsClip()
{
	s32 dictIndex = fwAnimManager::FindSlotFromHashKey(m_fpsDict.GetHash()).Get();

	if (dictIndex!=ANIM_DICT_INDEX_INVALID)
	{
		return fwAnimManager::GetClipIfExistsByDictIndex(dictIndex, m_fpsClip.GetHash());
	}

	return NULL;
}

bool CTaskScriptedAnimation::CanPlayFpsClip(ePlaybackPriority priority)
{
	return m_fpsClip.GetHash()!=0 && priority==kPriorityLow && GetPhysical() && GetPhysical()->GetIsTypePed();
}

bool CTaskScriptedAnimation::ShouldPlayFpsClip()
{
#if FPS_MODE_SUPPORTED
	if (GetPhysical() && GetPhysical()->GetIsTypePed())
	{
		return (GetPed()->IsFirstPersonShooterModeEnabledForPlayer(false) || IsInVehicleFps() ) && IsFlagSet(AF_USE_ALTERNATIVE_FP_ANIM);
	}
#endif // FPS_MODE_SUPPORTED
	return false;
}

bool CTaskScriptedAnimation::IsInVehicleFps()
{
	if (GetPhysical() && GetPhysical()->GetIsTypePed())
	{
		CPed* Ped = static_cast<CPed*>(GetPhysical()); 
		CVehicle* Vehicle = Ped->GetVehiclePedInside(); 

		if(Vehicle && !Ped->IsExitingVehicle())
		{
			if(camInterface::IsDominantRenderedCameraAnyFirstPersonCamera())
			{
				return true; 
			}
		}
	}
	return false; 
}

#if FPS_MODE_SUPPORTED
void CTaskScriptedAnimation::ProcessFirstPersonIk(eIkControlFlagsBitSet& flags)
{
	if (!flags.BitSet().IsSet(AIK_USE_FP_ARM_LEFT) && !flags.BitSet().IsSet(AIK_USE_FP_ARM_RIGHT))
	{
		return;
	}

	taskAssert(GetPhysical());
	taskAssert(GetPhysical()->GetIsTypePed());

	CPed* pPed = static_cast<CPed*>(GetPhysical());
	const bool bVehicleFPS = IsInVehicleFps();

	if (!pPed->IsFirstPersonShooterModeEnabledForPlayer(false) && !bVehicleFPS)
	{
		for (int helper = 0; helper < 2; ++helper)
		{
			if (m_apFirstPersonIkHelper[helper])
			{
				m_apFirstPersonIkHelper[helper]->Reset();
			}
		}

		return;
	}

	CFirstPersonIkHelper::eFirstPersonType desiredFirstPersonType = CFirstPersonIkHelper::FP_OnFoot;

	if (bVehicleFPS)
	{
		desiredFirstPersonType = pPed->IsInFirstPersonVehicleCamera() ? CFirstPersonIkHelper::FP_DriveBy : CFirstPersonIkHelper::FP_OnFootDriveBy;
	}

	for (int helper = 0; helper < 2; ++helper)
	{
		eIkControlFlags controlFlag = (helper == 0) ? AIK_USE_FP_ARM_LEFT : AIK_USE_FP_ARM_RIGHT;

		if (flags.BitSet().IsSet(controlFlag))
		{
			// Recreate helper if first person type changes
			if (m_apFirstPersonIkHelper[helper] && (m_apFirstPersonIkHelper[helper]->GetType() != desiredFirstPersonType))
			{
				delete m_apFirstPersonIkHelper[helper];
				m_apFirstPersonIkHelper[helper] = NULL;
			}

			if (m_apFirstPersonIkHelper[helper] == NULL)
			{
				m_apFirstPersonIkHelper[helper] = rage_new CFirstPersonIkHelper(desiredFirstPersonType);
				taskAssert(m_apFirstPersonIkHelper[helper]);

				m_apFirstPersonIkHelper[helper]->SetArm((helper == 0) ? CFirstPersonIkHelper::FP_ArmLeft : CFirstPersonIkHelper::FP_ArmRight);

				u32 extraFlags = 0;
				if (flags.BitSet().IsSet(AIK_USE_ARM_ALLOW_TAGS))
				{
					extraFlags|=AIK_USE_ANIM_ALLOW_TAGS;
					
					// The helper enabled block tags by default, so if the task is asking for allow
					// tags instead, we have to tell the helper to ignore block tags.
					if (!flags.BitSet().IsSet(AIK_USE_ARM_BLOCK_TAGS))
					{
						m_apFirstPersonIkHelper[helper]->SetUseAnimBlockingTags(false);
					}
				}
				if (flags.BitSet().IsSet(AIK_USE_ARM_BLOCK_TAGS))
				{
					extraFlags|=AIK_USE_ANIM_BLOCK_TAGS;
				}

				m_apFirstPersonIkHelper[helper]->SetExtraFlags(extraFlags);
			}

			if (m_apFirstPersonIkHelper[helper])
			{
				m_apFirstPersonIkHelper[helper]->Process(pPed);
			}
		}
	}
}
#endif // FPS_MODE_SUPPORTED

void CTaskScriptedAnimation::Running_OnExit()
{
	bool offsetEntity = false;
	bool reorientEntity = false;

	for (u8 i=0; i<kNumPriorities; i++)
	{
		offsetEntity |= GetSlot(i).GetData().IsFlagSet(AF_REPOSITION_WHEN_FINISHED);
		reorientEntity |= GetSlot(i).GetData().IsFlagSet(AF_REORIENT_WHEN_FINISHED);
	}
	
	CPhysical *pPhys = GetPhysical();
	bool bUsingRagdoll = false;

	if (pPhys && pPhys->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pPhys);

		if (IsFlagSet((u32)AF_ACTIVATE_RAGDOLL_ON_COLLISION))
		{
			CPed* pPed = GetPed();
			pPed->SetActivateRagdollOnCollision(false);
			pPed->ClearActivateRagdollOnCollisionEvent();
		}

		if (pPed->GetUsingRagdoll())
		{
			bUsingRagdoll = true;
		}
	}

	Mat34V rootMatrix;

	if ((offsetEntity || reorientEntity) && pPhys && pPhys->GetSkeleton() && !bUsingRagdoll)
	{
		pPhys->GetSkeleton()->GetGlobalMtx(BONETAG_ROOT, rootMatrix);
		
		// If we are attached to anything detach now
		if (pPhys->GetAttachParent())
		{
			pPhys->DetachFromParent(0);
		}

		if (!offsetEntity)
		{
			rootMatrix.Setd(pPhys->GetMatrix().d());
		}
		if (!reorientEntity)
		{
			rootMatrix.Set3x3(pPhys->GetMatrix());
		}

		pPhys->SetMatrix(RCC_MATRIX34(rootMatrix));

	}
}

void CTaskScriptedAnimation::CleanUp()
{
	CPhysical* pPhys = GetPhysical();

	RestorePhysicsState(pPhys);
		
	if (pPhys->GetIsTypePed())
	{
		CPed* pPed = GetPed();
		Vector3 eulerRotation;
		Matrix34 m = MAT34V_TO_MATRIX34(pPed->GetMatrix());
		m.ToEulersXYZ(eulerRotation);

		pPed->SetDesiredHeading(eulerRotation.z);

		if (IsFlagSet(AF_ADDITIVE))
		{
			pPed->GetMovePed().ClearAdditiveNetwork(m_moveNetworkHelper, m_blendOutDuration);
		}
		if (m_bTaskSecondary)
		{
			CMovePed::SecondarySlotFlags flags(CMovePed::Secondary_None);

			if (IsFlagSet(AF_TAG_SYNC_OUT))
			{
				flags = CMovePed::Secondary_TagSyncTransition;
			}

			if (m_cloneLeaveMoveNetwork)
			{
				m_moveNetworkHelper.ReleaseNetworkPlayer();
			}
			else
			{
				pPed->GetMovePed().ClearSecondaryTaskNetwork(m_moveNetworkHelper, m_blendOutDuration, flags);
			}
		}
		else
		{
			CMovePed::TaskSlotFlags flags(CMovePed::Task_None);

			if (IsFlagSet(AF_TAG_SYNC_OUT))
			{
				flags = CMovePed::Task_TagSyncTransition;
			}

			if (GetIsFlagSet(aiTaskFlags::HandleCloneSwapToSameTaskType))
			{
				taskAssertf(GetRunAsAClone() || pPed->IsNetworkClone(),"%s Only expect HandleCloneSwapToSameTaskType on clones!",pPed->GetDebugName());
				m_blendOutDuration = MIGRATE_SLOW_BLEND_DURATION; //Set to a large amount to minimize any temporary blend to idle
			}
			
			if (m_cloneLeaveMoveNetwork)
			{
				m_moveNetworkHelper.ReleaseNetworkPlayer();
			}
			else
			{
				pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper, m_blendOutDuration, flags);
			}
		}

		// Show Weapon
		if (pPed && m_recreateWeapon && pPed->GetWeaponManager())
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_KeepWeaponHolsteredUnlessFired, false);
			pPed->GetWeaponManager()->EquipWeapon(pPed->GetWeaponManager()->GetEquippedWeaponHash(), -1, true);
		}

		if (m_bOneShotFacialBlocked)
		{
			pPed->GetMovePed().ClearOneShotFacialBlock();

			m_bOneShotFacialBlocked = false;
		}
#if __BANK
		if(NetworkInterface::IsGameInProgress())
		{
			BANK_ONLY(taskDebugf2("CTaskScriptedAnimation::CleanUp Frame : %u - %s%s : %p :  state  %s:%s:%p  m_dict %s, m_clip %s. m_blendOutDuration %.2f, m_cloneLeaveMoveNetwork %s, HandleCloneSwapToSameTaskType %s",
				fwTimer::GetFrameCount(),
				GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ",
				GetPed()->GetDebugName(),
				GetPed(),
				GetTaskName(),
				GetStateName(GetState()),
				this,
				m_dict.TryGetCStr(),
				m_clip.TryGetCStr(),
				m_blendOutDuration,
				m_cloneLeaveMoveNetwork?"TRUE":"FALSE",
				GetIsFlagSet(aiTaskFlags::HandleCloneSwapToSameTaskType)?"TRUE":"FALSE") );
		}
#endif
	}
	else if (pPhys->GetIsTypeObject() )
	{
		CObject *pObject = static_cast<CObject*>(pPhys);
		if (pObject->GetAnimDirector())
		{
			pObject->GetMoveObject().ClearNetwork(m_moveNetworkHelper, m_blendOutDuration);
		}
		pObject->m_nDEflags.bForcePrePhysicsAnimUpdate = false;
	}
	else if (pPhys->GetIsTypeVehicle())
	{
		CVehicle *pVehicle = static_cast<CVehicle*>(pPhys);		
		if (pVehicle->GetAnimDirector())
		{
			pVehicle->GetMoveVehicle().ClearNetwork(m_moveNetworkHelper, m_blendOutDuration);
		}
		pVehicle->m_nVehicleFlags.bAnimateJoints = m_animateJoints;
		pVehicle->m_nDEflags.bForcePrePhysicsAnimUpdate = false;
	}		
}

void CTaskScriptedAnimation::ProcessIkControlFlags(CPed& ped, eIkControlFlagsBitSet& flags)
{
	if (flags.BitSet().IsSet(AIK_DISABLE_LEG_IK))
	{
		ped.GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_OFF);
		ped.GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_OFF);
	}

	if (flags.BitSet().IsSet(AIK_DISABLE_ARM_IK))
	{
		ped.SetPedResetFlag(CPED_RESET_FLAG_DisableArmSolver, true);
	}

	if (flags.BitSet().IsSet(AIK_DISABLE_HEAD_IK))
	{
		ped.SetPedResetFlag(CPED_RESET_FLAG_DisableHeadSolver, true);
	}

	if (flags.BitSet().IsSet(AIK_DISABLE_ARM_IK))
	{
		ped.SetPedResetFlag(CPED_RESET_FLAG_DisableArmSolver, true);
	}

	if (flags.BitSet().IsSet(AIK_DISABLE_TORSO_IK))
	{
		ped.SetPedResetFlag(CPED_RESET_FLAG_DisableTorsoSolver, true);
	}

	if (flags.BitSet().IsSet(AIK_DISABLE_TORSO_REACT_IK))
	{
		ped.SetPedResetFlag(CPED_RESET_FLAG_DisableTorsoReactSolver, true);
	}

	if (flags.BitSet().IsSet(AIK_DISABLE_TORSO_VEHICLE_IK))
	{
		ped.SetPedResetFlag(CPED_RESET_FLAG_DisableTorsoVehicleSolver, true);
	}

	if (flags.BitSet().IsSet(AIK_USE_ARM_ALLOW_TAGS))
	{
		CArmIkSolver *pArmsSolver = static_cast< CArmIkSolver * >(ped.GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeArm));
		if(pArmsSolver)
		{
			for(int arm = 0; arm < CArmIkSolver::NUM_ARMS; arm ++)
			{
				u32 controlFlags = pArmsSolver->GetControlFlags(static_cast< crIKSolverArms::eArms >(arm));
				controlFlags &= AIK_USE_ANIM_ALLOW_TAGS;
				controlFlags &= ~AIK_USE_ANIM_BLOCK_TAGS;
				pArmsSolver->SetControlFlags(static_cast< crIKSolverArms::eArms >(arm), controlFlags);
			}
		}
	}

	if (flags.BitSet().IsSet(AIK_USE_ARM_BLOCK_TAGS))
	{
		CArmIkSolver *pArmsSolver = static_cast< CArmIkSolver * >(ped.GetIkManager().GetSolver(IKManagerSolverTypes::ikSolverTypeArm));
		if(pArmsSolver)
		{
			for(int arm = 0; arm < CArmIkSolver::NUM_ARMS; arm ++)
			{
				u32 controlFlags = pArmsSolver->GetControlFlags(static_cast< crIKSolverArms::eArms >(arm));
				controlFlags &= ~AIK_USE_ANIM_ALLOW_TAGS;
				controlFlags &= AIK_USE_ANIM_BLOCK_TAGS;
				pArmsSolver->SetControlFlags(static_cast< crIKSolverArms::eArms >(arm), controlFlags);
			}
		}
	}

	if (flags.BitSet().IsSet(AIK_USE_LEG_ALLOW_TAGS))
	{
		ped.GetIkManager().SetFlag(PEDIK_LEGS_USE_ANIM_ALLOW_TAGS);
	}

	if (flags.BitSet().IsSet(AIK_USE_LEG_BLOCK_TAGS))
	{
		ped.GetIkManager().SetFlag(PEDIK_LEGS_USE_ANIM_BLOCK_TAGS);
	}

	if (flags.BitSet().IsSet(AIK_PROCESS_WEAPON_HAND_GRIP) || flags.BitSet().IsSet(AIK_USE_FP_ARM_LEFT) || flags.BitSet().IsSet(AIK_USE_FP_ARM_RIGHT))
	{
		ped.SetPedResetFlag(CPED_RESET_FLAG_ProcessPreRender2, true);
	}
}

void CTaskScriptedAnimation::ProcessIkControlFlagsPreRender(CPed& ped, eIkControlFlagsBitSet& flags)
{
	if (flags.BitSet().IsSet(AIK_PROCESS_WEAPON_HAND_GRIP))
	{
		const CWeaponInfo* pWeaponInfo = CTaskMotionPed::GetWeaponInfo(&ped);
		if(pWeaponInfo)
		{
			bool bAllow1HandedAttachToRightGrip = true;
			if(pWeaponInfo->GetUseLeftHandIkWhenAiming())
			{
				bAllow1HandedAttachToRightGrip = false;
			}
			ped.ProcessLeftHandGripIk(bAllow1HandedAttachToRightGrip, false);
		}
	}
}

#if !__FINAL
const char * CTaskScriptedAnimation::GetStaticStateName( s32 iState  )
{
	static char szStateName[128];
	switch (iState)
	{
	case State_Start:					sprintf(szStateName, "State_Start"); break;
	case State_Running:					sprintf(szStateName, "State_Running"); break;
	case State_Exit:					sprintf(szStateName, "State_Exit"); break;
	default: taskAssert(0); 	
	}
	return szStateName;
}

EXTERN_PARSER_ENUM(eScriptedAnimFlags);
EXTERN_PARSER_ENUM(eIkControlFlags);

extern const char* parser_eScriptedAnimFlags_Strings[];
extern const char* parser_eIkControlFlags_Strings[];

void CTaskScriptedAnimation::Debug() const
{
#if DEBUG_DRAW
	// render the ragdoll blocking flags being used
	Matrix34 root(M34_IDENTITY);
	GetPhysical()->GetGlobalMtx(BONETAG_ROOT, root);

	int xpos = -200;
	int ypos = 0;
	grcDebugDraw::Text(root.d, Color_yellow, 0, ypos, "Flags:");
	ypos+=10;
	// Add a toggle for each bit
	for (u8 i=0;i<kNumPriorities;i++)
	{
		for(int j = 0; j < PARSER_ENUM(eScriptedAnimFlags).m_NumEnums; j++)
		{
			if (GetSlot(i).GetData().GetFlags().BitSet().IsSet(j))
			{
				grcDebugDraw::Text(root.d, Color_yellow, xpos, ypos,  parser_eScriptedAnimFlags_Strings[j]);
				ypos+=10;
			}
		}
		for(int j = 0; j < PARSER_ENUM(eIkControlFlags).m_NumEnums; j++)
		{
			if (GetSlot(i).GetData().GetIkFlags().BitSet().IsSet(j))
			{
				grcDebugDraw::Text(root.d, Color_yellow, xpos, ypos,  parser_eIkControlFlags_Strings[j]);
				ypos+=10;
			}
		}

		xpos +=200;
		ypos = 0;
	}
	

#endif	// DEBUG_DRAW
}

#endif // __FINAL

#if __ASSERT
const char* CTaskScriptedAnimation::GetAssertNetEntityName()
{
	const char* entityName = "Null entity";
	if(GetEntity())
	{
		if(GetEntity()->GetType() == ENTITY_TYPE_PED)
		{
			entityName = GetPed()->GetNetworkObject()?GetPed()->GetNetworkObject()->GetLogName():GetEntity()->GetModelName();
		}
		else if(GetEntity()->GetType() == ENTITY_TYPE_OBJECT)
		{
			entityName = static_cast<CObject*>(GetEntity())->GetNetworkObject()?static_cast<CObject*>(GetEntity())->GetNetworkObject()->GetLogName():GetEntity()->GetModelName();
		}
		else
		{
			entityName = GetEntity()->GetModelName();
		}
	}

	return entityName;
}
#endif

CTask::FSM_Return CTaskScriptedAnimation::Start_OnEnterClone()
{
	if (!IsFlagSet(AF_USE_FULL_BLENDING))
	{
		CheckIfClonePlayerNeedsHeadingPositionAdjust();
	}
	
	return RequestCloneClips();
}

//////////////////////////////////////////////////////////////////////////
CTask::FSM_Return CTaskScriptedAnimation::UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin
		FSM_State(State_Start)
		FSM_OnEnter
		return Start_OnEnterClone();
	FSM_OnUpdate
		return Start_OnUpdate();

	FSM_State(State_Running)
		FSM_OnEnter
		Running_OnEnter();
	FSM_OnUpdate
		return Running_OnUpdate();
	FSM_OnExit
		Running_OnExit();

	FSM_State(State_Exit)
		FSM_OnEnter
		Exit_OnEnterClone();
		FSM_OnUpdate
		return FSM_Quit;
	FSM_End
}

void CTaskScriptedAnimation::Exit_OnEnterClone()
{
	//End any clips on clone not finished yet
	for (u8 i=0; i<kNumPriorities; i++)
	{
		if ( GetSlot(i).GetState()!=kStateEmpty)
		{
			float blendOutDuration = GetSlot(i).GetData().GetBlendOutDuration();
			GetSlot(i).SetState(kStateEmpty);
			m_moveNetworkHelper.SendRequest(GetEmptyTransitionId((ePlaybackPriority)i));
			m_moveNetworkHelper.SetFloat(GetDurationId((ePlaybackPriority)i), blendOutDuration);
		}
	}
}

void CTaskScriptedAnimation::OnCloneTaskNoLongerRunningOnOwner()
{
	SetStateFromNetwork(State_Exit);
}

CTaskInfo* CTaskScriptedAnimation::CreateQueriableState() const
{
	CTaskInfo* pInfo = NULL;

	if(!NetworkInterface::IsGameInProgress() || m_bAllowOverrideCloneUpdate || (GetEntity() && !m_bMPHasStoredInitSlotData) )
	{
		pInfo = rage_new CTaskInfo();
		return pInfo;
	}

	const InitSlotData* pData = GetSlot(TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT).GetInitData();

	if(taskVerifyf(pData,"%s No init slot data at priority slot %d, the only slot synced in MP games. m_clip %s, m_dict %s IsTransitioning %s \n",
		(GetEntity() && GetPed())?GetPed()->GetDebugName():"NUll entity",TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT,m_clip.GetCStr(),m_dict.GetCStr(),NetworkInterface::IsTransitioning()?"YES":"NO") )
    {
		if( pData->state==kStateBlend ) 
		{

			pInfo = rage_new CClonedScriptedAnimationInfo(	GetState(),
															pData,
															m_blendInDuration,
															m_blendOutDuration,
															m_CloneData.m_cloneClipPhase,
															m_CloneData.m_cloneClipRate,
															m_CloneData.m_cloneClipRate2,
															m_CloneData.m_clonePhaseControlled,
															m_CloneData.m_cloneStartClipPhase);
		}
		else if( pData->state==kStateSingleClip)
		{
			u32 uBoneMaskHash = pData->filter.GetHash();

			atHashWithStringNotFinal	dictName = m_dict;
			atHashWithStringNotFinal	clipName = m_clip;

			if( dictName.GetHash() == 0 &&
				clipName.GetHash() == 0 &&
				pData->clipData[0].dict.GetHash() != 0 &&
				pData->clipData[0].clip.GetHash() != 0 )
			{
				dictName = pData->clipData[0].dict;
				clipName = pData->clipData[0].clip;
			}

			pInfo = rage_new CClonedScriptedAnimationInfo(GetState(),
														  pData->blendInDuration,
														  pData->blendOutDuration,
														  reinterpret_cast<const u32&>(pData->flags),
														  pData->ikFlags.BitSet().IsSet(AIK_DISABLE_LEG_IK),
														  dictName,
														  clipName,
														  m_CloneData.m_cloneStartClipPhase ,
														  m_CloneData.m_cloneClipPhase ,
														  m_CloneData.m_cloneClipRate ,
														  m_CloneData.m_cloneClipRate2 ,
														  m_initialPosition,
														  m_initialOrientation,
														  m_physicsSettings!=0,
														  uBoneMaskHash,
														  m_CloneData.m_clonePhaseControlled);
		}
    }

	taskAssert(pInfo!=NULL);

	return pInfo;
}

void CTaskScriptedAnimation::ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo)
{
	taskAssert( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SCRIPTED_ANIMATION );
	CClonedScriptedAnimationInfo *scriptedAnimationInfo = static_cast<CClonedScriptedAnimationInfo*>(pTaskInfo);

	m_CloneData.m_cloneClipPhase		= scriptedAnimationInfo->GetClipPhase();
	m_CloneData.m_cloneClipRate			= scriptedAnimationInfo->GetClipRate();
	m_CloneData.m_cloneClipRate2		= scriptedAnimationInfo->GetClipRate2();

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

bool CTaskScriptedAnimation::GetQueriableDataForSecondaryBehaviour(	u32							&refFlags,
																	float						&refBlendIn,
																	float						&refBlendOut,
																	u8							&refPriority,
																	u32							&refDictHash,
																	u32							&refAnimHash)
{
	for (u8 i=0; i<kNumPriorities; i++)
	{				
		InitSlotData* pData = GetSlot(i).GetInitData();

		if (pData && pData->state == kStateSingleClip)
		{
			refBlendIn			= pData->blendInDuration;
			refBlendOut			= pData->blendOutDuration;
			refFlags			= reinterpret_cast<u32&>(pData->flags);
			refPriority			= (u8)i;
			refDictHash			= m_dict;
			refAnimHash			= m_clip;

			return true;
		}
	}

	return false;
}

bool CTaskScriptedAnimation::CheckRequestDictionaries(const InitSlotData* pData)
{
	if(pData)
	{
		if(pData->state==kStateSingleClip)
		{
			u32 dictHash = m_dict.GetHash();
			return CheckAndRequestSyncDictionary(&dictHash,1);
		}
		else if(pData->state==kStateBlend)
		{
			u32 dictHashSlot[3];

			for(int i=0;i<MAX_CLIPS_PER_PRIORITY;i++)
			{
				dictHashSlot[i] = pData->clipData[i].dict.GetHash();
			}
			return CheckAndRequestSyncDictionary(dictHashSlot,MAX_CLIPS_PER_PRIORITY);
		}
	}

	return false;
}


bool CTaskScriptedAnimation::IsInScope(const CPed* UNUSED_PARAM(pPed))
{
	bool inScope = true;

	taskAssertf(GetPed()->IsNetworkClone(),"Only expect this function to used with clones");

	const InitSlotData* pData = GetSlot(TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT).GetInitData();

	if(!m_bCloneDictRefAdded && pData)
	{
		inScope = CheckRequestDictionaries(pData);
	}

	return inScope;
}

bool CTaskScriptedAnimation::OverridesNetworkBlender(CPed *pPed)
{
    // don't want to override the blender for invisible, fixed peds,
    // as the animation will not be able to change there position/heading
    // and this may leave them out of sync
    bool bNetworkBlendingIsDisabled = pPed->GetNetworkObject() && pPed->GetNetworkObject()->IsLocalFlagSet(CNetObjGame::LOCALFLAG_DISABLE_BLENDING);

    if((!pPed->GetIsVisible() && pPed->GetIsAnyFixedFlagSet()) || bNetworkBlendingIsDisabled)
    {
        return false;
    }
    else
	{
		// Don't override blender (special cases i.e. script-side implemented diving animations)
		if (IsFlagSet(AF_USE_FULL_BLENDING))
		{
			return false;
		}

        return true;
    }
}

bool CTaskScriptedAnimation::ControlPassingAllowed(CPed *pPed, const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType))
{
	bool bAllowControlPassing = (GetState()>State_Start) && !m_bAllowOverrideCloneUpdate;//B* 518733 Check that task has progressed past the stage when we know clips have been streamed in. B* 1615195 Dont allow overriden peds to migrate

	if(pPed->IsNetworkClone())
	{
		const InitSlotData* pData = GetSlot(TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT).GetInitData();

		if(!m_bCloneDictRefAdded && pData)
		{
			bAllowControlPassing = CheckRequestDictionaries(pData);
		}
	}

	return bAllowControlPassing;  
}

CTaskFSMClone *CTaskScriptedAnimation::CreateTaskForClonePed(CPed *ASSERT_ONLY(pPed)) 
{
	CTaskFSMClone* pTask = NULL;
	CTaskInfo* pTaskInfo = CreateQueriableState();

	if(pTaskInfo)
	{
		taskAssertf( pTaskInfo->GetTaskInfoType() == CTaskInfo::INFO_TYPE_SCRIPTED_ANIMATION,"%s TaskInfoType %d, m_bMPHasStoredInitSlotData %s, m_bAllowOverrideCloneUpdate %s, m_clip (0x%x) %s, State %d",
			pPed?pPed->GetDebugName():"Null ped",
			pTaskInfo->GetTaskInfoType(),
			m_bMPHasStoredInitSlotData?"TRUE":"FALSE",
			m_bAllowOverrideCloneUpdate?"TRUE":"FALSE",
			m_clip.GetHash(),
			m_clip.GetCStr(),
			GetState());

		pTask = pTaskInfo->CreateCloneFSMTask();

		if(pTask)
		{
			// leave the move network for the task that will replace this one B*505550
			SetCloneLeaveMoveNetwork();

			if(GetState()==State_Running)
			{
				//copy the current phase if we are playing a single anim
				for (u8 i=0; i<kNumPriorities; i++)
				{				
					if(GetSlot(i).GetState()==kStateSingleClip)
					{
						CTaskScriptedAnimation * pTaskScriptedAnimation = static_cast<CTaskScriptedAnimation*>(pTask);

						pTaskScriptedAnimation->m_CloneData.m_clonePostMigrateStartClipPhase = GetPhase((ePlaybackPriority)i);
						break;
					}
				}
			}
		}

		delete pTaskInfo;
	}

	return pTask;
}

CTaskFSMClone *CTaskScriptedAnimation::CreateTaskForLocalPed(CPed *pPed)
{
	// do the same as clone ped
	CTaskFSMClone *pTask = CreateTaskForClonePed(pPed);

	if (pTask)
	{
		SafeCast(CTaskScriptedAnimation, pTask)->m_bClipsAreLoading = m_bClipsAreLoading;
	}

	return pTask;
}

//-------------------------------------------------------------------------
//	Used by the blended clips constructor to initialise the clone start phase value.
//	Currently clones have a very restricted use of blended clips that 
//	expect only using the low priority slot data
//-------------------------------------------------------------------------
void CTaskScriptedAnimation::InitClonesStartPhaseFromBlendedClipsData()
{
	if(!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	InitSlotData* pData = GetSlot(TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT).GetInitData();

	if( taskVerifyf(pData,"Expect slot num %d data to be set up before calling this func", TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT))
	{
		m_bMPHasStoredInitSlotData = true;
		m_CloneData.m_cloneStartClipPhase = pData->clipData[0].phase;
		taskAssertf(m_CloneData.m_cloneStartClipPhase<=1.0f,"m_CloneData.m_cloneStartClipPhase %f out of range.",m_CloneData.m_cloneStartClipPhase);
#if __ASSERT
		if(pData->clipData[1].clip.GetHash() != 0 && pData->clipData[1].weight !=0.0f)
		{
			taskAssertf(m_CloneData.m_cloneStartClipPhase == pData->clipData[1].phase,
				"Clones Second anim in low priority slot should have same start phase as first anim. First anim %s [0x%x] start phase %f, Second anim %s [0x%x] start phase %f ",
				pData->clipData[0].clip.GetCStr(),
				pData->clipData[0].clip.GetHash(),
				m_CloneData.m_cloneStartClipPhase,
				pData->clipData[1].clip.GetCStr(),
				pData->clipData[1].clip.GetHash(),
				pData->clipData[1].phase);
		}
		if(pData->clipData[2].clip.GetHash() != 0 && pData->clipData[2].weight !=0.0f)
		{
			taskAssertf(m_CloneData.m_cloneStartClipPhase == pData->clipData[2].phase,
				"Clones Third anim in low priority slot should have same start phase as first anim. First anim %s [0x%x] start phase %f, Third anim %s [0x%x] start phase %f ",
				pData->clipData[0].clip.GetCStr(),
				pData->clipData[0].clip.GetHash(),
				m_CloneData.m_cloneStartClipPhase,
				pData->clipData[2].clip.GetCStr(),
				pData->clipData[2].clip.GetHash(),
				pData->clipData[2].phase);
		}
#endif
	}
}

//Is task is running on a clone player running full body anim checks against remote position
//and heading to see if it is within tolerances and required to correct within tolerance
bool CTaskScriptedAnimation::CheckIfClonePlayerNeedsHeadingPositionAdjust()
{
	if (!m_bTaskSecondary				&&
		GetPhysical()					&&
		GetPhysical()->IsNetworkClone() &&
		GetPhysical()->GetIsTypePed()	&&
		GetPed()->GetNetworkObject()	&&
		GetPed()->IsPlayer() )
	{
		TUNE_GROUP_FLOAT(TASK_SCRIPTED_ANIMATION, fClonePlayerPositionTolerance, 0.2f, 0.0f, 1.0f, 0.05f); 
		TUNE_GROUP_FLOAT(TASK_SCRIPTED_ANIMATION, fClonePlayerHeadingTolerance, PI*0.06f, 0.0f, PI*0.25f, 0.01f); //About 10 deg

		float   fDesiredHeading		= NetworkInterface::GetLastHeadingReceivedOverNetwork(GetPed());
		Vector3 vDesiredPosition	= NetworkInterface::GetLastPosReceivedOverNetwork(GetPed());

		float   fLocalHeading       = fwAngle::LimitRadianAngleSafe(GetPed()->GetCurrentHeading());
		Vector3 vLocalPosition	    = VEC3V_TO_VECTOR3(GetPed()->GetTransform().GetPosition());

		float	fHeadingDiff        = SubtractAngleShorter(fDesiredHeading, fLocalHeading);

		if(!vDesiredPosition.IsClose(vLocalPosition, fClonePlayerPositionTolerance) ||
			fabs(fHeadingDiff) > fClonePlayerHeadingTolerance)
		{
			NetworkInterface::GoStraightToTargetPosition(GetPed());
			return true;
		}

	}
	return false;
}

u32 CTaskScriptedAnimation::GetFlagsForSingleAnim()
{
	u32 flags=0;

	for (u8 i=0; i<kNumPriorities; i++)
	{				
		const InitSlotData* pData = GetSlot(i).GetInitData();

		if(pData && pData->state==kStateSingleClip)
		{
			flags = reinterpret_cast<const u32&>(pData->flags);
			return flags;
		}

		if (GetSlot(i).GetState()==kStateSingleClip)
		{
			const CPriorityData& priorityData = GetSlot(i).GetData();
			flags = reinterpret_cast<const u32&>(priorityData.GetFlags());
			return flags;
		}
	}

	return flags;
}

u32  CTaskScriptedAnimation::GetBoneMaskHash()
{
	const InitSlotData* pData = GetSlot(TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT).GetInitData();

	if( pData && 
		pData->state==kStateSingleClip &&
		pData->filter != 0)
	{
		return pData->filter.GetHash();
	}

	return 0; //return 0 indicate default
}

#if __BANK
bool	CTaskScriptedAnimation::ms_bankOverrideCloneUpdate	= false;
bool	CTaskScriptedAnimation::ms_bankUseSecondaryTaskTree	= false;
bool	CTaskScriptedAnimation::ms_advanced	= false;

//Flags 
bool	CTaskScriptedAnimation::ms_AF_UPPERBODY					= true;
bool	CTaskScriptedAnimation::ms_AF_SECONDARY					= false;
bool	CTaskScriptedAnimation::ms_AF_LOOPING					= false;
bool	CTaskScriptedAnimation::ms_AF_NOT_INTERRUPTABLE			= false;
bool	CTaskScriptedAnimation::ms_AF_USE_KINEMATIC_PHYSICS		= false;
bool	CTaskScriptedAnimation::ms_AF_USE_MOVER_EXTRACTION		= false;
bool	CTaskScriptedAnimation::ms_AF_EXTRACT_INITIAL_OFFSET	= false;
bool	CTaskScriptedAnimation::ms_AIK_DISABLE_LEG_IK			= false;

char	CTaskScriptedAnimation::ms_NameOfAnim[MAX_ANIM_NAME_LEN];
char	CTaskScriptedAnimation::ms_NameOfDict[MAX_DICT_NAME_LEN];
float	CTaskScriptedAnimation::ms_fPlayRate				= 1.0f;
float	CTaskScriptedAnimation::ms_fStartPhase				= 0.0f;
float	CTaskScriptedAnimation::ms_fBlendIn					= 8.0f;
float	CTaskScriptedAnimation::ms_fBlendOut				= -8.0f;

void CTaskScriptedAnimation::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("A.I.");
	if (pBank)
	{
		strcpy(ms_NameOfDict,"anim@heists@fleeca_bank@ig_7_jetski_owner");
		strcpy(ms_NameOfAnim,"owner_reaction");

		pBank->PushGroup("Task Scripted Animation", false);
		pBank->AddSeparator();
		pBank->AddToggle("Use secondary task tree", &ms_bankUseSecondaryTaskTree, NullCB);
		pBank->AddToggle("Override Clone Update", &ms_bankOverrideCloneUpdate, NullCB);
		pBank->AddToggle("Advanced, move ped a bit in x", &ms_advanced, NullCB);
		pBank->AddSeparator();
		pBank->AddToggle("AF_LOOPING", &ms_AF_LOOPING, NullCB);
		pBank->AddToggle("AF_UPPERBODY", &ms_AF_UPPERBODY, NullCB);
		pBank->AddToggle("AF_SECONDARY", &ms_AF_SECONDARY, NullCB);
		pBank->AddToggle("AF_NOT_INTERRUPTABLE", &ms_AF_NOT_INTERRUPTABLE, NullCB);
		pBank->AddToggle("AF_USE_KINEMATIC_PHYSICS", &ms_AF_USE_KINEMATIC_PHYSICS, NullCB);
		pBank->AddToggle("AF_USE_MOVER_EXTRACTION", &ms_AF_USE_MOVER_EXTRACTION, NullCB);
		pBank->AddToggle("AF_EXTRACT_INITIAL_OFFSET", &ms_AF_EXTRACT_INITIAL_OFFSET, NullCB);
		pBank->AddSeparator();
		pBank->AddToggle("AIK_DISABLE_LEG_IK", &ms_AIK_DISABLE_LEG_IK, NullCB);
		pBank->AddSeparator();
		pBank->AddText("Anim name", ms_NameOfAnim,	MAX_ANIM_NAME_LEN, false);
		pBank->AddSeparator();
		pBank->AddText("Dict name", ms_NameOfDict,	MAX_DICT_NAME_LEN, false);
		pBank->AddSeparator();
		pBank->AddButton("Start Anim", StartTestAnim);
		pBank->AddSeparator();
		pBank->AddButton("Stream Dictionary", StreamTestDict);
		pBank->AddSeparator();
		pBank->AddSlider("Play Rate", &ms_fPlayRate,  0.0f, 2.0f, 0.001f, NullCB);
		pBank->AddSeparator();
		pBank->AddSlider("Start phase", &ms_fStartPhase,  0.0f, 1.0f, 0.001f, NullCB);
		pBank->AddSeparator();
		pBank->AddSlider("Blend In", &ms_fBlendIn,  -100.0f, 100.0f, 0.5f, NullCB);
		pBank->AddSeparator();
		pBank->AddSlider("Blend Out", &ms_fBlendOut,  -100.0f, 100.0f, 0.5f, NullCB);
		pBank->AddSeparator();

		pBank->PopGroup();
	}
}

void CTaskScriptedAnimation::StartTestAnim()
{
	CEntity* pFocusEntity1 = CDebugScene::FocusEntities_Get(0);
	if (pFocusEntity1 && pFocusEntity1->GetIsTypePed())
	{
		CPed* pPed = (CPed*)pFocusEntity1;
		if (pPed && pPed->GetPedIntelligence())
		{
			u32 iBoneMask = BONEMASK_ALL;
			CTaskScriptedAnimation* pTask =NULL;

			eScriptedAnimFlagsBitSet flags;

			unsigned flagBits =0;

			if(ms_AF_UPPERBODY)
			{
				flagBits |=BIT(AF_UPPERBODY);
			}
			if(ms_AF_SECONDARY)
			{
				flagBits |=BIT(AF_SECONDARY);
			}
			if(ms_AF_LOOPING)
			{
				flagBits |=BIT(AF_LOOPING);
			}
			if(ms_AF_NOT_INTERRUPTABLE)
			{
				flagBits |=BIT(AF_NOT_INTERRUPTABLE);
			}
			if(ms_AF_USE_KINEMATIC_PHYSICS)
			{
				flagBits |=BIT(AF_USE_KINEMATIC_PHYSICS);
			}
			if(ms_AF_USE_MOVER_EXTRACTION)
			{
				flagBits |=BIT(AF_USE_MOVER_EXTRACTION);
			}
			if(ms_AF_EXTRACT_INITIAL_OFFSET)
			{
				flagBits |=BIT(AF_EXTRACT_INITIAL_OFFSET);
			}

			flags.BitSet().SetBits(flagBits);

			eIkControlFlagsBitSet ikFlags = CTaskScriptedAnimation::AIK_NONE;

			unsigned ikFlagBits =0;

			if(ms_AIK_DISABLE_LEG_IK)
			{
				ikFlagBits |= BIT(AIK_DISABLE_LEG_IK);
			}
			ikFlags.BitSet().SetBits(ikFlagBits);

			if(ms_advanced)
			{
				Vector3 vPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				vPosition.x += 0.5f;
				const float fHeading = 0.0f;

				Vector3 vHeading(0.0f, 0.0f, fHeading);
				Quaternion rotationQuaternion;
				CScriptEulers::QuaternionFromEulers(rotationQuaternion, vHeading, EULER_XYZ);

				pTask = rage_new CTaskScriptedAnimation(ms_NameOfDict,
					ms_NameOfAnim,
					CTaskScriptedAnimation::kPriorityLow,
					iBoneMask,
					fwAnimHelpers::CalcBlendDuration(ms_fBlendIn),
					fwAnimHelpers::CalcBlendDuration(ms_fBlendOut),
					-1, /*nTimeToPlay,*/
					flags,
					ms_fStartPhase,
					vPosition,
					rotationQuaternion,
					false,
					false, /*phaseControlled,*/
					ikFlags,
					ms_bankOverrideCloneUpdate);
			}
			else
			{
				pTask = rage_new CTaskScriptedAnimation(ms_NameOfDict,
					ms_NameOfAnim,
					CTaskScriptedAnimation::kPriorityLow,
					iBoneMask,
					fwAnimHelpers::CalcBlendDuration(ms_fBlendIn),
					fwAnimHelpers::CalcBlendDuration(ms_fBlendOut),
					-1, /*nTimeToPlay,*/
					flags,
					ms_fStartPhase,
					false,
					false, /*phaseControlled,*/
					ikFlags,
					ms_bankOverrideCloneUpdate);
			}

			if(pPed->IsNetworkClone())
			{
				pPed->GetPedIntelligence()->AddLocalCloneTask(pTask, PED_TASK_PRIORITY_PRIMARY);
			}
			else
			{
				if(ms_bankUseSecondaryTaskTree)
				{
					ms_AF_SECONDARY = true; //always set this true is using secondary task tree
					pPed->GetPedIntelligence()->AddTaskSecondary( pTask, PED_TASK_SECONDARY_PARTIAL_ANIM );
				}
				else
				{
					int iEventPriority = E_PRIORITY_GIVE_PED_TASK;
					CEventGivePedTask event(PED_TASK_PRIORITY_PRIMARY,pTask,false,iEventPriority);
					pPed->GetPedIntelligence()->AddEvent(event);
				}

			}
		}
	}
}
void CTaskScriptedAnimation::StreamTestDict()
{
	atHashWithStringNotFinal dictName = ms_NameOfDict;

	u32 dictHash = dictName.GetHash();
	strLocalIndex clipDictIndex = strLocalIndex(1);

	if (dictHash)
	{
		clipDictIndex = fwAnimManager::FindSlotFromHashKey(dictHash);
		if(taskVerifyf(clipDictIndex!=-1,"Unrecognized anim dictionary %u",dictHash))
		{
			fwClipDictionaryLoader loader(clipDictIndex.Get(), false);

			// Get clip dictionary
			crClipDictionary *clipDictionary = loader.GetDictionary();

			if(clipDictionary==NULL)			
			{	//if not loaded put in a request for it
				g_ClipDictionaryStore.StreamingRequest(clipDictIndex, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
			}
		}
	}
}

#endif

void CTaskScriptedAnimation::RemoveEntityFromControllingAnimScene()
{
	// Notify the controlling anim scene that we've been removed from the scene
	if (m_owningAnimScene!=ANIM_SCENE_INST_INVALID)
	{
		CAnimScene* pScene = CAnimSceneManager::GetAnimScene(m_owningAnimScene);

		if (pScene)
		{
			pScene->RemoveEntityFromScene(m_owningAnimSceneEntity);
		}
	}
}

CTaskScriptedAnimation* CTaskScriptedAnimation::FindObjectTask(CObject* pObj)
{
	if (pObj)
	{
		if (pObj->GetObjectIntelligence())
		{
			return static_cast<CTaskScriptedAnimation*>(pObj->GetObjectIntelligence()->FindTaskSecondaryByType(CTaskTypes::TASK_SCRIPTED_ANIMATION));
		}
	}
	return NULL;
}

void CTaskScriptedAnimation::AbortObjectTask(CObject* pObj, CTaskScriptedAnimation* pTask)
{
	if (pObj)
	{
		CObjectIntelligence* pIntelligence = pObj->GetObjectIntelligence();
		if (pIntelligence)
		{
			if (pIntelligence->GetTaskActive(CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY)==pTask)
			{
				pObj->SetTask(NULL, CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM);
			}			
		}
	}
}


void CTaskScriptedAnimation::GiveTaskToObject(CObject* pObj, CTaskScriptedAnimation* pTask)
{
	if (pObj)
	{
		pObj->SetTask(pTask, CObjectIntelligence::OBJECT_TASK_TREE_SECONDARY, CObjectIntelligence::OBJECT_TASK_SECONDARY_ANIM);
	}
}


//-------------------------------------------------------------------------
// Task info for running a CTaskScriptedAnimation scripted clip
//-------------------------------------------------------------------------
CClonedScriptedAnimationInfo::CClonedScriptedAnimationInfo() :
m_clipDictHash(0)
, m_clipHashKey(0)
, m_blendInDuration(0.0f)
, m_blendOutDuration(0.0f)
, m_flags(0)
, m_disableLegIK(false)
, m_startClipPhase(0.0f)
, m_ClipPhase(-1.0f)
, m_ClipRate(1.0f)
, m_setPhase(false)
, m_setRate(false)
, m_vInitialPosition(VEC3_ZERO)
, m_InitialOrientation(Quaternion::sm_I)
, m_setOrientationPosition(false)
, m_BoneMaskHash(0)
{
	m_blendOutDuration = fwAnimHelpers::CalcBlendDuration(NORMAL_BLEND_OUT_DELTA);
}

CClonedScriptedAnimationInfo::CClonedScriptedAnimationInfo(s32 state,
														   float blendInDuration,
                                                           float blendOutDuration,
                                                           u32 flags,
														   bool disableLegIK,
														   atHashWithStringNotFinal m_dict,
														   atHashWithStringNotFinal m_clip,
														   float	startClipPhase,
														   float	clipPhase,
														   float	clipRate,
														   float	clipRate2,
														   const Vector3& vInitialPosition,
														   const Quaternion & initialOrientation,
														   bool		setOrientationPosition,
														   u32		boneMaskHash,
														   bool		phaseControlled) :
m_clipDictHash(0)
, m_clipHashKey(0)
, m_blendInDuration(blendInDuration)
, m_blendOutDuration(blendOutDuration)
, m_flags(flags)
, m_disableLegIK(disableLegIK)
, m_startClipPhase(startClipPhase)
, m_ClipPhase(clipPhase)
, m_ClipRate(clipRate)
, m_ClipRate2(clipRate2)
, m_setPhase(phaseControlled)
, m_setRate(clipRate != 1.0f)
, m_setRate2(clipRate2 != 1.0f)
, m_vInitialPosition(vInitialPosition)
, m_InitialOrientation(initialOrientation)
, m_setOrientationPosition(setOrientationPosition)
, m_BoneMaskHash(boneMaskHash)
{
	SetStatusFromMainTaskState(state);

	m_syncSlotClipstate = CTaskScriptedAnimation::kStateSingleClip;

	taskAssert(m_clip.GetHash());
	taskAssert(m_dict.GetHash());

	m_clipHashKey		= m_clip;
	m_clipDictHash		= m_dict;

	taskAssert(m_clipHashKey != 0);
}

CClonedScriptedAnimationInfo::CClonedScriptedAnimationInfo(s32 state,
														   const CTaskScriptedAnimation::InitSlotData* pPriorityLow,
														   float blendInTime,
														   float blendOutTime,
														   float	clipPhase,
														   float	clipRate,
														   float	clipRate2,
														   bool		phaseControlled,
														   float	startClipPhase ) :
m_clipDictHash(0)
, m_clipHashKey(0)
, m_blendInDuration(blendInTime)
, m_blendOutDuration(blendOutTime)
, m_flags(0)
, m_disableLegIK(false)
, m_startClipPhase(startClipPhase)
, m_ClipPhase(clipPhase)
, m_ClipRate(clipRate)
, m_ClipRate2(clipRate2)
, m_setPhase(phaseControlled)
, m_setRate(clipRate != 1.0f)
, m_setRate2(clipRate2 != 1.0f)
, m_vInitialPosition(VEC3_ZERO)
, m_InitialOrientation(Quaternion::sm_I)
, m_setOrientationPosition(false)
, m_BoneMaskHash(0)
{
	taskAssert(pPriorityLow);

	SetStatusFromMainTaskState(state);

	for(int i=0;i<MAX_CLIPS_SYNCED_PER_PRIORITY;i++)
	{
		m_syncSlotClipData[i].clip		= pPriorityLow->clipData[i].clip.GetHash();
		m_syncSlotClipData[i].dict		= pPriorityLow->clipData[i].dict.GetHash();
		m_syncSlotClipData[i].weight	= pPriorityLow->clipData[i].weight;
	}

	m_syncSlotClipflags = reinterpret_cast<const u32&>(pPriorityLow->flags);
	m_syncSlotClipstate = static_cast<u8>(pPriorityLow->state);
}


CTaskFSMClone *CClonedScriptedAnimationInfo::CreateCloneFSMTask()
{
	CTaskScriptedAnimation *cloneTask = 0;

	if(m_clipHashKey != 0 && m_clipDictHash != 0 )
	{
		if(sysAppContent::IsJapaneseBuild())
		{
			if(m_clipDictHash == ATSTRINGHASH("mp_suicide", 0xB793C8CC) && m_clipHashKey == ATSTRINGHASH("PISTOL", 0x797EF586))
			{
				m_clipHashKey = ATSTRINGHASH("PILL", 0x88C69B0E);
				m_flags |= BIT(AF_HIDE_WEAPON);
			}
		}

		u32 boneMask = 0;

		if((m_flags & BIT(AF_UPPERBODY)) != 0)
		{
			boneMask = (u32)BONEMASK_UPPERONLY;
		}

		if(!m_bDefaultBoneMaskHash)
		{
			boneMask = m_BoneMaskHash;
		}

		eIkControlFlagsBitSet ikFlags = CTaskScriptedAnimation::AIK_NONE;

		unsigned ikFlagBits =0;
		if(m_disableLegIK)
		{
			ikFlagBits |= BIT(AIK_DISABLE_LEG_IK);
		}
		ikFlags.BitSet().SetBits(ikFlagBits);

		CTaskScriptedAnimation::ePlaybackPriority priority = CTaskScriptedAnimation::kPriorityLow;

		eScriptedAnimFlagsBitSet flags;
		flags.BitSet().SetBits(m_flags);

		if(m_setOrientationPosition)
		{
			m_InitialOrientation.Normalize();

			cloneTask = rage_new CTaskScriptedAnimation(atHashWithStringNotFinal(m_clipDictHash),
														atHashWithStringNotFinal(m_clipHashKey),
														priority,
														boneMask,
														m_blendInDuration,
														m_blendOutDuration,
														-1,
														flags,
														m_startClipPhase,
														m_vInitialPosition,
														m_InitialOrientation,
														true,
														m_setPhase,
														ikFlags);
		}
		else
		{
			cloneTask = rage_new CTaskScriptedAnimation(atHashWithStringNotFinal(m_clipDictHash),
														atHashWithStringNotFinal(m_clipHashKey),
														priority,
														boneMask,
														m_blendInDuration,
														m_blendOutDuration,
														-1,
														flags,
														m_startClipPhase,
														true,
														m_setPhase,
														ikFlags);
		}
	}
	else if(m_syncSlotClipstate == CTaskScriptedAnimation::kStateBlend)
	{

		CTaskScriptedAnimation::InitSlotData priorityLow;
		CTaskScriptedAnimation::InitSlotData emptyPriorityMid;
		CTaskScriptedAnimation::InitSlotData emptyPriorityHigh;

		priorityLow.state = (CTaskScriptedAnimation::ePlayBackState)m_syncSlotClipstate;
		priorityLow.flags.BitSet().SetBits(m_syncSlotClipflags);

		for(int i=0;i<MAX_CLIPS_SYNCED_PER_PRIORITY;i++)
		{
			priorityLow.clipData[i].clip.SetHash(m_syncSlotClipData[i].clip);
			priorityLow.clipData[i].dict.SetHash(m_syncSlotClipData[i].dict);
			priorityLow.clipData[i].weight = m_syncSlotClipData[i].weight;
			priorityLow.clipData[i].phase  = m_startClipPhase;
		}

		cloneTask = rage_new CTaskScriptedAnimation(priorityLow,
													emptyPriorityMid,
													emptyPriorityHigh,
													m_blendInDuration,
													m_blendOutDuration,
													true);
	}

	if (cloneTask)
	{
		cloneTask->m_bClipsAreLoading = true;
	}

	return cloneTask;
}

CTask *CClonedScriptedAnimationInfo::CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity))
{
    return CreateCloneFSMTask();
}

//TODO: Since this version uses a duration value instead of delta we send the floats as unpacked for now
u32 CClonedScriptedAnimationInfo::PackBlendDuration(float fBlendDuration)
{
	u32 iBlendDuration = 0;

	if (fBlendDuration > 16.0f || fBlendDuration == 0.0f)
		iBlendDuration = 3; // instant blend in
	else if (fBlendDuration > 8.0f)
		iBlendDuration = 2;
	else if (fBlendDuration > 4.0f)
		iBlendDuration = 1;
	else 
		iBlendDuration = 0;

	return iBlendDuration;

}

//TODO: Since this version uses a duration value instead of delta we send the floats as unpacked for now
float CClonedScriptedAnimationInfo::UnpackBlendDuration(u32 iBlendDuration)
{
	float fBlendDelta = 0.0f;

	switch (iBlendDuration)
	{
	case 0:
		fBlendDelta = 4.0f;
		break;
	case 1:
		fBlendDelta = 8.0f;
		break;
	case 2:
		fBlendDelta = 16.0f;
		break;
	case 3:
		fBlendDelta = 1000.0f;
		break;
	default:
		taskAssert(0);
	}

	return fBlendDelta;
}
