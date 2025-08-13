#ifndef INC_TASK_REACT_H
#define INC_TASK_REACT_H

// Rage headers
#include "fwutil/Flags.h"

// Game headers
#include "AI/AITarget.h"
#include "Scene/RegdRefTypes.h"
#include "Task/System/Task.h"
#include "Task/System/Tuning.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/Weapons/WeaponTarget.h"
#include "Peds/QueriableInterface.h"
#include "network/general/NetworkUtil.h"

//////////////////////////////////////////////////////////////////////////
// CTaskReactAimWeapon
//////////////////////////////////////////////////////////////////////////

class CTaskReactAimWeapon : public CTaskFSMClone
{
public:

	enum
	{
		State_Start = 0,
		State_StreamAnims,
		State_PlayReaction,
		State_Finish
	};
	
	enum ConfigFlags
	{
		CF_Flinch		= BIT0,
		CF_Surprised	= BIT1,
		CF_Sniper		= BIT2,
	};
	
public:

	struct Tunables : CTuning
	{
		struct Ability
		{
			struct Situation
			{
				struct Weapon
				{
					Weapon()
					{}

					atHashWithStringNotFinal	m_ClipSetId;
					float						m_Rate;
					bool						m_HasSixDirections;
					bool						m_HasCreateWeaponTag;
					bool						m_HasInterruptTag;

					PAR_SIMPLE_PARSABLE;
				};

				Situation()
				{}

				Weapon	m_Pistol;
				Weapon	m_Rifle;
				Weapon	m_MicroSMG;

				PAR_SIMPLE_PARSABLE;
			};

			Ability()
			{}

			Situation	m_Flinch;
			Situation	m_Surprised;
			Situation	m_Sniper;
			Situation	m_None;

			PAR_SIMPLE_PARSABLE;
		};
		
		Tunables();
		
		Ability	m_Professional;
		Ability	m_NotProfessional;

		float	m_Rate;
		float	m_MaxRateVariance;
		
		PAR_PARSABLE;
	};
	
public:

	CTaskReactAimWeapon(const CPed* pTarget, fwFlags8 uConfigFlags = 0);
	CTaskReactAimWeapon(const CWeaponTarget& target, fwFlags8 uConfigFlags = 0);
	~CTaskReactAimWeapon();

	// Task required functions
	virtual aiTask*				Copy() const		{ return rage_new CTaskReactAimWeapon(m_target, m_uConfigFlags); }
	virtual int					GetTaskTypeInternal() const { return CTaskTypes::TASK_REACT_AIM_WEAPON; }

	// FSM required functions
	virtual FSM_Return			UpdateFSM	(const s32 iState, const FSM_Event iEvent);
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Finish; }

	// FSM optional functions
	virtual FSM_Return			ProcessPreFSM	();
	virtual void				CleanUp			();
	virtual bool				ProcessMoveSignals();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

#if DEBUG_DRAW
	void DebugRenderClonedInfo(void);
#endif /* DEBUG_DRAW */

    // Clone task implementation
	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);
	virtual bool                ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);

	virtual bool				IsInScope(const CPed* pPed);

private:

	// FSM State functions

	// Decide the reaction clip to play
	void						Start_OnEnter			();
	FSM_Return					Start_OnUpdate			();

	// Stream the clips in
	FSM_Return					StreamAnims_OnUpdate	();

	// Play the reaction clip
	FSM_Return					PlayReaction_OnEnter	();
	FSM_Return					PlayReaction_OnUpdate	();
	bool						PlayReaction_OnProcessMoveSignals();

	float				CalculateDirection();
	void				CreateWeapon();
	const CWeaponInfo*	GetEquippedGunInfo(bool& bIs2Handed) const;

	// Our move network helper and clip request helper
	CMoveNetworkHelper		m_moveNetworkHelper;
	fwClipSetRequestHelper	m_clipRequestHelper;
	fwClipSetRequestHelper	m_weaponClipRequestHelper;

	// current heading
	float					m_fReactBlendCurrentHeading;
	float					m_fReactBlendReferenceHeading;		

	float					m_fCurrentPitch;

	// The target given to use by the threat response
	CWeaponTarget			m_target;

	// We need to keep track of the clip set we want to load between the start state and play reaction states
	fwMvClipSetId			m_BaseClipSetId;
	fwMvClipSetId			m_WeaponClipSetId;
	float					m_fRate;
	
	fwFlags8	m_uConfigFlags;

	bool m_bHasSixDirections;
	bool m_bHasCreateWeaponTag;
	bool m_bHasInterruptTag;
	bool m_bMoveCreateWeapon;
	bool m_bMoveInterrupt;
	bool m_bMoveReactBlendFinished;
	bool m_bUsingOverrideClipSet;
	bool m_bNoLongerRunningOnOwner;

	// Move network variables
	static fwMvClipSetVarId	sm_AdditionalAnimsClipSet;
	static fwMvRequestId	ms_ReactBlendRequest;
	static fwMvRequestId	ms_AimRequest;
	static fwMvBooleanId	ms_OnEnterReactBlend;
	static fwMvBooleanId	ms_ReactBlendFinished;
	static fwMvFloatId		ms_ReactBlendDirection;
	static fwMvFloatId		ms_PitchId;
	static fwMvFloatId		ms_ReactRateId;
	static fwMvFlagId		ms_HasSixDirectionsId;
	
private:

	static Tunables sm_Tunables;

};

//-------------------------------------------------------------------------
// Task info
//-------------------------------------------------------------------------
class CClonedReactAimWeaponInfo : public CSerialisedFSMTaskInfo
{
public:

	struct InitParams
	{
		InitParams
		(
			s32				const	state,  
			fwMvClipSetId	const	clipSetId,
			fwMvClipSetId	const	weaponClipSetId,
			u8				const	flags,
			CWeaponTarget	const*	target,
			float			const	rate,
			bool			const   hasSixDirections,
			bool			const	hasCreateWeaponTag,
			bool			const	hasInterruptTag
		)
		:
			m_state(state),
			m_clipSetId(clipSetId),
			m_weaponClipSetId(weaponClipSetId),
			m_flags(flags),
			m_target(target),
			m_rate(rate),
			m_hasSixDirections(hasSixDirections),
			m_hasCreateWeaponTag(hasCreateWeaponTag),
			m_hasInterruptTag(hasInterruptTag)
		{}
		
		s32 					m_state;
		fwMvClipSetId 			m_clipSetId;
		fwMvClipSetId 			m_weaponClipSetId;
		u8						m_flags;	
		CWeaponTarget const*	m_target;
		float					m_rate;
		bool					m_hasSixDirections;
		bool					m_hasCreateWeaponTag;
		bool					m_hasInterruptTag;
	};

public:

    CClonedReactAimWeaponInfo(InitParams const& initParams)
	:
		m_clipSetId(initParams.m_clipSetId),
		m_weaponClipSetId(initParams.m_weaponClipSetId),
		m_flags(initParams.m_flags),
		m_rate(initParams.m_rate),
		m_hasSixDirections(initParams.m_hasSixDirections),
		m_hasCreateWeaponTag(initParams.m_hasCreateWeaponTag),
		m_hasInterruptTag(initParams.m_hasInterruptTag),
		m_weaponTargetHelper(initParams.m_target)
	{
		SetStatusFromMainTaskState(initParams.m_state);
	}

	CClonedReactAimWeaponInfo()
	:
		m_clipSetId(CLIP_SET_ID_INVALID),
		m_weaponClipSetId(CLIP_SET_ID_INVALID),
		m_flags(0),
		m_rate(0.0f),
		m_hasSixDirections(false),
		m_hasCreateWeaponTag(false),
		m_hasInterruptTag(false)
	{}

    ~CClonedReactAimWeaponInfo()
	{}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_REACT_AIM_WEAPON;}
	virtual CTaskFSMClone*	CreateCloneFSMTask();

    virtual bool	HasState() const			{ return true; }
	virtual u32		GetSizeOfState() const		{ return datBitsNeeded<CTaskReactAimWeapon::State_Finish>::COUNT; }
    virtual const char*	GetStatusName(u32) const	{ return "Status";}
		
	u8				GetFlags() const			{ return m_flags;			}
	fwMvClipSetId	GetClipSetId() const		{ return m_clipSetId;		}
	fwMvClipSetId	GetWeaponClipSetId() const	{ return m_weaponClipSetId;	}
	float			GetRate() const				{ return m_rate;			}
	bool			GetSixDirections() const	{ return m_hasSixDirections; }
	bool			GetCreateWeapon() const		{ return m_hasCreateWeaponTag; }
	bool			GetInterrupt() const		{ return m_hasInterruptTag; }

	inline CClonedTaskInfoWeaponTargetHelper const &	GetWeaponTargetHelper() const	{ return m_weaponTargetHelper; }
	inline CClonedTaskInfoWeaponTargetHelper&			GetWeaponTargetHelper()			{ return m_weaponTargetHelper; }

public:

	void Serialise(CSyncDataBase& serialiser)
	{
		static const u32 SIZE_OF_FLAGS	= 3;
		static const u32 SIZE_OF_RATE	= 8;

		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_ANIMATION_CLIP(serialiser, m_clipSetId,			m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall,	"Clip Set Id");
		SERIALISE_ANIMATION_CLIP(serialiser, m_weaponClipSetId,	m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall,	"Weapon Clip Set Id");
		
		SERIALISE_PACKED_FLOAT(serialiser, m_rate, 2.0f, SIZE_OF_RATE, "Rate");

		u8 flagstemp = m_flags;
		SERIALISE_UNSIGNED(serialiser, flagstemp, SIZE_OF_FLAGS, "Flags");
		m_flags = flagstemp;
		
		bool sixDirections = m_hasSixDirections;
		SERIALISE_BOOL(serialiser, sixDirections, "Six Directions Tag");
		m_hasSixDirections = sixDirections; 

		bool createWeaponTag = m_hasCreateWeaponTag;
		SERIALISE_BOOL(serialiser, createWeaponTag, "Create Weapon Tag");
		m_hasCreateWeaponTag = createWeaponTag;

		bool interruptTag = m_hasInterruptTag;
		SERIALISE_BOOL(serialiser, interruptTag, "Interrupt Tag");
		m_hasInterruptTag = interruptTag;

		m_weaponTargetHelper.Serialise(serialiser);
	}

private:

    CClonedReactAimWeaponInfo(const CClonedReactAimWeaponInfo &);
    CClonedReactAimWeaponInfo &operator=(const CClonedReactAimWeaponInfo &);

	CClonedTaskInfoWeaponTargetHelper m_weaponTargetHelper;
	fwMvClipSetId 	m_clipSetId;
	fwMvClipSetId 	m_weaponClipSetId;
	float			m_rate;
	u8				m_flags:2;
	bool			m_hasSixDirections:1;
	bool			m_hasCreateWeaponTag:1;
	bool			m_hasInterruptTag:1;

	fwMvClipId		m_ClipIDNotUsedButNeededForSerialiseAnimationFunctionCall;
};

#endif // INC_TASK_REACT_H
