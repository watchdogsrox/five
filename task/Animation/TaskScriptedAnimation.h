#ifndef TASK_SCRIPTED_ANIMATION_H
#define TASK_SCRIPTED_ANIMATION_H

//framework headers
#include "fwscene/stores/framefilterdictionarystore.h"

// Game headers
#include "animation/AnimDefines.h"
#include "animation/AnimScene/AnimScene.h"
#include "animation/animscene/AnimSceneManager.h"
#include "task/Animation/AnimTaskFlags.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "peds/ped.h"
#include "Peds/QueriableInterface.h"

#if FPS_MODE_SUPPORTED
class CFirstPersonIkHelper;
#endif // FPS_MODE_SUPPORTED

class CTaskScriptedAnimation : public CTaskFSMClone
{
	friend class CClonedScriptedAnimationInfo;

public:

	static const eScriptedAnimFlagsBitSet AF_NONE;
	static const eIkControlFlagsBitSet AIK_NONE;

	// playback state - Keep in sync with commands_task.sch ANIMATION_PLAYBACK_TYPE
	enum ePlayBackState
	{
		kStateEmpty,
		kStateSingleClip,
		kStateBlend
	};

	// clip playback structures used to cache requests from script
	// keep structure in sync with commands_task.sch STRUCT ANIM_DATA

	struct ScriptInitSlotData{

		ScriptInitSlotData()
		{
			state.Int = kStateEmpty;
			filter.Int = 0;
			blendInDuration.Float = NORMAL_BLEND_DURATION;
			blendOutDuration.Float = NORMAL_BLEND_DURATION;
			timeToPlay.Int = -1;
			flags.Int = 0;
			ikFlags.Int = 0;
		}

		scrValue state;

		scrValue dict0;
		scrValue clip0;
		scrValue phase0;
		scrValue rate0;
		scrValue weight0;

		scrValue dict1;
		scrValue clip1;
		scrValue phase1;
		scrValue rate1;
		scrValue weight1;

		scrValue dict2;
		scrValue clip2;
		scrValue phase2;
		scrValue rate2;
		scrValue weight2;

		scrValue filter; 
		scrValue blendInDuration; 
		scrValue blendOutDuration;
		scrValue timeToPlay; 
		scrValue flags;
		scrValue ikFlags;

	};

	static const u32	TASK_SCRIPTED_ANIMATION_CLONE_KEEP_INIT_DATA_SLOT	= 0;
	#define MAX_CLIPS_PER_PRIORITY 3	

	struct InitClipData
	{
		InitClipData()
			: dict((u32)0)
			, clip((u32)0)
			, weight(1.0f)
			, phase (0.0f)
			, rate(1.0f)
		{

		}
		atHashWithStringNotFinal dict;
		atHashWithStringNotFinal clip;
		float phase;
		float rate;
		float weight;
	};

	struct InitSlotData{

		InitSlotData()
			: state(kStateEmpty)
			, filter((u32)0)
			, blendInDuration(NORMAL_BLEND_DURATION)
			, blendOutDuration(NORMAL_BLEND_DURATION)
			, timeToPlay(-1)
		{
			flags.BitSet().Reset();
			ikFlags.BitSet().Reset();
		}

		InitSlotData(ScriptInitSlotData& data)
		{
			state = (ePlayBackState)data.state.Int;
			blendInDuration = data.blendInDuration.Float;
			blendOutDuration = data.blendOutDuration.Float;
			timeToPlay = data.timeToPlay.Int;
			flags.BitSet().SetBits(data.flags.Int);
			ikFlags.BitSet().SetBits(data.ikFlags.Int);

			filter.SetHash(data.filter.Int);

			clipData[0].dict.SetFromString(scrDecodeString(data.dict0.String));
			clipData[0].clip.SetFromString(scrDecodeString(data.clip0.String));
			clipData[0].phase = data.phase0.Float;
			clipData[0].rate = data.rate0.Float;
			clipData[0].weight = data.weight0.Float;

			clipData[1].dict.SetFromString(scrDecodeString(data.dict1.String));
			clipData[1].clip.SetFromString(scrDecodeString(data.clip1.String));
			clipData[1].phase = data.phase1.Float;
			clipData[1].rate = data.rate1.Float;
			clipData[1].weight = data.weight1.Float;

			clipData[2].dict.SetFromString(scrDecodeString(data.dict2.String));
			clipData[2].clip.SetFromString(scrDecodeString(data.clip2.String));
			clipData[2].phase = data.phase2.Float;
			clipData[2].rate = data.rate2.Float;
			clipData[2].weight = data.weight2.Float;
		}

		ePlayBackState state;
		InitClipData clipData[MAX_CLIPS_PER_PRIORITY];
		atHashWithStringNotFinal filter; 
		float blendInDuration; 
		float blendOutDuration;
		s32 timeToPlay; 
		eScriptedAnimFlagsBitSet flags;
		eIkControlFlagsBitSet ikFlags;
	};

	struct CCloneData{

		CCloneData()
			: m_cloneDictRefAdded(false)
			, m_cloneStartClipPhase(0.0f)
			, m_clonePostMigrateStartClipPhase(-1.0f)
			, m_cloneClipPhase(-1.0f)
			, m_cloneClipRate(1.0f)
			, m_cloneClipRate2(1.0f)
			, m_clonePhaseControlled(false)
		{

		}
		CCloneData(	float startPhase, bool phaseControlled )
			: m_cloneDictRefAdded(false)
			, m_cloneStartClipPhase(startPhase)
			, m_clonePostMigrateStartClipPhase(-1.0f)
			, m_cloneClipPhase(-1.0f)
			, m_cloneClipRate(1.0f)
			, m_cloneClipRate2(1.0f)
			, m_clonePhaseControlled(phaseControlled)
		{

		}
		float						m_cloneStartClipPhase;
		float						m_clonePostMigrateStartClipPhase;
		float						m_cloneClipPhase;
		float						m_cloneClipRate;
		float						m_cloneClipRate2;
		bool						m_clonePhaseControlled : 1;
		bool						m_cloneDictRefAdded : 1;

		CCloneData& operator=(const CCloneData &rhs) {
			m_cloneStartClipPhase			= rhs.m_cloneStartClipPhase;
			m_cloneClipPhase				= rhs.m_cloneClipPhase;
			m_cloneClipRate					= rhs.m_cloneClipRate;
			m_cloneClipRate2				= rhs.m_cloneClipRate2;
			m_clonePhaseControlled			= rhs.m_clonePhaseControlled;
			return *this;
		}
	};

	// supported playback priority levels
	enum ePlaybackPriority
	{
		kPriorityLow = 0,
		kPriorityMid,
		kPriorityHigh,
		kNumPriorities
	};

	// task states
	enum
	{
		State_Start,
		State_Running,
		State_Exit
	};

	CTaskScriptedAnimation(
		float blendInTime = INSTANT_BLEND_DURATION
		, float blendOutTime = INSTANT_BLEND_DURATION
		);

	//constructor for compatibility with TASK_PLAY_CLIP which allows creating clip tasks in sequences
	CTaskScriptedAnimation(
		atHashWithStringNotFinal clipDictName, 
		atHashWithStringNotFinal clipName, 
		ePlaybackPriority priority, 
		atHashWithStringNotFinal filter, 
		float blendInDelta, 
		float blendOutDelta,
		s32 timeToPlay, 
		eScriptedAnimFlagsBitSet flags, 
		float startPhase,
		bool clonedTask = false,
		bool phaseControlled = false,
		eIkControlFlagsBitSet ikFlags = AIK_NONE,
		bool bAllowOverrideCloneUpdate = false,
		bool bPartOfASequence = false
		);

	//constructor for compatibility with TASK_PLAY_CLIP which allows creating clip tasks in sequences
	CTaskScriptedAnimation(
		atHashWithStringNotFinal clipDictName, 
		atHashWithStringNotFinal clipName, 
		ePlaybackPriority priority, 
		atHashWithStringNotFinal filter, 
		float blendInDelta, 
		float blendOutDelta,
		s32 timeToPlay, 
		eScriptedAnimFlagsBitSet flags, 
		float startPhase,
		const Vector3 &vInitialPosition,
		const Quaternion &InitialOrientationQuaternion,
		bool clonedTask = false,
		bool phaseControlled = false,
		eIkControlFlagsBitSet ikFlags = AIK_NONE,
		bool bAllowOverrideCloneUpdate = false,
		bool bPartOfASequence = false
		);

	CTaskScriptedAnimation(
		 InitSlotData& priorityLow,
		 InitSlotData& priorityMid,
		 InitSlotData& priorityHigh,
		 float blendInTime = NORMAL_BLEND_DURATION,
		 float blendOutTime = NORMAL_BLEND_DURATION,
		 bool clonedTask = false
		);

	CTaskScriptedAnimation(
		ScriptInitSlotData& priorityLow,
		ScriptInitSlotData& priorityMid,
		ScriptInitSlotData& priorityHigh,
		float blendInTime = NORMAL_BLEND_DURATION,
		float blendOutTime = NORMAL_BLEND_DURATION
		);

	~CTaskScriptedAnimation();

	virtual aiTask* Copy() const { 
		CTaskScriptedAnimation* pTask = rage_new CTaskScriptedAnimation(); 
		for (int i=0; i<kNumPriorities; i++)
		{
			pTask->m_slotData[i] = m_slotData[i];
		}
		pTask->m_clip = m_clip;
		pTask->m_fpsClip = m_fpsClip;
		pTask->m_dict = m_dict;
		pTask->m_fpsDict = m_fpsDict;
		pTask->m_blendInDuration = m_blendInDuration;
		pTask->m_blendOutDuration = m_blendOutDuration;
		pTask->m_moveNetworkHelper.TransferNetworkPlayer(m_moveNetworkHelper);

		pTask->m_CloneData  = m_CloneData;

		pTask->m_initialPosition = m_initialPosition;
		pTask->m_initialOrientation = m_initialOrientation;
		pTask->m_physicsSettings = m_physicsSettings;
		pTask->m_firstUpdate = m_firstUpdate;
		pTask->m_exitNextUpdate = m_exitNextUpdate;
		pTask->m_hasSentDeathEvent = m_hasSentDeathEvent;
		pTask->m_bAllowOverrideCloneUpdate = m_bAllowOverrideCloneUpdate;
		pTask->m_bTaskSecondary = m_bTaskSecondary;

		pTask->SetOwningAnimScene(m_owningAnimScene, m_owningAnimSceneEntity);

		return pTask;
	}
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SCRIPTED_ANIMATION; }
	virtual s32				GetDefaultStateAfterAbort()	const {return State_Exit;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void Debug() const;
#endif
	// Clone task implementation for CTaskScriptedAnimation
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	virtual bool                IsInScope(const CPed* pPed);
	virtual bool                OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed));
	virtual bool                OverridesNetworkHeadingBlender(CPed *pPed) { return OverridesNetworkBlender(pPed); }
	virtual bool				IsClonedFSMTask() const { return !IsFlagSet(AF_SECONDARY); }

	bool	CheckRequestDictionaries(const InitSlotData* pData);
	bool	CheckIfClonePlayerNeedsHeadingPositionAdjust();
	void	SetCloneLeaveMoveNetwork() {m_cloneLeaveMoveNetwork = true; }

	void	InitClonesStartPhaseFromBlendedClipsData();
	u32		GetFlagsForSingleAnim();
	u32		GetBoneMaskHash();

	bool	GetQueriableDataForSecondaryBehaviour(	u32							&refFlags,
													float						&refBlendIn,
													float						&refBlendOut,
													u8							&refPriority,
													u32							&refDictHash,
													u32							&refAnimHash);

	virtual void ProcessPreRender2();
	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return ProcessPostFSM();
	virtual bool ProcessPostMovement();
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);	
	void			CleanUp			();


	virtual void DoAbort(const AbortPriority priority, const aiEvent* pEvent);
	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);
	// PURPOSE: Called by ped event scanner when checking dead peds tasks if this returns true a damage event is not generated if a dead ped runs this task
	virtual bool HandlesDeadPed() { return GetDontInterrupt(); }

	inline bool GetDontInterrupt() const { return IsFlagSet(AF_NOT_INTERRUPTABLE); }

	inline bool ShouldForceStart() const { return IsFlagSet(AF_FORCE_START); }

	static void ProcessIkControlFlags(CPed& ped, eIkControlFlagsBitSet& flags);
	static void ProcessIkControlFlagsPreRender(CPed& ped, eIkControlFlagsBitSet& flags);

	// PURPOSE: Set the anim scene and entity that 
	void SetOwningAnimScene(CAnimSceneManager::InstId scene, CAnimSceneEntity::Id entity) { m_owningAnimScene = scene; m_owningAnimSceneEntity = entity; }
	CAnimSceneManager::InstId GetOwningAnimScene() const { return m_owningAnimScene; }
		
	// PURPOSE: Call when aborting the task to remove the entity from it's controlling anim scene (if there is one).
	void RemoveEntityFromControllingAnimScene();

	// PURPOSE: handle adding and removing scripted animation tasks on objects (for consistency across systems).
	static CTaskScriptedAnimation* FindObjectTask(CObject* pObj);
	static void AbortObjectTask(CObject* pObj, CTaskScriptedAnimation* pTask);
	static void GiveTaskToObject(CObject* pObj, CTaskScriptedAnimation* pTask);

	enum 
	{
		Flag_first = BIT0
	};


	// PURPOSE: Blocks the moveblender velocity from being applied, used if the task is playing clips that should control the velocity applied
	//			should probably be an option / based on the specified filter?
	// virtual bool BlockMoveBlenderVelocity() const {return true;}

	//////////////////////////////////////////////////////////////////////////
	//	Playback functions
	//////////////////////////////////////////////////////////////////////////

	inline void StartSingleClip(const char * pDictName, const char * pClipName, ePlaybackPriority priority, float blendInDuration = NORMAL_BLEND_DURATION, float blendOutDuration = NORMAL_BLEND_DURATION, s32 timeToPlay = -1, eScriptedAnimFlagsBitSet flags = AF_NONE, eIkControlFlagsBitSet ikFlags = AIK_NONE )
	{
		//get the clip pointer
		const crClip* pClip = fwAnimManager::GetClipIfExistsByName(pDictName, pClipName);

		if (animVerifyf(pClip, "%s Unable to find clip '%s' in dictionary '%s'", GetEntity()?GetPed()->GetDebugName():"Null Ped", pDictName, pClipName))
		{
			StartSingleClip(pClip, priority, blendInDuration, blendOutDuration, timeToPlay, flags, ikFlags);
		}
	}

	inline void StartSingleClip(atHashWithStringNotFinal dictName, atHashWithStringNotFinal clipName, ePlaybackPriority priority, float blendInDuration = NORMAL_BLEND_DURATION, float blendOutDuration = NORMAL_BLEND_DURATION, s32 timeToPlay = -1, eScriptedAnimFlagsBitSet flags = AF_NONE, eIkControlFlagsBitSet ikFlags = AIK_NONE)
	{
		m_dict = dictName;
		m_clip = clipName;

		s32 iDictIndex = fwAnimManager::FindSlotFromHashKey(dictName.GetHash()).Get();
		//get the clip pointer
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, clipName.GetHash());

		if (animVerifyf(pClip, "%s Unable to find clip %u '%s' in dictionary %u '%s'", GetEntity()?GetPed()->GetDebugName():"Null Ped", clipName.GetHash(), clipName.TryGetCStr(), dictName.GetHash(), dictName.TryGetCStr()))
		{
			StartSingleClip(pClip, priority, blendInDuration, blendOutDuration, timeToPlay, flags, ikFlags);
		}
	}

	inline void StartSingleClip(const crClip* pClip, ePlaybackPriority priority, float blendInDuration = NORMAL_BLEND_DURATION, float blendOutDuration = NORMAL_BLEND_DURATION, s32 timeToPlay = -1, eScriptedAnimFlagsBitSet flags = AF_NONE, eIkControlFlagsBitSet ikFlags = AIK_NONE)
	{
		// send the move signals

		GetSlot(priority).SetState(kStateSingleClip);
		if (m_moveNetworkHelper.IsNetworkActive())
		{
			CEntity *pEntity = static_cast<CEntity *>(GetEntity());

			if (pEntity && pEntity->GetIsTypeObject() && NetworkInterface::IsGameInProgress() && flags.BitSet().IsSet(AF_ADDITIVE))
			{
				m_moveNetworkHelper.SendRequest(GetAdditiveTransitionId(priority));
			}
			else
			{
				m_moveNetworkHelper.SendRequest(GetClipTransitionId(priority));
			}
			m_moveNetworkHelper.SetFloat(GetPriorityWeightId(priority), 1.0f);
		}

		SetBlendInDuration(blendInDuration, priority);

		GetSlot(priority).GetData().GetFlags()=flags;
		GetSlot(priority).GetData().GetIkFlags()=ikFlags;
		GetSlot(priority).GetData().SetBlendOutDuration(blendOutDuration);

		SetClip(pClip, priority);

		// handle the timer
		SetTimeToPlay(timeToPlay, priority);

		// loop?
		SetLooped( GetSlot(priority).GetData().IsFlagSet(AF_LOOPING), priority );
	}

	inline void StartBlend(const crClip* pClip0, const crClip* pClip1, const crClip* pClip2, ePlaybackPriority priority, float blendInDuration = NORMAL_BLEND_DURATION, float blendOutDuration = NORMAL_BLEND_DURATION, s32 timeToPlay = -1, eScriptedAnimFlagsBitSet flags = AF_NONE, eIkControlFlagsBitSet ikFlags = AIK_NONE)
	{
		StartBlend(priority, blendInDuration, blendOutDuration, timeToPlay, flags, ikFlags);

		SetClip(pClip0, priority, 0);
		SetClip(pClip1, priority, 1);
		SetClip(pClip2, priority, 2);
	}

	inline void StartBlend( ePlaybackPriority priority, float blendInDuration = NORMAL_BLEND_DURATION, float blendOutDuration = NORMAL_BLEND_DURATION, s32 timeToPlay = -1, eScriptedAnimFlagsBitSet flags = AF_NONE, eIkControlFlagsBitSet ikFlags = AIK_NONE)
	{

		GetSlot((u8)priority).SetState(kStateBlend);

		if (m_moveNetworkHelper.IsNetworkActive())
		{
			m_moveNetworkHelper.SendRequest(GetBlendTransitionId(priority));
			m_moveNetworkHelper.SetFloat(GetPriorityWeightId(priority), 1.0f);
		}

		SetBlendInDuration(blendInDuration, priority);
		GetSlot(priority).GetData().GetFlags()=flags;
		GetSlot(priority).GetData().GetIkFlags()=ikFlags;
		GetSlot((u8)priority).GetData().SetBlendOutDuration(blendOutDuration);

		// handle the timer
		SetTimeToPlay(timeToPlay, priority);

		// loop?
		SetLooped( GetSlot(priority).GetData().IsFlagSet(AF_LOOPING), priority );
	}

	// PURPOSE: Starts a clip or blend on the given slot using the init data provided.
	//			Note: empty states will be ignored - to stop an clip or blend use StopPlayback().
	void StartPlayback(InitSlotData& data, ePlaybackPriority priority);

	// PURPOSE: Caches the data required to start clip playback on a priority level. For use by script commands
	//			when creating tasks that are going to be played back later as part of a sequence.
	void CachePlayback(InitSlotData& );


	// PURPOSE: Stops playback of any clip on the given slot (either a single clip or a blend)
	// PARAMS:	
	inline void StopPlayback (ePlaybackPriority priority, float blendOutDuration = -1.0f)
	{
		//request a transition to empty
		m_moveNetworkHelper.SendRequest(GetEmptyTransitionId(priority));
		
		if (blendOutDuration<0.0f)
		{
			// use the stored blend out duration
			blendOutDuration = GetSlot(priority).GetData().GetBlendOutDuration();
		}
		
		m_moveNetworkHelper.SetFloat(GetDurationId(priority), blendOutDuration);

		GetSlot(priority).SetState(kStateEmpty);

	}

	//////////////////////////////////////////////////////////////////////////
	//	Gets and sets for script commands
	//////////////////////////////////////////////////////////////////////////

	inline void SetFilter( atHashString filterHash , ePlaybackPriority priority)
	{
		crFrameFilter* pFilter = NULL;

		if (filterHash!=BONEMASK_ALL)
		{
			pFilter = g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(filterHash));
		}

		m_moveNetworkHelper.SetFilter( pFilter, GetFilterId(priority));
	}

	inline void SetPhase( float phase, ePlaybackPriority priority, u8 clipIndex = 0)
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		m_CloneData.m_cloneClipPhase = phase;

		switch(GetSlot(priority).GetState())
		{
		case kStateSingleClip:
			{
				m_moveNetworkHelper.SetFloat(GetPhaseId(priority), phase);
			}
			break;
		case kStateBlend:
			{
				m_moveNetworkHelper.SetFloat(GetBlendPhaseId(priority, clipIndex), phase);
			}
			break;
		default:
			animAssert(0);
			
		}

	}

	inline void SetTime( float time, ePlaybackPriority priority, u8 clipIndex = 0)
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);
		const crClip* pClip = GetSlot(priority).GetData().GetClip(clipIndex);
		if (pClip)
		{
			float phase = pClip->ConvertTimeToPhase(time);
			if (GetSlot(priority).GetData().IsFlagSet(AF_LOOPING))
			{
				float temp;
				phase = Modf(phase, &temp);
			}
			SetPhase(phase, priority, clipIndex);
		}
	}

	inline float GetPhase( ePlaybackPriority priority, u8 clipIndex = 0) const
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		switch(GetSlot(priority).GetState())
		{
		case kStateSingleClip:
			{
				return m_moveNetworkHelper.GetFloat(GetPhaseOutId(priority));
			}
		case kStateBlend:
			{
				return m_moveNetworkHelper.GetFloat(GetBlendPhaseOutId(priority, clipIndex));
			}
		default:
			animAssert(0);
		}

		return 0.0f;
	}

	inline void SetOrigin(const Quaternion& orientation, const Vector3& position)
	{
		m_initialOrientation=orientation;
		m_initialPosition=position;
	}

	inline float GetClipTime( ePlaybackPriority priority, u8 clipIndex = 0) const
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		const crClip* pClip = GetSlot(priority).GetData().GetClip(clipIndex);
		if (pClip)
		{
			return pClip->ConvertPhaseToTime(GetPhase(priority, clipIndex));
		}
		else
		{
			animAssertf(0, "There is no clip playing at this priority");
		}

		return 0.0f;
	}

	inline bool IsActive( ePlaybackPriority priority )
	{
		return (GetSlot(priority).GetState() != kStateEmpty);
	}

	inline void SetRate( float rate, ePlaybackPriority priority, u8 clipIndex = 0)
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		if(GetRate( priority, clipIndex ) != rate)
		{
			if(rate==0.0f)
			{
				m_CloneData.m_cloneClipPhase = GetPhase(priority, clipIndex );  //set the phase to latest when phase is first frozen by rate turning zero
				m_CloneData.m_clonePhaseControlled = true;
			}
			else
			{
				m_CloneData.m_clonePhaseControlled = false;				
			}
			if(clipIndex==0)
			{
				m_CloneData.m_cloneClipRate = rate;
			}
			if(clipIndex==1)
			{
				m_CloneData.m_cloneClipRate2 = rate;
			}
		}

		switch(GetSlot(priority).GetState())
		{
		case kStateSingleClip:
			{
				m_moveNetworkHelper.SetFloat(GetRateId(priority), rate);
			}
			break;
		case kStateBlend:
			{
				m_moveNetworkHelper.SetFloat(GetBlendRateId(priority, clipIndex), rate);
			}
			break;
		default:
			animAssert(0);

		}
	}

	inline float GetRate( ePlaybackPriority priority, u8 clipIndex = 0) const
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		switch(GetSlot(priority).GetState())
		{
		case kStateSingleClip:
			{
				return m_moveNetworkHelper.GetFloat(GetRateOutId(priority));
			}
		case kStateBlend:
			{
				return m_moveNetworkHelper.GetFloat(GetBlendRateOutId(priority, clipIndex));
			}
		default:
			animAssert(0);
		}

		return 0.0f;
	}

	inline void SetLooped( bool looped, ePlaybackPriority priority)
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities);

		if (m_moveNetworkHelper.IsNetworkActive())
		{
			m_moveNetworkHelper.SetBoolean(GetLoopedId(priority), looped);
		}
		
		// keep the flags up to date
		if (looped)
		{
			GetSlot(priority).GetData().SetFlag(AF_LOOPING, true);
		}
		else
		{
			GetSlot(priority).GetData().SetFlag(AF_LOOPING, false);
		}
	}

	inline void SetClip(atHashWithStringNotFinal dictName, atHashWithStringNotFinal clipName, ePlaybackPriority priority, u8 clipIndex=0)
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		s32 dictIndex = fwAnimManager::FindSlotFromHashKey(dictName.GetHash()).Get();

		if (dictIndex!=ANIM_DICT_INDEX_INVALID)
		{
			const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(dictIndex, clipName.GetHash());
			SetClip(pClip, priority, clipIndex);
		}
	}

	inline void SetClip(const crClip* pClip, ePlaybackPriority priority, u8 clipIndex=0)
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		switch(GetSlot(priority).GetState())
		{
		case kStateBlend:
			{
				if (m_moveNetworkHelper.IsNetworkActive())
				{
					m_moveNetworkHelper.SetClip( pClip, GetBlendClipId(priority, clipIndex));
				}
				GetSlot(priority).GetData().SetClip(clipIndex, pClip);
			}
			break;
		case kStateSingleClip:
			{
				animAssertf(clipIndex==0, "Unable to set clip clipIndex %d! This priority level is playing a single clip.", clipIndex);
				if (m_moveNetworkHelper.IsNetworkActive())
				{
					if (CanPlayFpsClip(priority) && ShouldPlayFpsClip())
					{
						m_moveNetworkHelper.SetClip( GetFpsClip(), GetClipId(priority));
						m_bUsingFpsClip = true;
					}
					else
					{
						m_moveNetworkHelper.SetClip( pClip, GetClipId(priority));
						m_bUsingFpsClip = false;
					}					
				}
				GetSlot(priority).GetData().SetClip(0, pClip);
			}
			break;
		default:
			{
				animAssert(0);
			}
			break;
		}
	}

	inline void SetBlend(float blend, ePlaybackPriority priority, u8 clipIndex)
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		switch(GetSlot(priority).GetState())
		{
		case kStateBlend:
			{
				m_moveNetworkHelper.SetFloat(GetBlendWeightId(priority, clipIndex), blend );
			}
			break;
		case kStateSingleClip:
			{
				m_moveNetworkHelper.SetFloat(GetPriorityWeightId(priority), blend);
			}
			break;
		default:
			{
				animAssert(0);
			}
			break;
		}
	}

	inline void SetTimeToPlay(s32 timeInMillisecs, ePlaybackPriority priority)
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities);

		if (timeInMillisecs>=0)
		{
			GetSlot(priority).GetData().GetTimer().Set(fwTimer::GetTimeInMilliseconds(), timeInMillisecs);
		}
		else
		{
			GetSlot(priority).GetData().GetTimer().Unset();
		}
	}

	inline void SetBlendInDuration(float blendDuration, ePlaybackPriority priority)
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities);

		GetSlot(priority).GetData().SetBlendInDuration(blendDuration);
		if (m_moveNetworkHelper.IsNetworkActive())
		{
			m_moveNetworkHelper.SetFloat(GetDurationId(priority), blendDuration);
		}
	}

	inline void SetBlendOutDuration(float blendDuration, ePlaybackPriority priority)
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities);

		GetSlot(priority).GetData().SetBlendOutDuration(blendDuration);
	}


	inline void BlendOut(float blendDuration)
	{
		m_blendOutDuration = blendDuration;
		for (u8 i=0; i<kNumPriorities; i++)
		{
			if (GetSlot(i).GetState()!=kStateEmpty)
			{
				GetSlot(i).GetData().GetTimer().Set(fwTimer::GetTimeInMilliseconds(), 0);
			}
		}
	}

	void ExitNextUpdate() { m_exitNextUpdate = true;}

	//////////////////////////////////////////////////////////////////////////
	//	Generic querying functions for getting phase / rate / etc
	//////////////////////////////////////////////////////////////////////////
	bool IsPlayingClip(atHashWithStringNotFinal dictionaryName, atHashWithStringNotFinal clipName ) const
	{
		ePlaybackPriority priority;
		s32 index;
		return FindFirstInstanceOfClip(dictionaryName, clipName, priority, index);
	}

	bool FindFirstInstanceOfClip(atHashWithStringNotFinal dictionaryName, atHashWithStringNotFinal clipName, ePlaybackPriority& priority, s32& index) const
	{
		// get the clip pointer
		s32 dictIndex = fwAnimManager::FindSlotFromHashKey(dictionaryName.GetHash()).Get();
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(dictIndex, clipName.GetHash());

		if (pClip)
		{
			//search the slot data for this clip
			for (u8 i=0; i<kNumPriorities; i++)
			{
				switch(GetSlot(i).GetState())
				{
				case kStateSingleClip:
					{
						if(GetSlot(i).GetData().GetClip(0)== pClip)
						{
							priority = (ePlaybackPriority)i;
							index = 0;
							return true;
						}
					}
					break;
				case kStateBlend:
					{
						for (int j=0; j<MAX_CLIPS_PER_PRIORITY; j++)
						{
							if(GetSlot(i).GetData().GetClip(j)== pClip)
							{
								priority = (ePlaybackPriority)i;

								animAssertf(!NetworkInterface::IsGameInProgress() || j<2,"Network games only sync first two blended anims. Dict %s [0x%x], Anim %s [0x%x], index = %d",
								dictionaryName.GetCStr(),dictionaryName.GetHash(),clipName.GetCStr(),clipName.GetHash(),j);

								index = j;
								return true;
							}
						}
					}
					break;
				default:
					{
						// nothing to see here...
					}
				}
			}
		}

		return false;
	}

	bool IsFlagSet(u32 flag) const
	{
		for (u8 i=0;i<kNumPriorities;i++)
		{
			if (GetSlot(i).GetData().IsFlagSet((eScriptedAnimFlags)flag))
			{
				return true;
			}
		}
		return false;
	}

	void SetFlag(u32 flag)
	{
		for (u8 i=0;i<kNumPriorities;i++)
		{
			GetSlot(i).GetData().SetFlag((eScriptedAnimFlags)flag, true);
		}
	}

	void ClearFlag(u32 flag)
	{
		for (u8 i=0;i<kNumPriorities;i++)
		{
			GetSlot(i).GetData().SetFlag((eScriptedAnimFlags)flag, false);
		}
	}

	bool IsHeldAtEnd(ePlaybackPriority priority, u8 clipIndex) const
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		const CPrioritySlot& slot = GetSlot(priority);

		if (slot.GetState()!=kStateEmpty)
		{
			return slot.GetData().IsFlagSet(AF_HOLD_LAST_FRAME) && (GetPhase(priority, clipIndex)>=1.0f);
		}
		return false;
	}

	float GetDuration(ePlaybackPriority priority, u8 clipIndex) const
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		const CPrioritySlot& slot = GetSlot(priority);

		if (slot.GetState()!=kStateEmpty)
		{
			return slot.GetData().GetClip(clipIndex) ? slot.GetData().GetClip(clipIndex)->GetDuration() : 0.0f;
		}
		return 0.0f;
	}

	// Hack to work around the cumulative mover track error when playing lengthy clips
	// Repositions and orients the entity based on offset from the stored initial position / orientation
	void ApplyMoverTrackFixup(CPhysical * pPhysical, ePlaybackPriority priority);

	// Store the peds current physics, use gravity and collision detection settings
	// then apply any changes asked for by the task flags
	void ApplyPhysicsStateChanges(CPhysical* pPhysical, const crClip* pClip);

	// Restore the peds physics, use gravity and collision detection settings
	// to the stored settings
	void RestorePhysicsState(CPhysical* pPhysical);

	atHashWithStringNotFinal	GetDict() {return m_dict; }
	atHashWithStringNotFinal	GetClip() {return m_clip; }

	void SetCreateWeaponAtEnd(bool bEquip)  { m_recreateWeapon = bEquip; }
	
	// Set the name of the clip variation (from the same dictionary) to use if in first person mode on the local player
	void SetFPSClipHash(atHashWithStringNotFinal fpsClipHash)  { m_fpsClip = fpsClipHash; }
	
private:

	bool StartNetworkPlayer();

	bool IsTimeToStartBlendOut(ePlaybackPriority priority);

	//clip state signals
	inline fwMvClipId GetClipId( ePlaybackPriority priority ) const { static const atHashValue ms_Id[kNumPriorities] = { ATSTRINGHASH("clip_low",0xDF552448), ATSTRINGHASH("clip_medium",0x6A27382A), ATSTRINGHASH("clip_high",0x6AAEFB78) }; return fwMvClipId(ms_Id[priority]); }
	inline fwMvFloatId GetPhaseId( ePlaybackPriority priority ) const { static const atHashValue ms_Id[kNumPriorities] = { ATSTRINGHASH("phase_low",0xCB932131), ATSTRINGHASH("phase_medium",0x8848CE0A), ATSTRINGHASH("phase_high",0x672A1A52) }; return fwMvFloatId(ms_Id[priority]); }
	inline fwMvFloatId GetRateId( ePlaybackPriority priority ) const{ static const atHashValue ms_Id[kNumPriorities] = { ATSTRINGHASH("rate_low",0x8FA91E), ATSTRINGHASH("rate_medium",0x6C8B94D3), ATSTRINGHASH("rate_high",0xBB106978) }; return fwMvFloatId(ms_Id[priority]); }
	inline fwMvId GetDeltaId( ePlaybackPriority priority ) const { static const atHashValue ms_Id[kNumPriorities] = { ATSTRINGHASH("delta_low",0x6B50D4EE), ATSTRINGHASH("delta_medium",0x88E2B298), ATSTRINGHASH("delta_high",0xDF6A2CD1) }; return fwMvId(ms_Id[priority]); }
	inline fwMvBooleanId GetLoopedId( ePlaybackPriority priority ) const { static const atHashValue ms_Id[kNumPriorities] = { ATSTRINGHASH("loop_low",0x43D73664), ATSTRINGHASH("loop_medium",0x9FB69433), ATSTRINGHASH("loop_high",0xF7859496) }; return fwMvBooleanId(ms_Id[priority]); }
	inline fwMvFilterId GetFilterId( ePlaybackPriority priority ) const { static const atHashValue ms_Id[kNumPriorities] = { ATSTRINGHASH("filter_low",0x93E670C1), ATSTRINGHASH("filter_medium",0x2E13D6B7), ATSTRINGHASH("filter_high",0x1FD31DC1) }; return fwMvFilterId(ms_Id[priority]); }
	inline fwMvFloatId GetDurationId( ePlaybackPriority priority ) const { static const atHashValue ms_Id[kNumPriorities] = { ATSTRINGHASH("duration_low",0xB552F6F0), ATSTRINGHASH("duration_medium",0x809CCEDB), ATSTRINGHASH("duration_high",0x7F070FD0) }; return fwMvFloatId(ms_Id[priority]); }
	inline fwMvFlagId GetUpperBodyIkFlagId( ePlaybackPriority priority ) const { static const atHashValue ms_Id[kNumPriorities] = { ATSTRINGHASH("use_upperbody_ik_low",0xDC85C8FC), ATSTRINGHASH("use_upperbody_ik_medium",0xDF195BA2), ATSTRINGHASH("use_upperbody_ik_high",0x934F8E7B) }; return fwMvFlagId(ms_Id[priority]); }

	inline fwMvFloatId GetPhaseOutId( ePlaybackPriority priority ) const { static const atHashValue ms_Id[kNumPriorities] = { ATSTRINGHASH("phase_out_low",0x900AD3B2), ATSTRINGHASH("phase_out_medium",0x418EF067), ATSTRINGHASH("phase_out_high",0xF5B12AFD) }; return fwMvFloatId(ms_Id[priority]); }
	inline fwMvFloatId GetRateOutId( ePlaybackPriority priority ) const { static const atHashValue ms_Id[kNumPriorities] = { ATSTRINGHASH("rate_out_low",0xC9DCEB75), ATSTRINGHASH("rate_out_medium",0x6CA9B3DC), ATSTRINGHASH("rate_out_high",0xBBE3B6CC) }; return fwMvFloatId(ms_Id[priority]); }
	inline fwMvBooleanId GetLoopedOutId( ePlaybackPriority priority ) const { static const atHashValue ms_Id[kNumPriorities] = { ATSTRINGHASH("looped_out_low",0x1FDB9734), ATSTRINGHASH("looped_out_medium",0x22777431), ATSTRINGHASH("looped_out_high",0xBD9EFCB3) }; return fwMvBooleanId(ms_Id[priority]); }

	inline fwMvBooleanId GetClipEndedId( ePlaybackPriority priority ) const { static const atHashValue ms_Id[kNumPriorities] = { ATSTRINGHASH("clip_ended_low",0x1CFFC0F6), ATSTRINGHASH("clip_ended_medium",0x778CCB7A), ATSTRINGHASH("clip_ended_high",0x736E3DD9) }; return fwMvBooleanId(ms_Id[priority]); }


	// blend state signals

	inline fwMvClipId GetBlendClipId( ePlaybackPriority priority, u8 clipIndex ) const
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		static const fwMvClipId ms_Id[kNumPriorities][MAX_CLIPS_PER_PRIORITY] = { 
			fwMvClipId("clip_0_0",0x5E908CD8), fwMvClipId("clip_0_1",0x4852605C), fwMvClipId("clip_0_2",0x108E70C5),
			fwMvClipId("clip_1_0",0x71FE6A63), fwMvClipId("clip_1_1",0x74194F7), fwMvClipId("clip_1_2",0x74CFF012),
			fwMvClipId("clip_2_0",0x1E5119B4), fwMvClipId("clip_2_1",0x2FFBBD09), fwMvClipId("clip_2_2",0x6791AC34)
		}; 
		return ms_Id[priority][clipIndex]; 
	}

	inline fwMvFloatId GetBlendWeightId( ePlaybackPriority priority, u8 clipIndex ) const
	{ 
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		static const fwMvFloatId ms_Id[kNumPriorities][MAX_CLIPS_PER_PRIORITY] = { 
			fwMvFloatId("weight_0_0",0x7A21E27), fwMvFloatId("weight_0_1",0x87259D2C), fwMvFloatId("weight_0_2",0x89E8A2B2),
			fwMvFloatId("weight_1_0",0xA4352A61), fwMvFloatId("weight_1_1",0xB3E6C9C4), fwMvFloatId("weight_1_2",0x8A487688),
			fwMvFloatId("weight_2_0",0x547C81D0), fwMvFloatId("weight_2_1",0xC5EEE4B3), fwMvFloatId("weight_2_2",0x73CBC06E)
		}; 
		return ms_Id[priority][clipIndex]; 
	}

	inline fwMvFloatId GetBlendPhaseId( ePlaybackPriority priority, u8 clipIndex ) const
	{ 
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		static const fwMvFloatId ms_Id[kNumPriorities][MAX_CLIPS_PER_PRIORITY] = { 
			fwMvFloatId("phase_0_0",0xF004DBBB), fwMvFloatId("phase_0_1",0x9E9DB8EE), fwMvFloatId("phase_0_2",0x90651C7D),
			fwMvFloatId("phase_1_0",0x11D83216), fwMvFloatId("phase_1_1",0x7C11DE8), fwMvFloatId("phase_1_2",0x46899BB0),
			fwMvFloatId("phase_2_0",0x335FD761), fwMvFloatId("phase_2_1",0x4D290AF3), fwMvFloatId("phase_2_2",0x78EE6281)
		}; 
		return ms_Id[priority][clipIndex]; 
	}

	inline fwMvFloatId GetBlendPhaseOutId( ePlaybackPriority priority, u8 clipIndex ) const
	{ 
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		static const fwMvFloatId ms_Id[kNumPriorities][MAX_CLIPS_PER_PRIORITY] = { 
			fwMvFloatId("phase_out_0_0",0x9429C0C6), fwMvFloatId("phase_out_0_1",0xAA18ECA4), fwMvFloatId("phase_out_0_2",0x3A168CA1),
			fwMvFloatId("phase_out_1_0",0x4222AD2A), fwMvFloatId("phase_out_1_1",0xEBD80096), fwMvFloatId("phase_out_1_2",0x5DA66431),
			fwMvFloatId("phase_out_2_0",0x9CA8958), fwMvFloatId("phase_out_2_1",0xADA2D10A), fwMvFloatId("phase_out_2_2",0xBC90EEE6)
		}; 
		return ms_Id[priority][clipIndex]; 
	}


	inline fwMvFloatId GetBlendRateId( ePlaybackPriority priority, u8 clipIndex ) const
	{ 
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		static const fwMvFloatId ms_Id[kNumPriorities][MAX_CLIPS_PER_PRIORITY] = { 
			fwMvFloatId("rate_0_0",0x2F7AACDE), fwMvFloatId("rate_0_1",0x22EA93BE), fwMvFloatId("rate_0_2",0x5538F85A),
			fwMvFloatId("rate_1_0",0x3A89CF8), fwMvFloatId("rate_1_1",0x14AF3F05), fwMvFloatId("rate_1_2",0x5F28D3F3),
			fwMvFloatId("rate_2_0",0x4DDE2F99), fwMvFloatId("rate_2_1",0x382B0433), fwMvFloatId("rate_2_2",0x6944E666)
		}; 
		return ms_Id[priority][clipIndex]; 
	}

	inline fwMvFloatId GetBlendRateOutId( ePlaybackPriority priority, u8 clipIndex ) const
	{ 
		animAssert(priority>=kPriorityLow && priority<kNumPriorities && clipIndex<MAX_CLIPS_PER_PRIORITY);

		static const fwMvFloatId ms_Id[kNumPriorities][MAX_CLIPS_PER_PRIORITY] = { 
			fwMvFloatId("rate_out_0_0",0x6AC87492), fwMvFloatId("rate_out_0_1",0x78901021), fwMvFloatId("rate_out_0_2",0x4C95B82D),
			fwMvFloatId("rate_out_1_0",0xE86D846), fwMvFloatId("rate_out_1_1",0xD6176768), fwMvFloatId("rate_out_1_2",0xB0D71CE8),
			fwMvFloatId("rate_out_2_0",0xAA6EB781), fwMvFloatId("rate_out_2_1",0x7CC3DBE4), fwMvFloatId("rate_out_2_2",0x86E4F026)
		}; 
		return ms_Id[priority][clipIndex]; 
	}

	inline fwMvFloatId GetPriorityWeightId( ePlaybackPriority priority ) const { static const u32 ms_Id[kNumPriorities] = { ATSTRINGHASH("weight_low",0x6599BB2D), ATSTRINGHASH("weight_medium",0xD46EAC44), ATSTRINGHASH("weight_high",0x2D3AF5FF) }; return fwMvFloatId(ms_Id[priority]); }

	//state change requests

	inline fwMvRequestId GetClipTransitionId( ePlaybackPriority priority ) const { static const fwMvRequestId ms_Id[kNumPriorities] = { fwMvRequestId("request_clip_low",0x319FDC4D), fwMvRequestId("request_clip_medium",0xA8332B08), fwMvRequestId("request_clip_high",0x2E8660B) }; return ms_Id[priority]; }
	inline fwMvRequestId GetEmptyTransitionId( ePlaybackPriority priority ) const { static const fwMvRequestId ms_Id[kNumPriorities] = { fwMvRequestId("request_empty_low",0x86A3DAA), fwMvRequestId("request_empty_medium",0xCB01A310), fwMvRequestId("request_empty_high",0x5893C4DF) }; return ms_Id[priority]; }
	inline fwMvRequestId GetBlendTransitionId( ePlaybackPriority priority ) const { static const fwMvRequestId ms_Id[kNumPriorities] = { fwMvRequestId("request_blend_low",0x773E6BF7), fwMvRequestId("request_blend_medium",0xEEB970D1), fwMvRequestId("request_blend_high",0x8E7FA208) }; return ms_Id[priority]; }
	inline fwMvRequestId GetAdditiveTransitionId(ePlaybackPriority priority) const { static const fwMvRequestId ms_Id[kNumPriorities] = { fwMvRequestId(fwMvRequestId::Null()), fwMvRequestId("request_clip_medium_additive",0xD9C10614), fwMvRequestId(fwMvRequestId::Null()) }; return ms_Id[priority]; }

#if __ASSERT
	const char* GetAssertNetEntityName();
#endif
	// State Functions
	// Start
	FSM_Return					Start_OnEnterClone();
	void						Start_OnEnter();
	FSM_Return					Start_OnUpdate();

	void						Running_OnEnter();
	FSM_Return					Running_OnUpdate();
	void						Running_OnExit();

	void						Exit_OnEnterClone();

	CTask::FSM_Return 			RequestCloneClips();
	bool						CloneClipsLoaded();

	//	Process functions for the individual slots, based on thier state.
	//	Should return true if the task needs to continue running, false otherwise.
	bool					ProcessSingleClip(ePlaybackPriority priority);
	bool					ProcessBlend3(ePlaybackPriority priority);

	// Common processing function used by both 3 way blends and single clips
	bool					ProcessCommon(ePlaybackPriority priority);

	const crClip*			GetFpsClip();
	bool					CanPlayFpsClip(ePlaybackPriority priority);
	bool					ShouldPlayFpsClip();
	bool					IsInVehicleFps(); 
#if FPS_MODE_SUPPORTED
	void					ProcessFirstPersonIk(eIkControlFlagsBitSet& flags);
#endif // FPS_MODE_SUPPORTED

	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper m_moveNetworkHelper;

	float m_blendInDuration;
	float m_blendOutDuration;

	// Clip requester helper object for clone
	CClipRequestHelper m_clipRequest[MAX_CLIPS_PER_PRIORITY];
	atHashWithStringNotFinal	m_dict;
	atHashWithStringNotFinal	m_clip;
	atHashWithStringNotFinal	m_fpsDict; // Keep the dictionary around to be used in first person mode.
	atHashWithStringNotFinal	m_fpsClip; // The version of the clip to be used in first person mode.


	CCloneData					m_CloneData;

#if FPS_MODE_SUPPORTED
	CFirstPersonIkHelper* m_apFirstPersonIkHelper[2];
#endif // FPS_MODE_SUPPORTED

	// Flags used to store the peds physics behaviour before running
	// the clip (so it can be restore at the end
	u8				m_physicsSettings;

	bool			m_firstUpdate : 1;
	bool			m_exitNextUpdate : 1;	// set to true to cause the task to exit on its next run
	bool			m_physicsStateChangesApplied : 1;
	mutable bool	m_cloneLeaveMoveNetwork : 1;
	bool			m_bAllowOverrideCloneUpdate : 1;
	bool			m_hasSentDeathEvent : 1;		// Set to true when sending a death event (as a result of the ENDS_IN_DEAD_POSE flag being used)
	bool			m_animateJoints : 1;			// Contains the original state of vehicle m_bAnimatedJoints
	bool			m_recreateWeapon : 1;			// should the ped equip his weapon at the end
	bool			m_bForceResendSyncData : 1;		// Set when changes need to be sent across immediately in network games
	bool			m_bMPHasStoredInitSlotData : 1; // Set when a task is in MP and has kept the clone slot InitSlotData for serialising. Useful to detect when transitioning from SP to MP while holding task running See B*1556121
	bool			m_bTaskSecondary : 1;			// Set to true if the task is running on the secondary task slot 
	bool			m_bUsingFpsClip : 1; // Set when the task is using the first person version of the clip
	bool			m_bClipsAreLoading : 1;			// Set when the clone task is loading clips, used when the task is recreated locally and needs to wait on this to finish
	bool			m_bOneShotFacialBlocked : 1;	// Set when the CMovePed::PlayOneShotFacialClip has been blocked and needs to be cleared in Cleanup.

	enum ePhysicsSettings
	{
		PS_APPLY_REPOSITION			= (1<<3),
		PS_APPLY_REORIENTATION		= (1<<4)
	};

	//reposition and reorient the entity before the clip starts (if PS_APPLY_REPOSITION and PS_APPLY_REORIENTATION are set in the physics flags)
	Vector3 m_initialPosition;
	Quaternion m_initialOrientation;
	float m_startPhase;	// stores the start phase handed to the task for use when extracting the initial offset

	//////////////////////////////////////////////////////////////////////////
	// Anim scene abort
	//////////////////////////////////////////////////////////////////////////
	CAnimSceneManager::InstId m_owningAnimScene;
	CAnimSceneEntity::Id m_owningAnimSceneEntity;

	class CPriorityData
	{
	public:	
		CPriorityData()
			: m_timer()
			, m_blendOutDuration(INSTANT_BLEND_DURATION)
		{
			for (int i=0; i< MAX_CLIPS_PER_PRIORITY; i++)
			{
				m_pClips[i]=NULL;
			}
			Reset();
		}
		~CPriorityData(){  Reset(); }
		void Reset()
		{
			for (int i=0; i< MAX_CLIPS_PER_PRIORITY; i++)
			{
				SetClip(i, NULL);
			}
			m_timer.Unset();
			m_flags.BitSet().Reset();
			SetBlendOutDuration(INSTANT_BLEND_DURATION);
		}

		const crClip* GetClip(int clipIndex) const { return m_pClips[clipIndex]; }
		void SetClip(int clipIndex, const crClip* pClip) {
			if (pClip)
			{
				pClip->AddRef();
			}

			if (m_pClips[clipIndex])
			{
				m_pClips[clipIndex]->Release();
				m_pClips[clipIndex] = NULL;
			}

			m_pClips[clipIndex] = pClip; 
		}

		eScriptedAnimFlagsBitSet& GetFlags(){ return m_flags; }
		const eScriptedAnimFlagsBitSet& GetFlags() const { return m_flags; }

		eIkControlFlagsBitSet& GetIkFlags(){ return m_ikFlags; }
		const eIkControlFlagsBitSet& GetIkFlags() const { return m_ikFlags; }

		bool IsFlagSet(eScriptedAnimFlags flag) const { return m_flags.BitSet().IsSet(flag); }
		void SetFlag(eScriptedAnimFlags flag, bool val){ m_flags.BitSet().Set(flag, val); }

		bool IsIkFlagSet(eIkControlFlags flag) const { return m_ikFlags.BitSet().IsSet(flag); }

		CTaskGameTimer& GetTimer() {return m_timer; }
		CTaskGameTimer GetTimer() const {return m_timer; }

		void SetBlendInDuration( float blendDuration ) { m_blendInDuration = blendDuration; }
		float GetBlendInDuration() const { return m_blendInDuration; }

		void SetBlendOutDuration( float blendDuration ) { m_blendOutDuration = blendDuration; }
		float GetBlendOutDuration() const { return m_blendOutDuration; }

		CPriorityData& operator=(const CPriorityData &rhs) {
			for (int i=0; i<MAX_CLIPS_PER_PRIORITY; i++)
			{
				SetClip(i, rhs.GetClip(i));
			}
			GetFlags() = rhs.GetFlags();
			GetTimer() = rhs.GetTimer();
			SetBlendInDuration(rhs.GetBlendInDuration());
			SetBlendOutDuration(rhs.GetBlendOutDuration());
			return *this;
		}

	protected:
		pgRef<const crClip> m_pClips[MAX_CLIPS_PER_PRIORITY];
		eScriptedAnimFlagsBitSet m_flags;
		eIkControlFlagsBitSet m_ikFlags;
		CTaskGameTimer m_timer;
		float m_blendInDuration;
		float m_blendOutDuration;
	};

	class CPrioritySlot
	{
	public:
		CPrioritySlot()
			: m_state(kStateEmpty)
			, m_data()
			, m_pInitData(NULL)
			, m_NoRefsUsed(false)
		{

		}

		~CPrioritySlot()
		{
			if(m_NoRefsUsed)
			{
				if (m_pInitData)
				{
					delete m_pInitData;
				}
			}
			else
			{
				// make sure we clean up any initialisation data
				SetInitData(NULL);
			}
		}

		CPriorityData& GetData() { return m_data; }

		const CPriorityData& GetData() const { return m_data; }

		ePlayBackState GetState() const {return m_state; }
		void SetState(ePlayBackState state) {m_state = state; m_data.Reset(); }

		CPrioritySlot& operator=(const CPrioritySlot &rhs) {
			m_data = rhs.m_data;
			m_state = rhs.m_state;

			if (rhs.GetInitData())
			{
				CacheInitData(*rhs.GetInitData());
			}

			return *this;
		}

		// Takes a copy of the provided initialisation data (usually given from script)
		// and stores it until it can be used to init the task. Necessary for 
		// running blends, etc in sequences
		void CacheInitData(InitSlotData& data)
		{
			InitSlotData* pCopyData = rage_new InitSlotData();
			*pCopyData = data;
			SetInitData(pCopyData);

			// Flags are often needed when the task first runs (to set up the
			// move network slot / etc)
			GetData().GetFlags()=data.flags;
			GetData().GetIkFlags()=data.ikFlags;
		}

		// Special version of CacheInitData(InitSlotData& data) for use by 
		// clones that do their own managing of the reference to the dictionary/ies
		void CacheInitDataWithoutRef(InitSlotData& data)
		{
			InitSlotData* pCopyData = rage_new InitSlotData();
			*pCopyData = data;
			m_pInitData = pCopyData;

			// Flags are often needed when the task first runs (to set up the
			// move network slot / etc)
			GetData().GetFlags()=data.flags;
			GetData().GetIkFlags()=data.ikFlags;
			m_NoRefsUsed = true;
		}

		// Takes a copy of the provided initialisation data (usually given from script)
		// and stores it until it can be used to init the task. Necessary for 
		// running blends, etc in sequences
		void CacheInitData(ScriptInitSlotData& data)
		{
			InitSlotData* pCopyData = rage_new InitSlotData(data);
			SetInitData(pCopyData);

			// Flags are often needed when the task first runs (to set up the
			// move network slot / etc)
			GetData().GetFlags().BitSet().SetBits(data.flags.Int);
		}

		InitSlotData* GetInitData() const
		{
			return m_pInitData;
		}

		void ClearInitData()
		{
			SetInitData(NULL);
		}

	private:

		void SetInitData(InitSlotData* pData)
		{
			if (pData)
			{
				//ref any dictionaries in case there's a delay before the task starts
				for (s32 i=0; i<MAX_CLIPS_PER_PRIORITY; i++)
				{
					strLocalIndex dictIndex = strLocalIndex(fwAnimManager::FindSlotFromHashKey(pData->clipData[i].dict));
					if (dictIndex.Get()>-1)
					{
						fwAnimManager::AddRef(dictIndex);
					}
				}
			}

			if (m_pInitData)
			{
				//release any dictionaries
				for (s32 i=0; i<MAX_CLIPS_PER_PRIORITY; i++)
				{
					strLocalIndex dictIndex = strLocalIndex(fwAnimManager::FindSlotFromHashKey(m_pInitData->clipData[i].dict));
					if (dictIndex.Get()>-1)
					{
						fwAnimManager::RemoveRefWithoutDelete(dictIndex);
					}
				}
				delete m_pInitData;
			}

			m_pInitData = pData;
		}

		ePlayBackState m_state;
		CPriorityData m_data;

		InitSlotData* m_pInitData;
		bool		  m_NoRefsUsed;
	};

	CPrioritySlot m_slotData[kNumPriorities];
	
	// slot data accessors
	inline CPrioritySlot& GetSlot(ePlaybackPriority priority)
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities);
		return m_slotData[priority];
	}

	inline CPrioritySlot& GetSlot(u8 priority)
	{
		animAssert(priority<kNumPriorities);
		return m_slotData[priority];
	}

	inline const CPrioritySlot& GetSlot(ePlaybackPriority priority) const
	{
		animAssert(priority>=kPriorityLow && priority<kNumPriorities);
		return m_slotData[priority];
	}

	inline const CPrioritySlot& GetSlot(u8 priority) const
	{
		animAssert(priority<kNumPriorities);
		return m_slotData[priority];
	}

	void SetTaskState(s32 iState)
	{
#if __BANK
		if(GetEntity() && GetEntity()->GetType() == ENTITY_TYPE_PED)
		{
			BANK_ONLY(taskDebugf2("Frame : %u - %s%s : %p : changed state from %s:%s:%p to %s. m_dict %s, m_clip %s. m_moveNetworkHelper.HasNetworkExpired %s",
				fwTimer::GetFrameCount(),
				GetPed()->IsNetworkClone() ? "Clone ped : " : "Local ped : ",
				GetPed()->GetDebugName(),
				GetPed(),
				GetTaskName(),
				GetStateName(GetState()),
				this,
				GetStateName(iState),
				m_dict.TryGetCStr(),
				m_clip.TryGetCStr(),
				m_moveNetworkHelper.HasNetworkExpired()?"TRUE":"FALSE"));
		}
#endif
		SetState(iState);
	}
#if __BANK
public:
#define		MAX_ANIM_NAME_LEN	 64
#define		MAX_DICT_NAME_LEN	 64
	static char		ms_NameOfAnim[MAX_ANIM_NAME_LEN];
	static char		ms_NameOfDict[MAX_DICT_NAME_LEN];

	static bool		ms_bankOverrideCloneUpdate;
	static bool		ms_bankUseSecondaryTaskTree;
	static bool		ms_advanced;

	static bool		ms_AF_UPPERBODY;
	static bool		ms_AF_SECONDARY;
	static bool		ms_AF_LOOPING;
	static bool		ms_AF_NOT_INTERRUPTABLE;
	static bool		ms_AF_USE_KINEMATIC_PHYSICS;
	static bool		ms_AF_USE_MOVER_EXTRACTION;
	static bool		ms_AF_EXTRACT_INITIAL_OFFSET;

	static bool		ms_AIK_DISABLE_LEG_IK;

	static float	ms_fPlayRate;
	static float	ms_fStartPhase;
	static float	ms_fBlendIn;
	static float	ms_fBlendOut;

	static void InitWidgets();
	static CPed* GetFocusPed();
	static void StartTestAnim();
	static void StreamTestDict();
#endif
};

//-------------------------------------------------------------------------
// Task info for running a script clip
//-------------------------------------------------------------------------
class CClonedScriptedAnimationInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedScriptedAnimationInfo();
	CClonedScriptedAnimationInfo(	s32 state,
									float blendInDuration,
                                    float blendOutDuration,
                                    u32 flags,
									bool disableLegIK,
									atHashWithStringNotFinal m_dict,
									atHashWithStringNotFinal m_clip,
									float				startClipPhase,
									float				clipPhase,
									float				clipRate,
									float				clipRate2,
									const Vector3&		vInitialPosition,
									const Quaternion&	initialOrientation,
									bool				setOrientationPosition,
									u32					boneMaskHash,
									bool				phaseControlled);

	CClonedScriptedAnimationInfo(	s32 state,
									const CTaskScriptedAnimation::InitSlotData* priorityLow,
									float				blendInTime,
									float				blendOutTime,
									float				clipPhase,
									float				clipRate,
									float				clipRate2,
									bool				phaseControlled ,
									float				startClipPhase);

	~CClonedScriptedAnimationInfo() {}

	virtual s32 GetTaskInfoType( )	const	{return INFO_TYPE_SCRIPTED_ANIMATION;}
    virtual int GetScriptedTaskType() const { return SCRIPT_TASK_PLAY_ANIM; }

	u32			GetClipDictHash()			{ return m_clipDictHash;  }
	u32			GetClipHashKey()			{ return m_clipHashKey;    }
	float		GetClipBlendInDuration()	{ return m_blendInDuration; }
	float		GetClipBlendOutDuration()	{ return m_blendOutDuration; }
	u32			GetClipFlags()				{ return reinterpret_cast<s32&>(m_flags);      }
	float		GetClipPhase()				{ return m_ClipPhase; }
	float		GetClipRate()				{ return m_ClipRate; }
	float		GetClipRate2()				{ return m_ClipRate2; }

	bool		GetBlend()					{ return (m_syncSlotClipstate == CTaskScriptedAnimation::kStateBlend); }
	bool		GetDisableLegIK()			{ return m_disableLegIK; }

	virtual CTaskFSMClone *CreateCloneFSMTask();
    virtual CTask         *CreateLocalTask(fwEntity *pEntity);

	virtual bool    HasState()			const { return true; }
	virtual u32		GetSizeOfState()	const { return datBitsNeeded<CTaskScriptedAnimation::State_Exit>::COUNT;}
	virtual const char*	GetStatusName(u32)	const { return "State";}

#define MAX_CLIPS_SYNCED_PER_PRIORITY (MAX_CLIPS_PER_PRIORITY)  //the maximum used by the only time we sync blended anims in the GOLF game. See "PROC PLAY_BLENDED_ANIMATION" in golf_anims_lib.sch

	struct SyncClipData
	{
		SyncClipData()
			: dict(0)
			, clip(0)
			, weight(1.0f)
		{

		}
		u32 dict;
		u32 clip;
		float weight;
	};

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		//SERIALISE_ANIMATION_ANIM(serialiser, m_clipDictHash, m_clipHashKey, "Animation");  //Note this doesn't serialise the dictionary hash for the receiver it just uses it to creat an index internally

		SERIALISE_UNSIGNED(serialiser, m_syncSlotClipstate, SIZEOF_BLEND_STATE, "Priority Blend State");

		if(m_syncSlotClipstate == CTaskScriptedAnimation::kStateBlend || serialiser.GetIsMaximumSizeSerialiser())
		{ //when serializing blend aims - currently only expect one priority slot (low) to be used
			for( unsigned i=0; i<MAX_CLIPS_SYNCED_PER_PRIORITY; i++)
			{
				SERIALISE_UNSIGNED(serialiser, m_syncSlotClipData[i].clip, SIZEOF_DICT_HASH, "Blend Dict hash");
				SERIALISE_UNSIGNED(serialiser, m_syncSlotClipData[i].dict, SIZEOF_CLIP_HASH, "Blend  Anim hash");
				SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_syncSlotClipData[i].weight, 1.0f, SIZEOF_CLIP_WEIGHT, "Blend  Clip weight");
			}
			SERIALISE_UNSIGNED(serialiser, m_syncSlotClipflags, SIZEOF_FLAGS, "Blend Flags");
		}
		else
		{
			SERIALISE_UNSIGNED(serialiser, m_clipDictHash, SIZEOF_DICT_HASH, "Dictionary hash");
			SERIALISE_UNSIGNED(serialiser, m_clipHashKey, SIZEOF_CLIP_HASH, "Anim hash");

			SERIALISE_ANIM_BLEND(serialiser, m_blendInDuration, "Blend In duration");
			SERIALISE_ANIM_BLEND(serialiser, m_blendOutDuration, "Blend Out duration");

			SERIALISE_UNSIGNED(serialiser, reinterpret_cast<s32&>(m_flags), SIZEOF_FLAGS, "Flags");
		}


		if(m_startClipPhase !=0.0f)
		{
			m_setStartPhase = true;
		}
		else
		{
			m_setStartPhase = false;
		}

		m_bDefaultBoneMaskHash = m_BoneMaskHash ==0;
		bool bBoneMaskDefault = m_bDefaultBoneMaskHash;
		SERIALISE_BOOL(serialiser, bBoneMaskDefault, "Bone mask default");
		m_bDefaultBoneMaskHash = bBoneMaskDefault;

		if(!bBoneMaskDefault)
		{
			SERIALISE_UNSIGNED(serialiser, m_BoneMaskHash, SIZEOF_BONEMASK_HASH, "Bone mask hash");
		}
		else
		{
			m_BoneMaskHash = 0;
		}

		bool disableLegIK = m_disableLegIK;
		SERIALISE_BOOL(serialiser, disableLegIK, "Disable Leg IK");
		m_disableLegIK = disableLegIK;

		bool setStartPhase = m_setStartPhase;
		SERIALISE_BOOL(serialiser, setStartPhase, "Set Start phase");
		m_setStartPhase = setStartPhase;

		bool setPhase = m_setPhase;
		SERIALISE_BOOL(serialiser, setPhase, "Set phase");
		m_setPhase = setPhase;

		bool setRate = m_setRate;
		SERIALISE_BOOL(serialiser, setRate, "Set rate");
		m_setRate = setRate;

		bool setRate2 = m_setRate2;
		SERIALISE_BOOL(serialiser, setRate2, "Set rate 2");
		m_setRate2 = setRate2;

		bool setOrientationPosition = m_setOrientationPosition;
		SERIALISE_BOOL(serialiser, setOrientationPosition, "Set Orientation position");
		m_setOrientationPosition = setOrientationPosition;

		if( m_setOrientationPosition  || serialiser.GetIsMaximumSizeSerialiser() )
		{
			SERIALISE_QUATERNION(serialiser, m_InitialOrientation,	SIZEOF_INITIAL_ORIENTATION_QUAT,	"Init Orientation");
			SERIALISE_VECTOR(serialiser, m_vInitialPosition,	WORLDLIMITS_XMAX,	SIZEOF_INITIAL_POSITION,	"Init Position");
		}

		if (m_setStartPhase || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_startClipPhase, 1.0f, SIZEOF_START_CLIP_PHASE, "Start Clip Phase");
		}
		else
		{
			m_startClipPhase=0.0f;
		}

		if(m_setPhase || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_ClipPhase, 1.0f, SIZEOF_CLIP_PHASE, "Clip Phase");
		}
		else
		{
			m_ClipPhase = 0.0f;
		}

		if(m_setRate || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_ClipRate, 4.0f, SIZEOF_CLIP_RATE, "Clip Rate");
		}
		else
		{
			m_ClipRate =1.0f;
		}

		if(m_setRate2 || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_ClipRate2, 4.0f, SIZEOF_CLIP_RATE, "Clip Rate 2");
		}
		else
		{
			m_ClipRate2 =1.0f;
		}
	}

private:

	static const unsigned int SIZEOF_BLEND_STATE				= datBitsNeeded<CTaskScriptedAnimation::kStateBlend>::COUNT;
	static const unsigned int SIZEOF_BLEND_FILTER_HASH			= 32;

	static const unsigned int SIZEOF_BLEND_PACKED				= 8;
	static const unsigned int SIZEOF_BONEMASK_HASH				= 32;
	static const unsigned int SIZEOF_CLIP_HASH					= 32;
	static const unsigned int SIZEOF_DICT_HASH					= 32;
	static const unsigned int SIZEOF_START_CLIP_PHASE			= 8;
	static const unsigned int SIZEOF_CLIP_PHASE					= 8;
	static const unsigned int SIZEOF_CLIP_RATE					= 8;
	static const unsigned int SIZEOF_CLIP_WEIGHT				= 8;
	static const unsigned int SIZEOF_FLAGS						= eScriptedAnimFlags_MAX_VALUE + 1;
	static const unsigned int SIZEOF_INITIAL_ORIENTATION_QUAT	= 19;
	static const unsigned int SIZEOF_INITIAL_POSITION			= 32;

	static u32		PackBlendDuration(float blendDuration);
	static float	UnpackBlendDuration(u32 blendDuration);

	u32		m_BoneMaskHash;
	u32		m_clipDictHash;
	u32		m_clipHashKey;
	float	m_blendInDuration;
	float	m_blendOutDuration;
	u32		m_flags;
	float	m_startClipPhase;
	float	m_ClipPhase;
	float	m_ClipRate;
	float	m_ClipRate2;

	Quaternion	m_InitialOrientation;
	Vector3		m_vInitialPosition;

	SyncClipData m_syncSlotClipData[MAX_CLIPS_SYNCED_PER_PRIORITY];
	u32			 m_syncSlotClipflags;
	u8			 m_syncSlotClipstate;

	bool	m_disableLegIK : 1;
	bool	m_setStartPhase : 1;
	bool	m_setPhase : 1;
	bool	m_setRate : 1;
	bool	m_setRate2 : 1;
	bool	m_setOrientationPosition : 1;
	bool	m_bDefaultBoneMaskHash : 1;
};


#endif // TASK_SCRIPTED_ANIMATION_H
