#ifndef TASK_MOVE_SCRIPTED_H
#define TASK_MOVE_SCRIPTED_H

// Rage headers
#include "vector/quaternion.h"

// Game headers
#include "animation/AnimDefines.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Peds/QueriableInterface.h"
#include "peds/ped.h"

#if FPS_MODE_SUPPORTED
class CFirstPersonIkHelper;
#endif // FPS_MODE_SUPPORTED

class CTaskMoVEScripted : public CTaskFSMClone
{
public:
	friend class CClonedTaskMoVEScriptedInfo;
	static const float DEFAULT_FLOAT_SIGNAL_LERP_RATE;
	static const float FLOAT_SIGNAL_LERP_EPSILON;

	// Keep structure in sync with commands_task.sch STRUCT MOVE_INITIAL_PARAMETERS
	struct ScriptInitialParameters
	{
		ScriptInitialParameters()
		{
			clipSetHash0.Int = 0;
			variableClipSetHash0.Int = 0;

			clipSetHash1.Int = 0;
			variableClipSetHash1.Int = 0;

			floatParamName0.String = NULL;
			floatParamValue0.Float = 0.0f;
			floatParamLerpValue0.Float = -1.0f;

			floatParamName1.String = NULL;
			floatParamValue1.Float = 0.0f;
			floatParamLerpValue1.Float = -1.0f;

			boolParamName0.String = NULL;
			boolParamValue0.Int = 0;

			boolParamName1.String = NULL;
			boolParamValue1.Int = 0;
		}

		// Initial Clipsets
		scrValue clipSetHash0;
		scrValue variableClipSetHash0;

		scrValue clipSetHash1;
		scrValue variableClipSetHash1;

		// Initial Float Params
		scrValue floatParamName0;
		scrValue floatParamValue0;
		scrValue floatParamLerpValue0;

		scrValue floatParamName1;
		scrValue floatParamValue1;
		scrValue floatParamLerpValue1;
		
		// Initial Bool Params
		scrValue boolParamName0;
		scrValue boolParamValue0;

		scrValue boolParamName1;
		scrValue boolParamValue1;
	};

	struct InitialParameters
	{
		InitialParameters()
		{
			m_clipSet0				= CLIP_SET_ID_INVALID;
			m_varClipSet0			= CLIP_SET_VAR_ID_INVALID;
			m_clipSet1				= CLIP_SET_ID_INVALID;
			m_varClipSet1			= CLIP_SET_VAR_ID_INVALID;

			m_floatParamId0			= FLOAT_ID_INVALID;
			m_floatParamValue0		= 0.0f;
			m_floatParamLerpValue0	= -1.0f;
			m_floatParamId1			= FLOAT_ID_INVALID;
			m_floatParamValue1		= 0.0f;
			m_floatParamLerpValue1	= -1.0f;

			m_boolParamId0			= BOOLEAN_ID_INVALID;
			m_boolParamValue0		= false;
			m_boolParamId1			= BOOLEAN_ID_INVALID;
			m_boolParamValue1		= false;
		}

		InitialParameters(InitialParameters* pOtherParams)
		{
			if (!Verifyf(pOtherParams, "pOtherParams should not be null"))
			{
				return;
			}

			m_clipSet0				= pOtherParams->m_clipSet0;
			m_varClipSet0			= pOtherParams->m_varClipSet0;
			m_clipSet1				= pOtherParams->m_clipSet1;
			m_varClipSet1			= pOtherParams->m_varClipSet1;

			m_floatParamId0			= pOtherParams->m_floatParamId0;
			m_floatParamValue0		= pOtherParams->m_floatParamValue0;
			m_floatParamLerpValue0	= pOtherParams->m_floatParamLerpValue0;
			m_floatParamId1			= pOtherParams->m_floatParamId1;
			m_floatParamValue1		= pOtherParams->m_floatParamValue1;
			m_floatParamLerpValue1	= pOtherParams->m_floatParamLerpValue1;

			m_boolParamId0			= pOtherParams->m_boolParamId0;
			m_boolParamValue0		= pOtherParams->m_boolParamValue0;
			m_boolParamId1			= pOtherParams->m_boolParamId1;
			m_boolParamValue1		= pOtherParams->m_boolParamValue1;
		}

		InitialParameters(ScriptInitialParameters* pScriptInitParams)
		{
			if (!Verifyf(pScriptInitParams, "pScriptInitParams should not be null"))
			{
				return;
			}

			// Clipsets
			m_clipSet0 = pScriptInitParams->clipSetHash0.Int != 0 ? fwMvClipSetId(pScriptInitParams->clipSetHash0.Int) : CLIP_SET_ID_INVALID;
			m_clipSet1 = pScriptInitParams->clipSetHash1.Int != 0 ? fwMvClipSetId(pScriptInitParams->clipSetHash1.Int) : CLIP_SET_ID_INVALID;
			m_varClipSet0 = pScriptInitParams->variableClipSetHash0.Int != 0 ? fwMvClipSetVarId(pScriptInitParams->variableClipSetHash0.Int) : CLIP_SET_VAR_ID_DEFAULT;
			m_varClipSet1 = pScriptInitParams->variableClipSetHash1.Int != 0 ? fwMvClipSetVarId(pScriptInitParams->variableClipSetHash1.Int) : CLIP_SET_VAR_ID_DEFAULT;

			// Floats
			if (pScriptInitParams->floatParamName0.String != NULL) {
				m_floatParamId0 = fwMvFloatId(scrDecodeString(pScriptInitParams->floatParamName0.String));
			}
			else {
				m_floatParamId0 = FLOAT_ID_INVALID;
			}

			if (pScriptInitParams->floatParamName1.String != NULL) {
				m_floatParamId1 = fwMvFloatId(scrDecodeString(pScriptInitParams->floatParamName1.String));
			}
			else {
				m_floatParamId1 = FLOAT_ID_INVALID;
			}

			m_floatParamValue0 = pScriptInitParams->floatParamValue0.Float;
			m_floatParamValue1 = pScriptInitParams->floatParamValue1.Float;

			m_floatParamLerpValue0 = pScriptInitParams->floatParamLerpValue0.Float;
			m_floatParamLerpValue1 = pScriptInitParams->floatParamLerpValue1.Float;

			// Booleans
			if (pScriptInitParams->boolParamName0.String != NULL) {
				m_boolParamId0 = fwMvBooleanId(scrDecodeString(pScriptInitParams->boolParamName0.String));
			}
			else {
				m_boolParamId0 = BOOLEAN_ID_INVALID;
			}

			if (pScriptInitParams->boolParamName1.String != NULL) {
				m_boolParamId1 = fwMvBooleanId(scrDecodeString(pScriptInitParams->boolParamName1.String));
			}
			else {
				m_boolParamId1 = BOOLEAN_ID_INVALID;
			}

			m_boolParamValue0 = pScriptInitParams->boolParamValue0.Int ? true : false;
			m_boolParamValue1 = pScriptInitParams->boolParamValue1.Int ? true : false;
		}

		fwMvClipSetId		m_clipSet0;
		fwMvClipSetVarId	m_varClipSet0;
		fwMvClipSetId		m_clipSet1;
		fwMvClipSetVarId	m_varClipSet1;

		fwMvFloatId			m_floatParamId0;
		float				m_floatParamValue0;
		float				m_floatParamLerpValue0;
		fwMvFloatId			m_floatParamId1;
		float				m_floatParamValue1;
		float				m_floatParamLerpValue1;

		fwMvBooleanId		m_boolParamId0;
		bool				m_boolParamValue0;
		fwMvBooleanId		m_boolParamId1;
		bool				m_boolParamValue1;
	};

	struct LerpFloat
	{
		LerpFloat() : fLerpRate(DEFAULT_FLOAT_SIGNAL_LERP_RATE)
		{
		}

		float fTarget;
		float fCurrent;
		float fLerpRate;
	};

	typedef atMap< u32, LerpFloat>	FloatSignalMap;
	typedef atMap< u32, bool>	BoolSignalMap;

	enum
	{
		State_Start,
		State_Transitioning,
		State_InState,
		State_Exit
	};

	CTaskMoVEScripted(const fwMvNetworkDefId &networkDefId, u32 iFlags, float fBlendInDuration = INSTANT_BLEND_DURATION, const Vector3 &vInitialPosition = VEC3_ZERO, const Quaternion &initialOrientation = Quaternion::sm_I, ScriptInitialParameters* pInitParams = NULL);
	~CTaskMoVEScripted();

	// Request/Reset state transitions
	void RequestStateTransition(const char* szState);
	void ResetStateTransitionRequest();
	void SetExpectedCloneNextStateTransition(const char* szState);

	virtual aiTask* Copy() const { 

		CTaskMoVEScripted* pTask = rage_new CTaskMoVEScripted(m_networkDefId, m_iFlags, m_fBlendInDuration,  m_vInitialPosition, m_InitialOrientation); 

		pTask->CopyInitialParameters(m_pInitialParameters);
		pTask->SetCloneSyncDictHash(m_CloneSyncDictHash);

		return pTask;
	}

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_MOVE_SCRIPTED; }
	virtual s32				GetDefaultStateAfterAbort()	const {return State_Exit;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	virtual const char *GetStateName( s32 ) const;
	static const char * GetStaticStateName( s32  );
#endif

	// Clone task implementation for CTaskMoVEScripted
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual bool                IsInScope(const CPed* pPed);

	void UpdateCloneFloatSignals();

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return ProcessPostFSM();
	FSM_Return		UpdateFSM			(const s32 iState, const FSM_Event iEvent);	
	void			CleanUp			();

#if FPS_MODE_SUPPORTED
	virtual void	ProcessPreRender2();
	void			ProcessFirstPersonIk();
#endif // FPS_MODE_SUPPORTED

	// Returns the current script state name
	const char* GetScriptStateName() { return m_szState; }

	//Clone Dictionary set syncing
	void SetCloneSyncDictHash(u32 dictHash) { m_CloneSyncDictHash = dictHash; }  //this is needed for tasks used in MP games that sync clones e.g. Arm_wrestling
	// Sends a float signal
	void SetSignalFloat(const char* szSignal, float fSignal, bool bSend=true);
	void SetSignalFloat(const u32 signalHash, float fSignal, bool bSend=true);
	void SetCloneSignalFloat(const u32 signalHash, float fSignal, float fLerpRate=DEFAULT_FLOAT_SIGNAL_LERP_RATE);
	void SetSignalFloatLerpRate(const char* szSignal, float fLerpRate );
	void SetSignalFloatLerpRate(const u32 signalHash, float fLerpRate );
	// Sends a bool signal
	void SetSignalBool(const char* szSignal, bool bSignal, bool bSend=true);
	void SetSignalBool(const u32 signalHash, bool bSignal, bool bSend=true);

	//Helpers for accessing move network when syncing net players secondary animation.
	//currently only allows syncing it with concurrent two float values e.g. pitch/heading in pointing
	bool GetCloneMappedFloatSignal(u32 &rsignalHash, float &rfValue, u32 findIndex);
	bool GetCloneMappedBoolSignal(u32 uFindHash, bool &rbValue);
	u32 GetCloneSyncDictHash() { return m_CloneSyncDictHash; } 
	u32 GetStatePartialHash() { return m_StatePartialHash; }
	u32 GetCloneNetworkDefHash() {return m_networkDefId.GetHash(); }
	bool GetIsSlowBlend() {return (m_fBlendInDuration == REALLY_SLOW_BLEND_DURATION); }

	// Get signals from the MoVE network
	float GetSignalFloat(const char* szSignal);
	bool GetSignalBool(const char* szSignal);

	// Returns true if the network is active
	bool GetIsNetworkActive() { return m_moveNetworkHelper.IsNetworkActive(); }

	//	Returns true if an event with the given name has just been fired by the MoVE network
	bool GetMoveEvent(const char * eventName);

	enum 
	{
		Flag_SetInitialPosition						= BIT0,
		Flag_AllowOverrideCloneUpdate				= BIT1,
		Flag_UseKinematicPhysics					= BIT2,
		Flag_Secondary								= BIT3,
		Flag_UseFirstPersonArmIkLeft				= BIT4,
		Flag_UseFirstPersonArmIkRight				= BIT5,
		Flag_EnableCollisionOnNetworkCloneWhenFixed = BIT6,
		Flag_Max = Flag_EnableCollisionOnNetworkCloneWhenFixed
	};

	bool GetAllowOverrideCloneUpdate() { return (m_iFlags & Flag_AllowOverrideCloneUpdate)!=0; }

	void SetClipSet(fwMvClipSetVarId varId, fwMvClipSetId setId)
	{
		if (m_moveNetworkHelper.IsNetworkActive())
		{
			m_moveNetworkHelper.SetClipSet(setId, varId);

			animTaskDebugf(this, "SetClipSet setId:%u %s varId:%u %s",
				setId.GetHash(), setId.GetCStr(),
				varId.GetHash(), varId.GetCStr());
		}
		else
		{
			// cache the details so we can apply them when the task starts
			// unfortunately we have to do this so that script can override the clip sets before the network begins
			CScriptedClipSetRequest& req = m_clipSetsRequested.Grow();
			req.setId = setId;
			req.varId = varId;
		}
	}

#if __BANK
#define		MAX_TRANSISTION_NAME_LEN	 64
#define		MAX_SIGNAL_NAME_LEN	 64
	static char		ms_NameOfTransistion[MAX_TRANSISTION_NAME_LEN];
	static char		ms_NameOfSignal[MAX_SIGNAL_NAME_LEN];

	static bool		ms_bankOverrideCloneUpdate;

	static bool		ms_bankBoolSignal;
	static float	ms_bankFloatSignal;
	static float	ms_bankFloatLerpRate;

	static void InitWidgets();
	static CTaskMoVEScripted* GetActiveMoVENetworkTaskOnFocusPed(bool bOnlyGetLocal = false);
	static void SetStartStateTransitionRequest();
	static void SendBoolSignal();
	static void SendFloatSignal();
	static void SetFloatLerpRate();
	static void SetAllowOverrideCloneUpdate();

	void SetAllowOverrideCloneUpdateFlag()		{m_iFlags |= Flag_AllowOverrideCloneUpdate;}
	void ClearAllowOverrideCloneUpdateFlag()	{m_iFlags &= ~Flag_AllowOverrideCloneUpdate;}
	
#endif

	void SetEnableCollisionOnNetworkCloneWhenFixed(bool bEnableCollisionOnNetworkCloneWhenFixed);
	bool GetEnableCollisionOnNetworkCloneWhenFixed() const { return (m_iFlags & Flag_EnableCollisionOnNetworkCloneWhenFixed) != 0; }

private:
	// State Functions
	// Start
	void						Start_OnEnter();
	FSM_Return					Start_OnUpdate();
	// Transitioning
	void						Transitioning_OnEnter();
	FSM_Return					Transitioning_OnUpdate();
	void						Transitioning_OnExit();
	// InState
	void						InState_OnEnter();
	FSM_Return					InState_OnUpdate();

	void SetInitParams(ScriptInitialParameters* pInitParams);
	void CopyInitialParameters(InitialParameters* pInitParams);

	// Move network helper, contains the interface to the move network
	CMoveNetworkHelper m_moveNetworkHelper;

	fwMvNetworkDefId m_networkDefId;

	// Requested state transitions
	enum { STATE_NAME_LEN = 32 };
	char				m_szPreviousState[STATE_NAME_LEN];
	char				m_szState[STATE_NAME_LEN];
	char				m_szStateRequested[STATE_NAME_LEN];
	bool				m_bStateRequested : 1;

	//For clone
	u32					m_StatePartialHash;
	u32					m_ExpectedCloneNextStatePartialHash;
	u32					m_CloneSyncDictHash;

	static const int	MAX_NUM_FLOAT_SIGNALS_PER_UPDATE = 3;
	static const int	MAX_NUM_BOOL_SIGNALS_PER_UPDATE = 2;

	FloatSignalMap		m_mapSignalFloatValues;
	mutable BoolSignalMap		m_mapSignalBoolValues;

	u32					m_iFlags;
	Vector3				m_vInitialPosition;
	Quaternion			m_InitialOrientation;
	float				m_fBlendInDuration;

	static const fwMvBooleanId	ms_ExitId;
	static const fwMvBooleanId	ms_IntroFinishedId;
	static const fwMvClipId		ms_IntroOffsetClipId;
	static const fwMvFloatId	ms_IntroOffsetPhaseId;

	fwMvClipSetId		m_initClipSet0;
	fwMvClipSetVarId	m_initVarClipSet0;
	
	struct CScriptedClipSetRequest
	{
		fwMvClipSetVarId varId;
		fwMvClipSetId setId;
	};

	atArray< CScriptedClipSetRequest > m_clipSetsRequested;	// Need to make a list of these and set them onto the network player as soon as it is created.

	InitialParameters* m_pInitialParameters; // Any initial clipsets, float and bools we need to send immediately to the network player as soon as it is created.

#if FPS_MODE_SUPPORTED
	CFirstPersonIkHelper* m_apFirstPersonIkHelper[2];
#endif // FPS_MODE_SUPPORTED
};



//-------------------------------------------------------------------------
// Task info for TaskMoVEScripted
//-------------------------------------------------------------------------
class CClonedTaskMoVEScriptedInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedTaskMoVEScriptedInfo();
	CClonedTaskMoVEScriptedInfo(s32					state,
								u32					networkDefHash,
								u32					statePartialHash,
								u32					stateNextPartialHash,
								u8					flags,
								float				fBlendInDuration,
								u32					networkSyncDictHash,
								const CTaskMoVEScripted::FloatSignalMap	&mapSignalFloatValues,
								const CTaskMoVEScripted::BoolSignalMap	&mapSignalBoolValues,
								const Vector3&		vInitialPosition,
								const Quaternion&	initialOrientation,
								u32					initClipIdHash,
								u32					initVarClipIdHash);

	~CClonedTaskMoVEScriptedInfo(){}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_MOVE_SCRIPTED;}

	u32		GetFlags() const			{	return m_Flags;				}

	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskMoVEScripted::State_Exit>::COUNT;}
	virtual const char*	GetStatusName(u32) const { return "Move Scripted State";}

	u32			GetExpectedNextStatePartialHash()	{return m_ExpectedNextStatePartialHash;}
	u32			GetStatePartialHash()	{return m_StatePartialHash;}
	u32			GetNetworkDefHash()		{return m_NetworkDefHash;}
	u32			GetFlags()				{return m_Flags;}
	u32			GetNetworkSyncDictHash()		{return  m_SyncDictHash;}

	u32			GetNumFloatValues()		{return  m_NumFloatValues;}
	u32			GetNumBoolValues()		{return  m_NumBoolValues;}

	u32			GetFloatSignalHash(unsigned i)			{Assertf(i<CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE, "Float Index %d out of range", i); return m_FloatSignalHash[i];}
	float		GetFloatSignalValue(unsigned i)			{Assertf(i<CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE, "Float Index %d out of range", i);return m_FloatSignalValues[i];}
	float		GetFloatSignalLerpValue(unsigned i)		{Assertf(i<CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE, "Float Index %d out of range", i);return m_FloatSignalLerpValues[i];}
	bool		GetFloatHasLerpValue(unsigned i);

	u32			GetBoolSignalHash(unsigned i)			{Assertf(i<CTaskMoVEScripted::MAX_NUM_BOOL_SIGNALS_PER_UPDATE, "Bool Index %d out of range", i); return m_BoolSignalHash[i];}
	bool		GetBoolSignalValue(unsigned i);

	Quaternion	GetInitialOrientation() {return m_InitialOrientation;}
	Vector3		GetInitialPosition()	{return m_vInitialPosition;}

	void Serialise(CSyncDataBase& serialiser)
	{
		static const float MAX_VALUE_BLEND_RATIO = 1.0f;
		static const int   NUM_BITS_BLEND_RATIO = 8;

		CSerialisedFSMTaskInfo::Serialise(serialiser);
		SERIALISE_UNSIGNED(serialiser, m_Flags,					SIZEOF_FLAGS,							"Move Scripted Flags");
		SERIALISE_HASH(serialiser, m_NetworkDefHash,			"network Def Hash");

		Assertf(m_NumFloatValues<=CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE,"Too many float signals expect %d got %d",CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE,m_NumFloatValues);
		Assertf(m_NumBoolValues<=CTaskMoVEScripted::MAX_NUM_BOOL_SIGNALS_PER_UPDATE,"Too many bool signals expect %d got %d",CTaskMoVEScripted::MAX_NUM_BOOL_SIGNALS_PER_UPDATE,m_NumBoolValues);

		SERIALISE_PACKED_FLOAT(serialiser, m_fBlendInDuration, MAX_VALUE_BLEND_RATIO, NUM_BITS_BLEND_RATIO, "Blend Duration")

		bool bDictionarySynced = m_SyncDictHash !=0;

		SERIALISE_BOOL(serialiser, bDictionarySynced, "Is Dictionary Synced");

		if( bDictionarySynced || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_HASH(serialiser, m_SyncDictHash,		"Dictionary Hash");
		}
		else
		{
			m_SyncDictHash=0;
		}

		if( !( m_Flags & CTaskMoVEScripted::Flag_AllowOverrideCloneUpdate) || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_NumFloatValues,	datBitsNeeded<CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE>::COUNT,	"Num float signals");
			SERIALISE_UNSIGNED(serialiser, m_NumBoolValues,	datBitsNeeded<CTaskMoVEScripted::MAX_NUM_BOOL_SIGNALS_PER_UPDATE>::COUNT,	"Num bool signals");

			u32 numFloatValues = m_NumFloatValues;
			u32 numBoolValues  = m_NumBoolValues;

			if (serialiser.GetIsMaximumSizeSerialiser())
			{
				numFloatValues = CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE;
				numBoolValues = CTaskMoVEScripted::MAX_NUM_BOOL_SIGNALS_PER_UPDATE;
			}

			for( unsigned i=0; i<numFloatValues; i++)
			{
				SERIALISE_HASH(serialiser, m_FloatSignalHash[i],		"Float signal hash");
				SERIALISE_PACKED_FLOAT(serialiser, m_FloatSignalValues[i],		1.0f, SIZEOF_SIGNAL_FLOAT,	"Float signal value");

				bool bFloatHasLerpValue = m_FloatSignalLerpValues[i] != CTaskMoVEScripted::DEFAULT_FLOAT_SIGNAL_LERP_RATE;				
				SERIALISE_BOOL(serialiser, bFloatHasLerpValue, "Float has lerp value");
				SetFloatHasLerpValue(i,bFloatHasLerpValue);

				if(bFloatHasLerpValue || serialiser.GetIsMaximumSizeSerialiser())
				{
					SERIALISE_PACKED_FLOAT(serialiser, m_FloatSignalLerpValues[i],	1.0f, SIZEOF_FLOAT_LERP,	"Float signal Lerp value");
				}
				else
				{
					m_FloatSignalLerpValues[i] = CTaskMoVEScripted::DEFAULT_FLOAT_SIGNAL_LERP_RATE;
				}
			}

			for( unsigned i=0; i<numBoolValues; i++)
			{
				SERIALISE_HASH(serialiser, m_BoolSignalHash[i],		"Bool signal hash");
				bool bBoolSignalValue = GetBoolSignalValue(i);
				SERIALISE_BOOL(serialiser, bBoolSignalValue, "Bool signal value");
				SetBoolSignalValue(i,bBoolSignalValue);
			}

			SERIALISE_PARTIALHASH(serialiser, m_StatePartialHash,			"State Partial Hash");

			bool bHasExpectedNextStatePartialHash = m_ExpectedNextStatePartialHash !=0;

			SERIALISE_BOOL(serialiser, bHasExpectedNextStatePartialHash, "Has Expected Next State Partial Hash");

			if( bHasExpectedNextStatePartialHash || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_PARTIALHASH(serialiser, m_ExpectedNextStatePartialHash,		"Expected Next State Partial Hash");
			}
			else
			{
				m_ExpectedNextStatePartialHash=0;
			}
		}
		
		if( (m_Flags & CTaskMoVEScripted::Flag_SetInitialPosition ) || serialiser.GetIsMaximumSizeSerialiser()  )
		{
			SERIALISE_QUATERNION(serialiser, m_InitialOrientation,	SIZEOF_INITIAL_ORIENTATION_QUAT,	"Initial Orientation");
			SERIALISE_VECTOR(serialiser, m_vInitialPosition,	WORLDLIMITS_XMAX,	SIZEOF_INITIAL_POSITION,	"Initial Position");
		}

		bool hasClipset0 = m_clipSet0Hash != 0;
		SERIALISE_BOOL(serialiser, hasClipset0, "Has Init Clipset 0");
		if(hasClipset0 || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_HASH(serialiser, m_clipSet0Hash, "Clipset 0 Hash");
		}
		else
		{
			m_clipSet0Hash = 0;
		}

		bool hasVarClipset0 = m_varClipSet0Hash != 0;
		SERIALISE_BOOL(serialiser, hasVarClipset0, "Has Init Var Clipset 0");
		if(hasVarClipset0 || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_HASH(serialiser, m_varClipSet0Hash, "Var Clipset 0 Hash");
		}
		else
		{
			m_varClipSet0Hash = 0;
		}
	}

private:

	void	SetBoolSignalValue(unsigned index, bool bValue );
	void	SetFloatHasLerpValue(unsigned index, bool bValue );

    CClonedTaskMoVEScriptedInfo(const CClonedTaskMoVEScriptedInfo &);

	static const unsigned int SIZEOF_FLAGS			    = datBitsNeeded<CTaskMoVEScripted::Flag_Max>::COUNT;  //Max needed flag;   
	static const unsigned int SIZEOF_SIGNAL_FLOAT		= 8;
	static const unsigned int SIZEOF_FLOAT_LERP			= 8;
	static const unsigned int SIZEOF_INITIAL_ORIENTATION_QUAT	= 19;
	static const unsigned int SIZEOF_INITIAL_POSITION			= 32;

	u32			m_clipSet0Hash;
	u32			m_varClipSet0Hash;

	u32			m_StatePartialHash;
	u32			m_ExpectedNextStatePartialHash;
	u32			m_NetworkDefHash;
	u32			m_SyncDictHash;

	u32		m_FloatSignalHash[CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE];
	float	m_FloatSignalValues[CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE];
	float	m_FloatSignalLerpValues[CTaskMoVEScripted::MAX_NUM_FLOAT_SIGNALS_PER_UPDATE];

	u32		m_BoolSignalHash[CTaskMoVEScripted::MAX_NUM_BOOL_SIGNALS_PER_UPDATE];

	u8		m_Flags;
	u8		m_NumFloatValues;
	u8		m_NumBoolValues;
	bool    m_BoolSignalValue1 :1;  //MAX_NUM_BOOL_SIGNALS_PER_UPDATE = 2 currently so only two used
	bool    m_BoolSignalValue2 :1;

	bool    m_FloatHasLerpValue1 :1;  //MAX_NUM_FLOAT_SIGNALS_PER_UPDATE = 2 currently so only two used
	bool    m_FloatHasLerpValue2 :1;

	Quaternion	m_InitialOrientation;
	Vector3		m_vInitialPosition;
	float		m_fBlendInDuration;
};

#endif // TASK_MOVE_SCRIPTED_H
