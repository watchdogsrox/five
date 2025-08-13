#ifndef TASK_ANIMS_H
#define TASK_ANIMS_H

// Game headers

#if __BANK
#include "animation/debug/AnimDebug.h"
#endif //__BANK
#include "fwanimation/animdirector.h"
#include "network/General/NetworkSerialisers.h"
#include "network/General/NetworkSynchronisedScene.h"
#include "Peds/QueriableInterface.h"
#include "scene/Physical.h"
#include "peds/ped.h"
#include "system/controlMgr.h"
#include "task/Animation/AnimTaskFlags.h"
#include "task/Animation/TaskScriptedAnimation.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskHelpers.h"
 
class CClipPlayer;

#if FPS_MODE_SUPPORTED
class CFirstPersonIkHelper;
#endif // FPS_MODE_SUPPORTED

//-------------------------------------------------------------------------------------------------
// 
// CTaskClip
// 
// Base class. Provides Common interface to clips tasks.
// Also provides common destructor and MakeAbortable function
//
//-------------------------------------------------------------------------------------------------
class CTaskClip : public CTaskFSMClone
{
public:

	enum eClipTaskFlag
	{
		// Set to true when the task has finished
		ATF_TASK_FINISHED = (1<<0),					

		// Hold the last frame of the clip
		ATF_HOLD_LAST_FRAME = (1<<1),

		// Don't allow some events to abort the task
		ATF_DONT_INTERRUPT = (1<<2),

		// Sync this clip over the network
		ATF_DONT_SYNC_OVER_NETWORK = (1<<3),

		// Blend out quickly if we abort
		ATF_FAST_BLEND_OUT_ON_ABORT = (1<<4),

		// Run as a secondary task
		ATF_SECONDARY = (1<<5),

		// Run as a secondary task
		ATF_RUN_IN_SEQUENCE = (1<<6),

		// Translate the ped when the task finishes
		ATF_OFFSET_PED = (1<<7),

		// Orient the ped when the task finishes
		ATF_ORIENT_PED = (1<<8),

		// Delay the blend out one frame when the task finishes
		ATF_DELAY_BLEND_WHEN_FINISHED = (1<<9),

		//Turn off collision detection during the clip
		ATF_TURN_OFF_COLLISION = (1<<10),

		//override physics forces during the clip
		ATF_OVERRIDE_PHYSICS = (1<<11),

		//ignore gravity during the clip
		ATF_IGNORE_GRAVITY = (1<<12),

		//extract the authored initial offset from the clip file
		ATF_EXTRACT_INITIAL_OFFSET = (1<<13),

		//exit the task when coming back from being aborted (otherwise a looped clip will restart)
		ATF_EXIT_AFTER_INTERRUPTED = (1<<14),

		//Use physics to drive the skeleton to the clip position
		ATF_DRIVE_TO_POSE = (1<<15),

		// Internal use only. Identify the first time the update function is called
		ATF_FIRST_UPDATE = (1<<16),

		//Loop the clip
		ATF_LOOP = (1<<17),

		//Loop the clip
		ATF_FORCE_START = (1<<18),

		//Use the kinematic physics mode
		ATF_USE_KINEMATIC_PHYSICS = (1<<19),

		//Ignore mover rotation blendout.
		ATF_IGNORE_MOVER_BLEND_ROTATION = (1<<20)
	};

	CTaskClip();
	virtual ~CTaskClip();

	CClipHelper* GetClipPlayer() {return GetClipHelper();}
	CClipHelper* GetClipToBeNetworked() { if (!(m_iTaskFlags & ATF_DONT_SYNC_OVER_NETWORK)) return GetClipHelper(); return NULL; }

	// Get/Set/Query a specific clip flag (See eClipPlayerFlags in ClipDefines.h)
	void SetClipFlag(const int iFlag) {m_iClipFlags |= iFlag;}
    void SetClipFlags(const u32 iClipFlags) { m_iClipFlags = iClipFlags; }
	void UnsetClipFlag(const int iFlag) {if(IsClipFlagSet(iFlag)) m_iClipFlags &= ~iFlag;}
	bool IsClipFlagSet(const int iFlag) const {return ((m_iClipFlags & iFlag) ? true : false);}

	// Get/Set/Query a specific clip flag (See eClipTaskFlag in TaskClips.h)
	void SetTaskFlag(const int iFlag) {m_iTaskFlags |= iFlag;}
	void UnsetTaskFlag(const int iFlag) {if(IsTaskFlagSet(iFlag)) m_iTaskFlags &= ~iFlag;}
	bool IsTaskFlagSet(const int iFlag) const {return ((m_iTaskFlags & iFlag) ? true : false);}

	// Get/Set Functions
	bool GetSyncOverNetwork() const { return !IsTaskFlagSet(ATF_DONT_SYNC_OVER_NETWORK); }
	void SetSyncOverNetwork(bool b) { b ? UnsetTaskFlag(ATF_DONT_SYNC_OVER_NETWORK) : SetTaskFlag(ATF_DONT_SYNC_OVER_NETWORK); }

	bool GetFastBlendOnAbort() const { return IsTaskFlagSet(ATF_FAST_BLEND_OUT_ON_ABORT); }
	void SetFastBlendOnAbort(bool b) { b ? SetTaskFlag(ATF_FAST_BLEND_OUT_ON_ABORT) : UnsetTaskFlag(ATF_FAST_BLEND_OUT_ON_ABORT); }

	float GetStartPhase()				{ return m_fStartPhase; }
	void SetStartPhase(float fPhase)	{ m_fStartPhase = fPhase; }

	float GetRate()						{ return m_fRate; }
	void SetRate(float fRate)			{ m_fRate = fRate; }

	float GetBlendDelta()					{ return m_fBlendDelta; }
	void SetBlendDelta(float fBlendDelta)	{ m_fBlendDelta = fBlendDelta; }

	float GetBlendOutDelta()					{ return m_fBlendOutDelta; }
	void SetBlendOutDelta(float fBlendOutDelta)	{ m_fBlendOutDelta = fBlendOutDelta; }


	//int GetQuitEvent()						{ return m_iQuitEvent; }
	//void SetQuitEvent( s32 iQuitEvent )	{ m_iQuitEvent = iQuitEvent; }

	int GetTaskTypeInternal() const					{ return m_eTaskType; }
	void SetTaskType(int taskType)			{ m_eTaskType = (CTaskTypes::eTaskType)taskType; }
	
	const fwMvClipSetId& GetClipSetId() const { return m_clipSetId; }
	const fwMvClipId& GetClipId() const { return m_clipId; }

	// PURPOSE: Updates the root transformation (the position and orientation of the entity at the start of the clip)
	//			when playing clips using the mover track fixup and absolute animated position.
	void UpdateClipRootTransform(const Matrix34 &transform);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	// Hack to work around the cumulative mover track error when playing lengthy clips
	// Repositions and orients the entity based on offset from the stored initial position / orientation
	void ApplyMoverTrackFixup(CPhysical * pPhysical);

	bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	// Clean up
	void CleanUp();

	// PURPOSE: Return the clip helper if the clip is currently playing
	const CClipHelper* GetClipIfPlaying() { return GetClipHelper(); }

	inline bool ShouldForceStart() const { return IsTaskFlagSet(ATF_FORCE_START); }

protected:

	bool GetDontInterrupt() const { return IsTaskFlagSet(ATF_DONT_INTERRUPT); }

	// Print out all the information we have
	void PrintDebugInfo();

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	FSM_Return ProcessPreFSM();

	// Store the peds current physics, use gravity and collision detection settings
	// then apply any changes asked for by the task flags
	void ApplyPhysicsStateChanges(CPhysical* pPhysical, const crClip* pClip);

	// Restore the peds physics, use gravity and collision detection settings
	// to the stored settings
	void RestorePhysicsState(CPhysical* pPhysical);

	// Need to add ref our clip pointer locally because its possible for a controlling script to 
	// terminate whilst the task is still waiting to be run in a sequence. As a result the clip
	// dictionary gets streamed out before it can be used.
	void SetReferenceClip(const crClip* pClip);

	// Returns the time remaining (duration minus running time)
	int GetRemainingDuration(); 

	// Returns the flags needed to stop the clip.
	u32 GetStopClipFlags() const;
	
	// Task flags
	s32 m_iTaskFlags;

	// Task type
	CTaskTypes::eTaskType m_eTaskType;

	// blend in delta
	float m_fBlendDelta;

	// blend in delta
	float m_fBlendOutDelta;

	// clip rate
	float m_fRate;

	// clip start phase
	float m_fStartPhase;

	// clip priority
	u32	m_iPriority;

	// clip flags
	u32	m_iClipFlags;

	// boneMask
	u32	m_iBoneMask;

	// quit the task if this event occurs in the event track of the clip
	//s32 m_iQuitEvent; 

	// quit the task after duration milliseconds
	CTaskGameTimer m_timer;
	int m_iDuration;

	//reposition and reorient the entity before the clip starts (if PS_APPLY_REPOSITION and PS_APPLY_REORIENTATION are set in the physics flags)
	Vector3 m_initialPosition;
	Quaternion m_initialOrientation;

	// Clip set and id 
	// We can use these to get the clip data, the clip priority and the clip flags
	fwMvClipSetId m_clipSetId;
	fwMvClipId m_clipId;

	// Clip slot index and clip hash key
	// We can use these to get the clip data 
	// We need to get the clip priority and the clip flags separately
	int m_clipDictIndex;
	atHashWithStringNotFinal m_clipHashKey;

	// Flags used to store the peds physics behaviour before running
	// the clip (so it can be restore at the end
	u8	m_physicsSettings;

    // Clip requester helper object
    CClipRequestHelper m_clipRequest;
	
	enum ePhysicsSettings
	{
		PS_USE_EXTRACTED_Z		= (1<<0),
		PS_USES_COLLISION		= (1<<2),
		PS_APPLY_REPOSITION		= (1<<3),
		PS_APPLY_REORIENTATION	= (1<<4)
	};

	enum eTerminationBehavior
	{
		TB_DO_NOTHING,
		TB_DELAY_BLEND_OUT_ONE_FRAME,
		TB_OFFSET_PED
	};

	u8 m_iTerminationBehavior;

	// for local referencing
	pgRef<const crClip> m_pClip;
};

//-------------------------------------------------------------------------------------------------
// 
// CTaskRunClip
// 
// Basic task to play an clip using either the clip set id and clip id 
// or the clip slot index and clip key
// 
//-------------------------------------------------------------------------------------------------
class CTaskRunClip : public CTaskClip
{
public:
	CTaskRunClip(
		const fwMvClipSetId &clipSetId, 
		const fwMvClipId &clipId, 
		const float fBlendInDelta, 
		const float fBlendOutDelta, 
		const s32 iDuration,
		const s32 iTaskFlags,
		const CTaskTypes::eTaskType taskType = CTaskTypes::TASK_RUN_CLIP);

	CTaskRunClip(
		const int clipSlotIndex,
		const u32 clipHashKey,
		const float fBlendDelta,
		const s32 animPriority,
		const s32 clipFlags,
		const u32 boneMask,
		const s32 iDuration,
		const s32 iTaskFlags,
		const CTaskTypes::eTaskType taskType = CTaskTypes::TASK_RUN_CLIP);

	virtual aiTask*			Copy() const;

	//void					SetQuitEvent( s32 iQuitEvent ) { m_iQuitEvent = iQuitEvent; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	// FSM required implementations
	virtual FSM_Return		UpdateFSM	(const s32 iState, const FSM_Event iEvent);

    // Clone task implementation
	// Returns true below as it always takes its state from the remote machine
	virtual CTaskInfo*		CreateQueriableState() const;
	virtual void			ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return		UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
    virtual void            OnCloneTaskNoLongerRunningOnOwner();

	// Play an clip
	FSM_Return				Start_OnUpdate			(CPhysical* pPhysical);
	FSM_Return				PlayingClip_OnUpdate	(CPhysical* pPhysical);
	void					PlayingClip_OnExit		(CPhysical* pPhysical);

	virtual s32				GetDefaultStateAfterAbort()	const {return State_Finish;}
	virtual bool			MayDeleteOnAbort() const { return true; }	// This task doesn't resume, OK to delete if aborted.

	enum
	{
		State_Start = 0,
		State_PlayingClip,
		State_Finish
	};

protected:

	void StartClip(CPhysical* pPhysical);
};

//-------------------------------------------------------------------------------------------------
// 
// CTaskRunNamedClip
//
// Task to play an clip using the clip dictionary name and clip name
// Used by script.
// Provides functionality to reposition and/or reorient the entity at the end of the clip
//
//-------------------------------------------------------------------------------------------------
class CTaskRunNamedClip : public CTaskClip
{
	friend class CClonedScriptClipInfo;
public:

	CTaskRunNamedClip(
		const strStreamingObjectName pClipDictName,
		const char* pClipName,
		const u32 iPriority,
		const s32 iClipFlags,
		const u32 iBoneMask,
		const float fBlendDelta,
		const s32 iDuration,
		const s32 iTaskFlags,
		const float fStartPhase);

	CTaskRunNamedClip(
		const strStreamingObjectName pClipDictName,
		const char* pClipName,
		const u32 iPriority,
		const s32 iClipFlags,
		const u32 iBoneMask,
		const float fBlendDelta,
		const s32 iDuration,
		const s32 iTaskFlags, 
		const float fStartPhase,
		const Vector3 &vInitialPosition,
		const Quaternion &InitialOrientationQuaternion);

	CTaskRunNamedClip(
		s32 clipDictIndex,
		u32 clipHashKey,
		const u32 iPriority,
		const s32 flags,
		const u32 boneMask,
		const float fBlendDelta,
		const int iDuration = -1,
		const s32 iTaskFlags = 0,
		const float fStartPhase = 0.0f,
		const bool bIsCloneTask = false);

	void CleanUp();

	virtual aiTask* Copy() const;

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	// FSM required implementations
	virtual FSM_Return		UpdateFSM	(const s32 iState, const FSM_Event iEvent);

	// FSM optional implementations
	virtual FSM_Return		ProcessPreFSM();

	// PURPOSE: Called by ped event scanner when checking dead peds tasks if this returns true a damage event is not generated if a dead ped runs this task
	virtual bool HandlesDeadPed() { return GetDontInterrupt(); }

    // Clone task implementation
	// Returns true below as it always takes its state from the remote machine
	virtual CTaskInfo*		CreateQueriableState() const;
	virtual void			ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return		UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
    virtual void            OnCloneTaskNoLongerRunningOnOwner();
	
	// Play an clip
	void		Start_OnEnterClone		(CPhysical* pPhysical);
    FSM_Return	Start_OnUpdate			(CPhysical* pPhysical);
    FSM_Return	Start_OnUpdateClone		(CPhysical* pPhysical);
	void		PlayingClip_OnEnter		(CPhysical* pPhysical);
	FSM_Return	PlayingClip_OnUpdate	(CPhysical* pPhysical);
	void		PlayingClip_OnExit		(CPhysical* pPhysical);

	virtual s32				GetDefaultStateAfterAbort()	const {return IsClipFlagSet(APF_ISLOOPED) && !IsTaskFlagSet(ATF_EXIT_AFTER_INTERRUPTED) ? State_Start : State_Finish;}

	virtual bool ProcessPostMovement();

	enum
	{
		State_Start = 0,
		State_PlayingClip,
		State_Finish
	};

protected:

	enum eTerminationBehavior
	{
		TB_DO_NOTHING,
		TB_DELAY_BLEND_OUT_ONE_FRAME = (1<<0),
		TB_OFFSET_PED = (1<<1),
	};

	crFrame m_PoseMatchFrame;
	u8 m_iTerminationBehavior;

	void StartClip(CPhysical* pPhysical);

	// When the clip has finished delay the blend out for one frame
	void DelayBlendOut(CPhysical* pPhysical);

	// When the clip has finished offset the entity
	void OffsetEntityPosition(CPhysical* pPhysical);

	float ComputeMatchingPose(const fragInst& frag, const phArticulatedCollider& collider);
	float ComputeMatchingPoseHelper(float timeBegin, float timeEnd, float errorBegin, float errorEnd, unsigned maxSamples, const fragInst& frag, const phArticulatedCollider& collider);
	float ComputePoseError(float poseTime, const fragInst& frag, const phArticulatedCollider& collider);

protected:

	CTaskRunNamedClip():CTaskClip(){}
};

//-------------------------------------------------------------------------
// Task info for running a script clip
//-------------------------------------------------------------------------
class CClonedScriptClipInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedScriptClipInfo();
	CClonedScriptClipInfo(s32 nState, s32 nDictID, u32 nClipHash, float fBlendIn, u8 iPriority, u32 iFlags);
	virtual ~CClonedScriptClipInfo() {}

	virtual s32 GetTaskInfoType() const { return INFO_TYPE_SCRIPT_ANIM; }
	
	int		GetClipSlotIndex()		{ return m_slotIndex;		}
	u32		GetClipHashKey()		{ return m_hashKey;			}
	float	GetClipBlendDelta()		{ return UnpackBlendDelta(m_nBlendDelta); }
	u32		GetClipPriority()		{ return m_iPriority;		}
	u32		GetClipFlags()			{ return m_iClipFlags;      }
	
	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual CTaskFSMClone *CreateLocalTask(fwEntity* pEntity);

    virtual bool	HasState() const { return true; }
    virtual u32		GetSizeOfState() const { return datBitsNeeded<CTaskRunNamedClip::State_Finish>::COUNT; }
    virtual const char*	GetStatusName(u32) const { return "State"; }

    void Serialise(CSyncDataBase& serialiser)
    {
        CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_ANIMATION_ANIM(serialiser, m_slotIndex, m_hashKey, "Animation");
		SERIALISE_UNSIGNED(serialiser, m_nBlendDelta, SIZEOF_BLEND, "Blend In Delta Packed");
		SERIALISE_UNSIGNED(serialiser, m_iClipFlags, SIZEOF_FLAGS, "Flags");
		SERIALISE_UNSIGNED(serialiser, m_iPriority, SIZEOF_CLIPPRIORITY, "Priority");
    }

protected:
	enum
	{
		SCI_BLEND_4,
		SCI_BLEND_8,
		SCI_BLEND_16,
		SCI_BLEND_INSTANT,
	};

	static const unsigned int SIZEOF_BLEND         = datBitsNeeded<SCI_BLEND_INSTANT>::COUNT;;
    static const unsigned int SIZEOF_CLIPPRIORITY  = 2;
    static const unsigned int SIZEOF_FLAGS         = 31;

    static u8		PackBlendDelta(float blendDelta);
	static float	UnpackBlendDelta(u8 blendDelta);

	s32				m_slotIndex;
	u32				m_hashKey;
	u32				m_iClipFlags;
	u8				m_nBlendDelta;
	u8				m_iPriority;
};

#if __BANK

//////////////////////////////////////////////////////////////////////////
//	MoVE test network parameter hookup widgets
//////////////////////////////////////////////////////////////////////////

class CMoveParameterWidget : public datBase
{
public:

	CMoveParameterWidget():
	  m_type(kParameterWidgetNum),
	  m_group(NULL),
	  m_move(NULL),
	  m_network(NULL),
	  m_requiresSave(false),
	  m_perFrameUpdate(false),
	  m_outputMode(false)
	{
		//nothing to do here
	}

	enum ParameterWidgetType
	{
		kParameterWidgetReal = 0,
		kParameterWidgetBool,
		kParameterWidgetFlag,
		kParameterWidgetRequest,
		kParameterWidgetEvent,
		kParameterWidgetClip,
		kParameterWidgetExpression,
		kParameterWidgetFilter,

		kParameterWidgetNum	// this must always be at the end of the enum
	};

	static const char * ms_widgetTypeNames[kParameterWidgetNum]; //definition of this should match the enum above

	virtual void AddWidgets(bkBank* bank) = 0;

	virtual void DeleteWidgets()
	{
		if (m_group)
		{
			m_group->Destroy();
			m_group = NULL;
		}
	}

	static const char ** GetWidgetTypeNames() { return &ms_widgetTypeNames[0]; }

	inline void WidgetUpdated()	{	m_requiresSave = true;	}

	// in case atString is causing issues
	inline const char * GetName() { return m_name.c_str(); }

	void Update()
	{
		if (m_outputMode)
		{
			Read();
		}
		else if (m_requiresSave || m_perFrameUpdate)
		{
			Save();
			m_requiresSave = false;
		}
	}

	virtual void Save() = 0;

	virtual void Read() 
	{
		// nothing to do here
	}

	ParameterWidgetType GetType() {return m_type;}


	ParameterWidgetType m_type;
	atString		m_name;
	bkWidget*		m_group;
	fwMove*			m_move;
	fwMoveNetworkPlayer*	m_network;
	bool			m_requiresSave;
	bool			m_perFrameUpdate;
	bool			m_outputMode;
};

class CMoveParameterWidgetReal : public CMoveParameterWidget
{
public:
	CMoveParameterWidgetReal(
		fwMove* move, 
		fwMoveNetworkPlayer* network, 
		const char * paramName, 
		bool bUpdateEveryFrame = false, 
		float minValue = 0.0f, 
		float maxValue = 1.0f,
		bool bOutputParameter = false,
		int controlElement = 0
		)
	{
		m_type = kParameterWidgetReal;
		m_name = paramName;
		m_move = move;
		m_network = network;
		m_value = 0.0f;
		m_perFrameUpdate = bUpdateEveryFrame;
		m_min = minValue;
		m_max = maxValue;
		m_controlValue = controlElement;
		if (m_controlValue)
		{
			m_perFrameUpdate = true; // force this on if we're controlling with an axis
		}

		m_outputMode = bOutputParameter;
	}

	void AddWidgets(bkBank* bank)
	{
		m_group = (bkWidget*)bank->AddSlider(m_name, &m_value, m_min, m_max, (m_max - m_min) / 100.0f,
			datCallback(MFA(CMoveParameterWidget::WidgetUpdated), (datBase*)this));
	}

	void Save();

	void Read()
	{
		m_value = m_network->GetFloat( fwMvFloatId( GetName() ));
	}


	float m_value;
	float m_min;
	float m_max;
	int m_controlValue; //the axis / analogue button assigned to this value
};

class CMoveParameterWidgetFlag : public CMoveParameterWidget
{
public:
	CMoveParameterWidgetFlag( fwMove* move, fwMoveNetworkPlayer* network, const char * paramName, int controlElement)
	{
		m_type = kParameterWidgetFlag;
		m_name = paramName;
		m_move = move;
		m_network = network;
		m_value = false;
		m_controlValue = controlElement; 
	}

	void AddWidgets(bkBank* bank)
	{
		m_group = (bkWidget*)bank->AddToggle(m_name, &m_value, datCallback(MFA(CMoveParameterWidget::WidgetUpdated), (datBase*)this));
	}

	void Save();

	bool m_value;
	int m_controlValue; 
};

class CMoveParameterWidgetBool : public CMoveParameterWidget
{
public:
	CMoveParameterWidgetBool(
		fwMove* move, 
		fwMoveNetworkPlayer* network, 
		const char * paramName, 
		bool bUpdateEveryFrame = false,
		bool bOutputParameter = false,
		int controlElement = 0
		)
	{
		m_type = kParameterWidgetBool;
		m_name = paramName;
		m_move = move;
		m_network = network;
		m_value = false;
		m_perFrameUpdate = bUpdateEveryFrame;
		m_outputMode = bOutputParameter;
		m_controlValue = controlElement; 
		if (m_controlValue)
		{
			m_perFrameUpdate = true; // force this on if we're controlling with an axis
		}
	}

	void AddWidgets(bkBank* bank)
	{
		m_group = (bkWidget*)bank->AddToggle(m_name, &m_value, datCallback(MFA(CMoveParameterWidget::WidgetUpdated), (datBase*)this));
	}

	void Save();

	void Read()
	{
		m_value = m_network->GetBoolean( fwMvBooleanId( GetName() ));
	}

	bool m_value;
	int m_controlValue; //the axis / analogue button assigned to this value
};

class CMoveParameterWidgetRequest : public CMoveParameterWidget
{
public:
	CMoveParameterWidgetRequest(  fwMove* move, fwMoveNetworkPlayer* handle, const char * paramName, int controlElement)
	{
		m_type = kParameterWidgetRequest;
		m_name = paramName;
		m_move = move;
		m_network = handle;
		m_controlValue = controlElement;
		if (m_controlValue)
		{
			m_perFrameUpdate = true; // force this on if we're controlling with an axis
		}
	}

	void AddWidgets(bkBank* bank)
	{
		m_group = (bkWidget*)bank->AddButton(m_name, datCallback(MFA(CMoveParameterWidget::WidgetUpdated), (datBase*)this));
	}

	void Save();

	bool m_value;
	int m_controlValue; //the axis / analogue button assigned to this value
};

class CMoveParameterWidgetEvent : public CMoveParameterWidget
{
public:
	CMoveParameterWidgetEvent(  fwMove* move, fwMoveNetworkPlayer* handle, const char * paramName, int controlElement)
	{
		m_type = kParameterWidgetEvent;
		m_name = paramName;
		m_move = move;
		m_network = handle;
		m_controlValue = controlElement;
		if (m_controlValue)
		{
			m_perFrameUpdate = true; // force this on if we're controlling with an axis
		}
	}

	void AddWidgets(bkBank* bank)
	{
		m_group = (bkWidget*)bank->AddButton(m_name, datCallback(MFA(CMoveParameterWidget::WidgetUpdated), (datBase*)this));
	}

	void Save();

	bool m_value;
	int m_controlValue; //the axis / analogue button assigned to this value
};

class CMoveParameterWidgetClip : public CMoveParameterWidget
{
public:

	CMoveParameterWidgetClip( fwMove* move, fwMoveNetworkPlayer* handle, const char * paramName, bool bUpdateEveryFrame = false);

	virtual ~CMoveParameterWidgetClip();

	void BuildClipMap(atMap< atHashValue, atHashValue > &clipMap, const fwMvClipSetId &clipSetId);
	void GetClipSetClips(int &clipCount, const char **&clips, const fwMvClipSetId &clipSetId);

	void AddWidgets(bkBank *bank);
	void AddVariableClipsetWidgets(bkBank *bank);
	void AddAbsoluteClipsetWidgets(bkBank *bank);
	void AddClipDictionaryWidgets(bkBank *bank);
	void AddFilePathWidgets(bkBank *bank);

	void Save();

	void OnSourceChanged();
	void OnVariableClipsetChanged();
	void OnAbsoluteClipset_ClipsetChanged();
	void OnAbsoluteClipset_ClipChanged();
	void OnAbsoluteClipsetChanged();
	void OnClipDictionary_ClipDictionaryChanged();
	void OnClipDictionary_ClipChanged();
	void OnClipDictionaryChanged();
	void OnFilePathChanged();

	enum SOURCE
	{
		eVariableClipset,
		eAbsoluteClipset,
		eClipDictionary,
		eFilePath,
		NUM_SOURCES,
	};

	// Data

	const crClip *m_clip;
	char m_clipTextString[256];
	bkText *m_clipText;

	int m_sourceIndex;

	char m_variableClipset_Clipset[256];
	char m_variableClipset_Clip[256];

	int m_absoluteClipset_ClipsetIndex;
	int m_absoluteClipset_ClipsetCount;
	const char **m_absoluteClipset_Clipsets;
	int m_absoluteClipset_ClipIndex;
	int m_absoluteClipset_ClipCount;
	const char **m_absoluteClipset_Clips;

	int m_clipDictionary_ClipDictionaryIndex;
	int m_clipDictionary_ClipDictionaryCount;
	const char **m_clipDictionary_ClipDictionaries;
	int m_clipDictionary_ClipIndex;
	int m_clipDictionary_ClipCount;
	const char **m_clipDictionary_Clips;

	char m_filePath_Clip[256];

	// Static data

	static const char *ms_sources[];
};

class CMoveParameterWidgetExpression : public CMoveParameterWidget
	{
public:

	CMoveParameterWidgetExpression( fwMove* move, fwMoveNetworkPlayer* handle, const char * paramName, bool bUpdateEveryFrame = false);

	virtual ~CMoveParameterWidgetExpression();

	void AddWidgets(bkBank *bank);
	void AddExpressionDictionaryWidgets(bkBank *bank);
	void AddFilePathWidgets(bkBank *bank);

	void Save();

	void OnSourceChanged();
	void OnExpressionDictionary_ExpressionDictionaryChanged();
	void OnExpressionDictionary_ExpressionChanged();
	void OnExpressionDictionaryChanged();
	void OnFilePathChanged();

	enum SOURCE
		{
		eExpressionDictionary,
		eFilePath,
		NUM_SOURCES,
	};

	// Data

	const crExpressions *m_expressions;
	char m_expressionsTextString[256];
	bkText *m_expressionsText;

	int m_sourceIndex;

	int m_expressionDictionary_ExpressionDictionaryIndex;
	int m_expressionDictionary_ExpressionDictionaryCount;
	const char **m_expressionDictionary_ExpressionDictionaries;
	int m_expressionDictionary_ExpressionsIndex;
	int m_expressionDictionary_ExpressionsCount;
	const char **m_expressionDictionary_Expressionss;

	char m_filePath_Expression[256];

	// Static data

	static const char *ms_sources[];
};

class CMoveParameterWidgetFilter : public CMoveParameterWidget
	{
public:

	CMoveParameterWidgetFilter( fwMove* move, fwMoveNetworkPlayer* handle, const char * paramName, bool bUpdateEveryFrame = false);

	virtual ~CMoveParameterWidgetFilter();

	void AddWidgets(bkBank *bank);
	void AddBoneFilterManagerWidgets(bkBank *bank);
	void AddFilePathWidgets(bkBank *bank);

	void Save();

	void OnSourceChanged();
	void OnBoneFilterManagerChanged();
	void OnFilePathChanged();

	enum SOURCE
	{
		eBoneFilterManager,
		eFilePath,
		NUM_SOURCES,
	};

	// Data

	crFrameFilter *m_filter;
	char m_filterTextString[256];
	bkText *m_filterText;

	int m_sourceIndex;

	int m_boneFilterManager_FilterIndex;
	int m_boneFilterManager_FilterCount;
	const char **m_boneFilterManager_Filters;

	char m_filePath_Filter[256];

	// Static data

	static const char *ms_sources[];
};


//////////////////////////////////////////////////////////////////////////
//	FSM task to handle previewing a MoVE network on a ped
//////////////////////////////////////////////////////////////////////////

class CTaskPreviewMoveNetwork : public CTask
{
public:
	CTaskPreviewMoveNetwork(const char * fileName);
	CTaskPreviewMoveNetwork(const mvNetworkDef* networkDef);
	virtual ~CTaskPreviewMoveNetwork();

	enum ePreviewMoveNetworkState
	{
		State_Start,
		State_PreviewNetwork,
		State_Exit
	};

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM( const s32 iState, const FSM_Event iEvent);

	virtual s32	GetDefaultStateAfterAbort() const { return State_Exit; }
	virtual aiTask*	Copy() const;
	virtual int		GetTaskTypeInternal() const { return CTaskTypes::TASK_PREVIEW_MOVE_NETWORK; }

	// FSM optional functions
	virtual	void		CleanUp			();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );

	void LoadParamWidgets();
	void SaveParamWidgets();
#endif

	// Parameter widget support methods
	void AddWidgets(bkBank* pBank);
	void ClearParamWidgets();
	void RemoveWidgets();
	void AddParameterHookup(CMoveParameterWidget* pParam);

	// Clip set support methods
	void ApplyClipSet(const fwMvClipSetId &clipSetId, const fwMvClipSetVarId &clipSetVarId = CLIP_SET_VAR_ID_DEFAULT);

	fwMoveNetworkPlayer* GetNetworkPlayer() { return m_moveNetworkHelper.GetNetworkPlayer(); }

private:
	// initial state
	FSM_Return					Start_OnUpdate			();

	void						PreviewNetwork_OnEnter			();
	FSM_Return					PreviewNetwork_OnUpdate			();
	void						PreviewNetwork_OnExit			();

	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper	m_moveNetworkHelper;

	// A pointer to the definition object for the network we're previewing
	const mvNetworkDef*		m_networkDef;

	// Network filename (used when loading and saving param widgets)
	atString m_networkFileName;

	// List of parameter widgets for this object
	atArray<CMoveParameterWidget*> m_widgetList;

	// VEHICLE SPECIFIC
	//--------------------------------------------------
	bool			m_animateJoints;

public:

	enum TaskSlotFlags {
		Task_None = 0,
		Task_TagSyncContinuous = BIT0,
		Task_TagSyncTransition = BIT1,
		Task_DontDisableParamUpdate = BIT2,
		Task_IgnoreMoverBlendRotation = BIT3,
		Task_IgnoreMoverBlend = BIT4
	};

	enum SecondarySlotFlags {
		Secondary_None = 0,
		Secondary_TagSyncContinuous = BIT0,
		Secondary_TagSyncTransition = BIT1,
	};

	static float sm_blendDuration;

	static CDebugFilterSelector sm_filterSelector;

	static bool		sm_secondary;
	static bool		sm_BlockMovementTasks;

	static bool sm_Task_TagSyncContinuous;
	static bool sm_Task_TagSyncTransition;
	static bool sm_Task_DontDisableParamUpdate;
	static bool sm_Task_IgnoreMoverBlendRotation;
	static bool sm_Task_IgnoreMoverBlend;

	static bool sm_Secondary_TagSyncContinuous;
	static bool sm_Secondary_TagSyncTransition;
	

};

//////////////////////////////////////////////////////////////////////////
//	FSM task to create a repro for a tricky MoVE / task related bug
//////////////////////////////////////////////////////////////////////////

class CTaskTestMoveNetwork : public CTask
{
public:
	enum
	{
		State_Start,
		State_RunningTest,
		State_Exit
	};
	CTaskTestMoveNetwork() {};

	~CTaskTestMoveNetwork(){};

	virtual aiTask* Copy() const { return rage_new CTaskTestMoveNetwork(); }
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_NONE; }
	virtual s32				GetDefaultStateAfterAbort()	const {return State_Exit;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif

	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);	
	void			CleanUp			();

private:
	// State Functions
	// Start
	FSM_Return					Start_OnUpdate();
	// running the scene
	FSM_Return					RunningTest_OnUpdate();

	CMoveNetworkHelper m_parentNetwork;

	CMoveNetworkHelper m_childNetwork;

	float m_previousPhase;

	static const fwMvFlagId ms_FlagId;
	static const fwMvFloatId ms_PhaseId;
	static const fwMvRequestId ms_RequestId;
	static const fwMvFloatId ms_DurationId;
	static const fwMvNetworkId ms_Subnetwork0Id;

public:

	static float ms_ReferencePhase;
	static float ms_TransitionDuration;
};

#endif //__BANK

//-------------------------------------------------------------------------

class CTaskSynchronizedScene : public CTaskFSMClone
{
public:

	static const eSyncedSceneFlagsBitSet SYNCED_SCENE_NONE;
	static const eRagdollBlockingFlagsBitSet RBF_NONE;

	enum
	{
		State_Start,
		State_Homing,
		State_RunningScene,
		State_Exit
	};

	CTaskSynchronizedScene( 
		fwSyncedSceneId scene, 
		u32 clipPartialHash, 
		strStreamingObjectName dictionary, 
		float blendIn = INSTANT_BLEND_IN_DELTA, 
		float blendOut = INSTANT_BLEND_OUT_DELTA,
		eSyncedSceneFlagsBitSet flags = SYNCED_SCENE_NONE, 
		eRagdollBlockingFlagsBitSet ragdollBlockingFlags = RBF_NONE,
		float moverBlendIn = INSTANT_BLEND_IN_DELTA,
        int networkSceneID = -1,
		eIkControlFlagsBitSet ikFlags = CTaskScriptedAnimation::AIK_NONE
		);

    CTaskSynchronizedScene(const CTaskSynchronizedScene &taskToCopy);

	~CTaskSynchronizedScene();

	virtual aiTask* Copy() const
    {
        CTaskFSMClone *pNewTask = rage_new CTaskSynchronizedScene(m_scene, m_clipPartialHash, m_dictionary, m_blendInDelta, m_blendOutDelta, m_flags, m_ragdollBlockingFlags, m_moverBlendRate, m_NetworkSceneID, m_ikFlags);

        if(GetNetSequenceID() != INVALID_TASK_SEQUENCE_ID)
        {
            pNewTask->SetNetSequenceID(GetNetSequenceID());
        }

        return pNewTask;
    }

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_SYNCHRONIZED_SCENE; }
	virtual s32				GetDefaultStateAfterAbort()	const {return State_Exit;}

    int GetNetworkSceneID() const { return m_NetworkSceneID; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	void Debug() const;
#endif

	FSM_Return ProcessPreFSM();

	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);
	void			CleanUp			();

	virtual void ProcessPreRender2();

	void ExitSceneNextUpdate() { m_ExitScene = true; }

    // Clone task implementation
	// Returns true below as it always takes its state from the remote machine
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
    virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
    virtual bool                IsInScope(const CPed* UNUSED_PARAM(pPed)) { return true; }
    virtual bool                OverridesNetworkBlender(CPed *UNUSED_PARAM(pPed)) { return true; }
	virtual bool                ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType));
    virtual bool                OverridesVehicleState() const	{ return true; }
	virtual bool                IgnoresCloneTaskPriorities() const;
    virtual bool                RequiresTaskInfoToMigrate() const { return false; }

	virtual bool				ProcessPhysics(float timestep, int nTimeSlice);
	virtual bool				ProcessPostMovement();

	// Determine whether or not the task should abort
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);

	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	void SetBlendOutDelta(float blendOutDelta) { m_blendOutDelta = blendOutDelta; }

	fwSyncedSceneId GetScene() const { return m_scene; }
	const eSyncedSceneFlagsBitSet &GetSynchronizedSceneFlags() const { return m_flags; }
	bool IsSceneFlagSet(eSyncedSceneFlags flag) const { return m_flags.BitSet().IsSet(flag);} 
	bool IsRagdollBlockingFlagSet(eRagdollBlockingFlags flag) const { return m_ragdollBlockingFlags.BitSet().IsSet(flag);} 

	u32 GetClipPartialHash() const { return m_clipPartialHash; }
	const strStreamingObjectName GetClip() const { return atFinalizeHash(m_clipPartialHash); }
	const strStreamingObjectName &GetDictionary() const { return m_dictionary; }

	float GetVehicleLightingScalar() const { return m_fVehicleLightingScalar; }

private:
	// State Functions
	// Start
	FSM_Return					Start_OnUpdate();
	
	void						Homing_OnEnter();
	FSM_Return					Homing_OnUpdate();

	// running the scene
	void						RunningScene_OnEnter();
	FSM_Return					RunningScene_OnUpdate();
	void						RunningScene_OnExit();
	// quitting the scene
	FSM_Return					QuitScene_OnUpdate();

	void						RestorePhysicsState(CPed* pPed);

	//	helper function to handle interpolating the mover
	void						InterpolateMover();

	//  helper function to apply the ragdoll blocking flags to the ped
	void						ApplyRagdollBlockingFlags(CPed* pPed);

	//	helper function to calculate the animated velocity from the clip directly.
	//	synced scenes play back with a rate of 0.0 so Move doesn't give us the velocity.
	void						CalculateAnimatedVelocities(Vector3& velocityOut, Vector3& angularVelocityOut);

	void						UpdateNetworkPedRateCompensate(CPed* pPed, fwAnimDirectorComponentSyncedScene *animDirectorComponentSyncedScene );

#if FPS_MODE_SUPPORTED
	//  helper function to process first person ik
	void						ProcessFirstPersonIk(eIkControlFlagsBitSet& flags);
#endif // FPS_MODE_SUPPORTED

	// helper function to grab the ped pointer if applicable (this task can be used on vehicles and objects as well)
	CPed* TryGetPed() 
	{ 
		if (GetPhysical() && GetPhysical()->GetIsTypePed())
		{
			return static_cast<CPed*>(GetPhysical());
		}
		else
		{
			return NULL;
		}
	}

	fwSyncedSceneId m_scene;
	u32							m_clipPartialHash;
	strStreamingObjectName		m_dictionary;
	float						m_blendInDelta;				// The rate the synchronized scene clip should blend in at.
	float						m_blendOutDelta;			// The rate the synchronized scene clip should blend in at.
	eSyncedSceneFlagsBitSet		m_flags;					// synchronized scene flags
	eRagdollBlockingFlagsBitSet	m_ragdollBlockingFlags;		// ragdoll blocking flags
	eIkControlFlagsBitSet		m_ikFlags;					// ik blocking flags
    int                         m_NetworkSceneID;           // network synchronised scene ID (used if this is part of a network synced scene)
	float						m_fVehicleLightingScalar;	// 0.0 = outside vehicle, 1.0 = in vehicle.  Set by anim event tag.

#if FPS_MODE_SUPPORTED
	CFirstPersonIkHelper*		m_apFirstPersonIkHelper[2];
#endif // FPS_MODE_SUPPORTED

	//	mover cross blend data
	Quaternion			m_startOrientation;	// The initial orientation of the entity
	Vector3				m_startPosition;	// The initial position of the entity
	Vector3				m_startVelocity;	// The initial movement speed of the entity
	float				m_moverBlend;
	float				m_moverBlendRate;

	bool				m_DisabledMapCollision : 1;
	bool				m_DisabledGravity : 1;
	bool				m_ExitScene : 1;
	bool				m_StartSituationValid : 1;
	bool				m_NetworkCoordinateScenePhase : 1;

    u32                 m_EstimatedCompletionTime; // the estimated network time the animation will hit phase 1.0f (it may reset to 0.0f afterwards if the animation is looped). Only valid in network games

	u32					m_FrameMoverLastProcessed;

	Vector2				m_startMoveBlendRatio; //maintain the initial move blend ratio when lerping the mover.

	static const char *ms_facialClipSuffix;
};

//-------------------------------------------------------------------------
// Task info for running a synchronised scene
//-------------------------------------------------------------------------
class CClonedSynchronisedSceneInfo : public CSerialisedFSMTaskInfo
{
public:

    CClonedSynchronisedSceneInfo(int networkSceneID, u32 estimatedCompletionTime) : m_NetworkSceneID(networkSceneID), m_EstimatedCompletionTime(estimatedCompletionTime) {}
    CClonedSynchronisedSceneInfo() {}

	virtual s32 GetTaskInfoType() const { return INFO_TYPE_SYNCHRONISED_SCENE; }

	int GetNetworkSceneID()          const { return m_NetworkSceneID;  }
    u32 GetEstimatedCompletionTime() const { return m_EstimatedCompletionTime; }

    void Serialise(CSyncDataBase& serialiser)
    {
        CSerialisedFSMTaskInfo::Serialise(serialiser);

        SERIALISE_INTEGER(serialiser, m_NetworkSceneID, CNetworkSynchronisedScenes::SIZEOF_SCENE_ID, "Network Scene ID");

        const unsigned SIZEOF_ESTIMATED_COMPLETION_TIME = 32;
        SERIALISE_UNSIGNED(serialiser, m_EstimatedCompletionTime, SIZEOF_ESTIMATED_COMPLETION_TIME, "Estimated Completion Time");
    }

protected:

	int m_NetworkSceneID;
    u32 m_EstimatedCompletionTime; // the estimated network time the animation will hit phase 1.0f (it may reset to 0.0f afterwards if the animation is looped)
};

//-------------------------------------------------------------------------

#endif // TASK_ANIMS_H
