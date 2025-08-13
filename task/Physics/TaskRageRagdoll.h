// Filename   :	TaskRageRagdoll.h
// Description:	Task to handle non-natural motion ragdolls.

#ifndef INC_TASKRAGERAGDOLL_H_
#define INC_TASKRAGERAGDOLL_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Game headers
#include "peds/QueriableInterface.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskTypes.h"
#include "Task/System/Tuning.h"

// ------------------------------------------------------------------------------


//
// Task info for CTaskRageRagdoll
//
class CClonedRageRagdollInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedRageRagdollInfo() {}
	~CClonedRageRagdollInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_RAGE_RAGDOLL;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
	}

private:

	CClonedRageRagdollInfo(const CClonedRageRagdollInfo &);
	CClonedRageRagdollInfo &operator=(const CClonedRageRagdollInfo &);
};

// ------------------------------------------------------------------------------

class CTaskRageRagdoll : public CTaskFSMClone
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		struct RageRagdollImpulseTuning
		{
			// Values used to keep the impulses of rapid firing stable (first hit is normal, subsequent ones are reduced)
			float m_fImpulseReductionPerShot;
			float m_fImpulseRecoveryPerSecond;
			float m_fMaxImpulseModifier;
			float m_fMinImpulseModifier;
			float m_fCounterImpulseRatio;
			float m_fTempInitialStiffnessWhenShot;  
			float m_fAnimalMassMult;
			float m_fAnimalImpulseMultMin;  
			float m_fAnimalImpulseMultMax;
			float m_fInitialHitImpulseMult;    

			PAR_SIMPLE_PARSABLE;
		};

		RageRagdollImpulseTuning m_RageRagdollImpulseTuning;

		struct WritheStrengthTuning
		{
			float m_fStartStrength;
			float m_fMidStrength;
			float m_fEndStrength;

			float m_fInitialDelay;
			float m_fDurationStage1;
			float m_fDurationStage2;

			PAR_SIMPLE_PARSABLE;
		};

		WritheStrengthTuning m_SpineStrengthTuning;
		WritheStrengthTuning m_NeckStrengthTuning;
		WritheStrengthTuning m_LimbStrengthTuning;

		float m_fMuscleAngleStrengthRampDownRate;
		float m_fMuscleSpeedStrengthRampDownRate;

		PAR_PARSABLE;
	};

	static bank_float ms_fDefaultInterruptablePhase;

	enum
	{
		State_Ragdoll = 0,
		State_AnimatedFallback,
		State_Finish
	};

	// Constructors and Destructor.
	CTaskRageRagdoll(bool bRunningPrototype = false, CEntity *pHitEntity = NULL);

	// Helper function for the constructors.
	void Init();

	CEntity * GetHitEntity() const { return m_pHitEntity; }
	void SetHitEntityImpactNormal(CEntity *entity, Vec3V_In vNormal) { m_pHitEntity = entity; m_vHitEntityImpactNormal = vNormal; m_ImpactedHitEntity = true; }

	virtual aiTask* Copy() const
	{
		return rage_new CTaskRageRagdoll(m_bRunningPrototype, m_pHitEntity);
	}

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const
	{
		return rage_new CClonedRageRagdollInfo();
	}
	virtual CTaskFSMClone* CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone* CreateTaskForLocalPed(CPed* pPed);

	virtual bool HandlesRagdoll(const CPed*) const { return true; }

	static float GetImpulseReductionPerShot() { return sm_Tunables.m_RageRagdollImpulseTuning.m_fImpulseReductionPerShot; } 
	static float GetImpulseRecoveryPerSecond() { return sm_Tunables.m_RageRagdollImpulseTuning.m_fImpulseRecoveryPerSecond; } 
	static float GetMaxImpulseModifier() { return sm_Tunables.m_RageRagdollImpulseTuning.m_fMaxImpulseModifier; } 
	static float GetMinImpulseModifier() { return sm_Tunables.m_RageRagdollImpulseTuning.m_fMinImpulseModifier; } 			
	
	static float GetCounterImpulseRatio() { return sm_Tunables.m_RageRagdollImpulseTuning.m_fCounterImpulseRatio; } 
	static float GetTempInitialStiffnessWhenShot() { return sm_Tunables.m_RageRagdollImpulseTuning.m_fTempInitialStiffnessWhenShot; } 
	static float GetAnimalMassMult() { return sm_Tunables.m_RageRagdollImpulseTuning.m_fAnimalMassMult; } 
	static float GetAnimalImpulseMultMin() { return sm_Tunables.m_RageRagdollImpulseTuning.m_fAnimalImpulseMultMin; } 	
	static float GetAnimalImpulseMultMax() { return sm_Tunables.m_RageRagdollImpulseTuning.m_fAnimalImpulseMultMax; } 
	static float GetInitialHitImpulseMult() { return sm_Tunables.m_RageRagdollImpulseTuning.m_fInitialHitImpulseMult; }

	static float GetDesiredStiffness(const CPed* pPed);

	////////////////////////////
	// CTask functions:
protected:
	FSM_Return ProcessPreFSM();
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual bool ControlPassingAllowed(CPed* pPed, const netPlayer& player, eMigrationType migrationType);
	s32 GetDefaultStateAfterAbort() const {return State_Finish;}

	// Determine whether or not the task should abort.
	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);
	// Handle aborting the task - this will be called in addition to CleanUp when we are aborting.
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		Assert(iState>=State_Ragdoll&&iState<=State_Finish);
		static const char* aStateNames[] = 
		{
			"State_Ragdoll",
			"State_AnimatedFallback",
			"State_Finish"
		};

		return aStateNames[iState];
	}
#endif // !__FINAL

private:
	//////////////////////////////////////////////
	// Helper functions for FSM state management:
	void Ragdoll_OnEnter(CPed* pPed);
	FSM_Return Ragdoll_OnUpdate(CPed* pPed);

	void AnimatedFallback_OnEnter(CPed* pPed);
	FSM_Return AnimatedFallback_OnUpdate(CPed* pPed);

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_RAGE_RAGDOLL;}

	void CorrectAwkwardPoses(CPed* pPed);
	void CorrectUnstablePoses(CPed* pPed);

	void ProcessJointRelaxation(CPed* pPed);
	void ProcessWrithe(CPed* pPed);
	void ProcessHitByVehicle(CPed* pPed);

	RegdEnt m_pHitEntity;

	Vec3V m_vHitEntityImpactNormal;

	int m_driveTimer;
	float m_initialStiffness;

	fwMvClipId m_suggestedClipId;
	fwMvClipSetId m_suggestedClipSetId;
	float m_suggestedClipPhase;

	bool m_bRunningPrototype : 1;
	bool m_StartedCorrectingPoses : 1;
	bool m_WritheFinished : 1;
	bool m_ImpactedHitEntity : 1;
	bool m_UpwardCarImpactApplied : 1;

	static Tunables	sm_Tunables;
};

#endif // ! INC_TASKRAGERAGDOLL_H_
