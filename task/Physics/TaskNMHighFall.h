// Filename   :	TaskNMHighFall.h
// Description:	Natural Motion high fall class (FSM version)

#ifndef INC_TASKNMHIGHFALL_H_
#define INC_TASKNMHIGHFALL_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMBrace.h"
#include "Task/Physics/GrabHelper.h"

// ------------------------------------------------------------------------------

// high fall
// triggers:	fall from a height to try and land on feet
//
class CTaskNMHighFall : public CTaskNMBehaviour
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		s32 m_HighFallTimeToBlockInjuredOnGround;

		CTaskNMBehaviour::Tunables::TunableForce m_PitchInDirectionForce;
		s32	m_PitchInDirectionComponent; 

		CTaskNMBehaviour::Tunables::TunableForce m_StuntJumpPitchInDirectionForce;
		s32	m_StuntJumpPitchInDirectionComponent; 

		CNmTuningSet m_Start;
		CNmTuningSet m_Update;

		// contextual modifiers for the start message
		CNmTuningSet m_InAir;
		CNmTuningSet m_Vault;
		CNmTuningSet m_FromCarHit;
		CNmTuningSet m_SlopeSlide;
		CNmTuningSet m_TeeterEdge;
		CNmTuningSet m_StuntJump;

		CNmTuningSet m_SprintExhausted;
		CNmTuningSet m_OnBalanceFailedSprintExhausted;
		bool m_DisableStartMessageForSprintExhausted;

		CNmTuningSet m_JumpCollision;

		CNmTuningSet m_HighHighFallStart;
		CNmTuningSet m_HighHighFallEnd;
		CNmTuningSet m_SuperHighFallStart;

		CTaskNMBehaviour::Tunables::StandardBlendOutThresholds m_BlendOut;

		CTaskNMBehaviour::Tunables::BlendOutThreshold m_PlayerQuickBlendOut;
		CTaskNMBehaviour::Tunables::BlendOutThreshold m_MpPlayerQuickBlendOut;

		float m_MaxHealthLossForQuickGetup;
		float m_MinHealthForQuickGetup;
		float m_MpMaxHealthLossForQuickGetup;
		float m_MpMinHealthForQuickGetup;

		bool m_UseRemainingMinTimeForGroundWrithe;
		u32 m_MinTimeRemainingForGroundWrithe;
		u32 m_MinTimeElapsedForGroundWrithe;

		float m_DistanceZThresholdForHighHighFall;
		float m_VelocityZThresholdForHighHighFall;
		float m_VelocityZThresholdForSuperHighFall;
		float m_RagdollComponentAirResistanceMinStiffness[RAGDOLL_NUM_JOINTS];
		float m_RagdollComponentAirResistanceForce[RAGDOLL_NUM_COMPONENTS];

		u8 m_AirResistanceOption;

#if __BANK
		void PreAddWidgets(bkBank& bank);
#endif

		PAR_PARSABLE;
	};

	enum eHighFallEntryType { // keep in sync with commands_task.sch - HIGH_FALL_ENTRY_TYPE
		HIGHFALL_IN_AIR,
		HIGHFALL_VAULT,
		HIGHFALL_FROM_CAR_HIT,
		HIGHFALL_SLOPE_SLIDE,
		HIGHFALL_TEETER_EDGE,
		HIGHFALL_SPRINT_EXHAUSTED,
		HIGHFALL_STUNT_JUMP,
		HIGHFALL_JUMP_COLLISION,
		NUM_HIGHFALL_TYPES
	};

#if !__FINAL
	static const char * GetEntryTypeName(eHighFallEntryType type)
	{
		static const char* s_EntryTypeNames[NUM_HIGHFALL_TYPES] = 
		{
			"HIGHFALL_IN_AIR",
			"HIGHFALL_VAULT",
			"HIGHFALL_FROM_CAR_HIT",
			"HIGHFALL_SLOPE_SLIDE",
			"HIGHFALL_TEETER_EDGE",
			"HIGHFALL_SPRINT_EXHAUSTED",
			"HIGHFALL_STUNT_JUMP",
			"HIGHFALL_JUMP_COLLISION",
		};

		if (type>=HIGHFALL_IN_AIR && type < NUM_HIGHFALL_TYPES)
			return s_EntryTypeNames[type];
		else
			return "UNKNOWN ENTRY TYPE!";
	}
#endif // !__FINAL

	CTaskNMHighFall(u32 nMinTime, const CGrabHelper* pGrabHelper=NULL, eHighFallEntryType eType = HIGHFALL_IN_AIR, const Vector3 *pitchDirOverride = NULL, bool blockEarlyGetup = false);
	~CTaskNMHighFall();
protected:
	CTaskNMHighFall(const CTaskNMHighFall& otherTask);
public:

	void SetPitchDirectionOverride(const Vector3 &vel) { m_PitchDirOverride = vel; } 
	void SetEntryType(eHighFallEntryType eType) { m_eType = eType; }
	eHighFallEntryType GetEntryType() const { return m_eType; }

	bool HasBalanceFailed() const { return m_bBalanceFailed; }

	virtual void CleanUp();

	virtual bool	IsInScope(const CPed* pPed);
	virtual bool	HandlesDeadPed(){ return true; }
	virtual bool	ShouldContinueAfterDeath() const {return true;}
	bool			HandleRagdollImpact(float fMag, const CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId);
	virtual bool	ShouldAbortForWeaponDamage(CEntity* UNUSED_PARAM(pFiringEntity), const CWeaponInfo* pWeaponInfo, const f32 UNUSED_PARAM(fWeaponDamage), const fwFlags32& UNUSED_PARAM(flags), 
		const bool UNUSED_PARAM(bWasKilledOrInjured), const Vector3& UNUSED_PARAM(vStart), WorldProbe::CShapeTestHitPoint* UNUSED_PARAM(pResult), 
		const Vector3& UNUSED_PARAM(vRagdollImpulseDir), const f32 UNUSED_PARAM(fRagdollImpulseMag)) 

	{ 
		// Don't abort high fall for unknown damage, even when dead (because we're going to continue running after death)
		if (pWeaponInfo->GetDamageType() == DAMAGE_TYPE_UNKNOWN)
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return m_strTaskAndStateName;}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMHighFall(*this);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_HIGH_FALL;}

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	void SetMaxTime(u16 maxTime) { m_nMaxTime = maxTime; }

	///////////////////////////
	// CTaskNMBehaviour functions
	virtual void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourSuccess(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourFinish(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);
	virtual void BehaviourEvent(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);

	virtual bool ProcessPhysics(float fTimeStep, int nTimeSlice);

	bool GetIsNetworkBlenderOffForHighFall() {return m_bNetworkBlenderOffForHighFall;}

protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	virtual void StartAnimatedFallback(CPed* pPed);
	virtual bool ControlAnimatedFallback(CPed* pPed);

	virtual void AddStartMessages(CPed* pPed, CNmMessageList& list);

	//////////////////////////
	// Local functions and data
	CGrabHelper& GetGrabHelper() {return m_GrabHelper;}

private:

	bool m_bDoingHighHighFall : 1;
	bool m_bDoingSuperHighFall : 1;
	bool m_bNetworkBlenderOffForHighFall : 1; //latched true if the task sees a fall greater than the m_bDoingHighHighFall test height to prevent clone warp corrections while falling from great heights
	bool m_bBalanceFailed : 1;
	bool m_foundgrab : 1;
	bool m_SideStabilizationApplied : 1;
	bool m_BlockEarlyGetup : 1;
	bool m_bUsePitchInDirectionForce : 1;

	float m_StartingHealth;

	Vector3 m_PitchDirOverride;

	CGrabHelper m_GrabHelper;
	eHighFallEntryType m_eType;

#if !__FINAL
	// For debugging the internal state machine.
	atString m_strTaskAndStateName;
#endif // !__FINAL

	// tunable by widgets
public:

	static const char* ms_TypeToString[NUM_HIGHFALL_TYPES];
	
	static Tunables	sm_Tunables;
};

//
// Task info for CTaskNMHighFall
//
class CClonedNMHighFallInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMHighFallInfo(u32 highFallType);
	CClonedNMHighFallInfo();
	~CClonedNMHighFallInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_HIGHFALL;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_UNSIGNED(serialiser, m_highFallType, SIZEOF_HIGHFALL_TYPE, "High fall type");
	}

private:

	CClonedNMHighFallInfo(const CClonedNMHighFallInfo &);
	CClonedNMHighFallInfo &operator=(const CClonedNMHighFallInfo &);

	static const unsigned int SIZEOF_HIGHFALL_TYPE	= datBitsNeeded<CTaskNMHighFall::NUM_HIGHFALL_TYPES>::COUNT;

	u32	m_highFallType;
};

#endif // ! INC_TASKNMHIGHFALL_H_
