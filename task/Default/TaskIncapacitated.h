#ifndef TASK_INCAPACITATED_H
#define TASK_INCAPACITATED_H

// Game headers
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskTypes.h"

#if CNC_MODE_ENABLED

class CTaskIncapacitated : public CTaskFSMClone
{

public:
	enum eTASK_INCAPACITATED_STATE
	{
		State_Start,
		State_DownedRagdoll,
		State_Incapacitated,
		State_Escape,
		State_Arrested,
		State_Finish,
		NUM_STATES
	};

public:
	struct Tunables : public CTuning
	{
		Tunables();

		u32 m_TimeSinceRecoveryToIgnoreEnduranceDamage;
		u32 m_IncapacitateTime;
		u32 m_GetUpFasterModifier;
		u32 m_GetUpFasterLimit;

		PAR_PARSABLE;
	};

	static void InitTunables();

	CTaskIncapacitated();
	virtual ~CTaskIncapacitated();

	aiTask* Copy() const override;
	int GetTaskTypeInternal() const override { return CTaskTypes::TASK_INCAPACITATED; }

	// Returns the default state that the task should restart in when resuming after being aborted (can be the current state)
	s32 GetDefaultStateAfterAbort() const override;

	// FSM implementation
	FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent) override;

	bool IsArrestable() const;
	bool IsArrestableByPed(CPed* pPed) const;

	bool GetIsArrested() const { return m_bArrested; }
	void SetIsArrested(bool bArrested) { m_bArrested = bArrested; }

	CPed* GetPedArrester() const { return m_pArrester; }
	void SetPedArrester(CPed* pPed) { m_pArrester = pPed; }

	u32 GetGetupTime() const { return m_iGetupTime; }
	void SetGetupTime(u32 iGetupTime) { m_iGetupTime = iGetupTime; }

	u32 GetFastGetupOffset() const { return m_iFastGetupOffset; }
	void SetFastGetupOffset(u32 iGetupTime) { m_iFastGetupOffset = iGetupTime; }

	void SetDesiredEscapeHeading(float fHeading) {m_fDesiredEscapeHeading = fHeading;}

	void SetClipVeriation(bool bUseVeriation01) { m_bUseClipsetVeriation01 = bUseVeriation01; }

	void SetForcedNaturalMotionTask(aiTask* pSubtask);

	int CalculateTimeTilRecovery() const;

	void ShowWeapon(bool show);

	void SetupNetworkForEscape();

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName(s32);
#endif

	void CleanUp() override;

	static Tunables	sm_Tunables;

	static u32 GetTimeSinceRecoveryToIgnoreEnduranceDamage();
	static u32 GetIncapacitateTime();
	static u32 GetRecoverFasterModifier();
	static u32 GetRecoverFasterLimit();

	virtual bool OverridesNetworkBlender(CPed*);

private:
	FSM_Return	Start_OnEnter();
	FSM_Return	Start_OnUpdate(CPed* pPed);

	FSM_Return	DownedRagdoll_OnEnter(CPed* pPed);
	FSM_Return	DownedRagdoll_OnUpdate();
	FSM_Return  DownedRagdoll_OnExit();

	FSM_Return	Incapacitated_OnEnter();
	FSM_Return	Incapacitated_OnUpdate(CPed* pPed);

	FSM_Return	Escape_OnEnter();
	FSM_Return	Escape_OnUpdate(CPed* pPed);

	FSM_Return	Arrest_OnEnter();
	FSM_Return	Arrest_OnUpdate();

	void HandleUserInput(CPed* pPed);

	bool DoesStateUseStreamedAnims(const s32 iState) const;

protected:

	// PURPOSE: Called by the nm getup task on the primary task to determine which get up sets to use when
	//			blending from nm. Allows the task to request specific get up types for different a.i. behaviors
	virtual bool		UseCustomGetup() { return true; }
	virtual bool		AddGetUpSets(CNmBlendOutSetList& sets, CPed* pPed);
	virtual void		GetUpComplete(eNmBlendOutSet setMatched, CNmBlendOutItem* lastItem, CTaskMove* pMoveTask, CPed* pPed);
	virtual bool		ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	void				OnCloneTaskNoLongerRunningOnOwner() override;
	CTaskInfo*			CreateQueriableState() const override;
	void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo) override;
	virtual bool        ControlPassingAllowed(CPed *pPed, const netPlayer& player, eMigrationType migrationType);
	FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent) override;
	fwMvClipSetId		GetCorrectInVehicleAnim() const;
private:

	CMoveNetworkHelper m_moveNetworkHelper;
	fwClipSetRequestHelper m_ClipRequestHelper;

	RegdaiTask m_pForcedNmTask;

	bool m_bCloneQuit;
	bool m_bArrested;
	bool m_bInVehicleAnim;
	bool m_bTimerStarted;
	bool m_bGetupComplete;
	u32 m_iFastGetupOffset;
	u32 m_iGetupTime;
	u32 m_iTimeRagdollLastMoved;
	float m_fDesiredEscapeHeading;
	float m_fDesiredEscapeHeadingLocal;
	bool m_bUseClipsetVeriation01;

	RegdPed m_pArrester;
	fwMvClipSetId m_InVehicleClipset;

	static const fwMvNetworkDefId ms_TaskIncapacitatedNetwork;
	static const fwMvBooleanId ms_EventOnEnterEscape;
	static const fwMvBooleanId ms_EventEscapeFinished;
	static const fwMvRequestId ms_RequestEscape;
	static const fwMvClipSetId ms_IncapacitatedMaleDictVer1;
	static const fwMvClipSetId ms_IncapacitatedMaleDictVer2;
	static const fwMvFloatId ms_GetupHeading;
	static const fwMvFlagId ms_UseLeftSideGetups;
	static const fwMvFlagId ms_UseRightSideGetups;

	fwMvClipSetId ms_IncapacitatedDict;

	// Cloud tunables
	static s32 ms_TimeSinceRecoveryToIgnoreEnduranceDamage;
	static s32 ms_IncapacitateTime;
	static s32 ms_GetUpFasterModifier;
	static s32 ms_GetUpFasterLimit;
};

//////////////////////////////////////////////////////////////////////////
// CClonedTaskIncapacitatedInfo
//////////////////////////////////////////////////////////////////////////

class CClonedTaskIncapacitatedInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedTaskIncapacitatedInfo(s32 iState, bool bShouldArrest, bool bUseVeriation01, float fDesiredEscapeHeading);
	CClonedTaskIncapacitatedInfo();
	~CClonedTaskIncapacitatedInfo();

	virtual s32 GetTaskInfoType() const { return INFO_TYPE_INCAPACITATED; }
	virtual CTaskFSMClone *CreateCloneFSMTask();

	virtual bool HasState() const { return true; }
	virtual u32 GetSizeOfState() const { return datBitsNeeded< CTaskIncapacitated::State_Finish >::COUNT; }

	bool GetShouldArrest() const { return m_bShouldArrest; }
	bool GetClipVeriation() const { return m_bUseVeriation01; }
	float GetEscapeHeading() const { return m_fEscapeHeading; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_BOOL(serialiser, m_bShouldArrest, "Should Arrest");
		SERIALISE_BOOL(serialiser, m_bUseVeriation01, "Should Use Veriation 01");
		SERIALISE_PACKED_FLOAT(serialiser, m_fEscapeHeading, PI, SIZEOF_SIGNAL_FLOAT, "Escape Heading float");
	}

private:
	static const u32 SIZEOF_SIGNAL_FLOAT = 8;

	bool m_bShouldArrest;
	bool m_bUseVeriation01;
	float m_fEscapeHeading;
};

#endif
#endif //CNC_MODE_ENABLED