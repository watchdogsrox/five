// Filename   :	TaskNMInjuredOnGround.h
// Description:	GTA-side of the NM InjuredOnGround behavior

#ifndef INC_TASKNMInjuredOnGround_H_
#define INC_TASKNMInjuredOnGround_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

class CClonedNMInjuredOnGroundInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMInjuredOnGroundInfo() {}
	~CClonedNMInjuredOnGroundInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_INJURED_ON_GROUND;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
	}

private:

	CClonedNMInjuredOnGroundInfo(const CClonedNMInjuredOnGroundInfo &);
	CClonedNMInjuredOnGroundInfo &operator=(const CClonedNMInjuredOnGroundInfo &);
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMInjuredOnGround //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CTaskNMInjuredOnGround : public CTaskNMBehaviour
{
public:
	struct Tunables : CTuning
	{
		Tunables();

		float m_fDoInjuredOnGroundChance;

		float m_fFallingSpeedThreshold;

		int m_iRandomDurationMin;
		int m_iRandomDurationMax;

		int m_iMaxNumInjuredOnGroundAgents;

		PAR_PARSABLE;
	};

	static Tunables	sm_Tunables;

	CTaskNMInjuredOnGround(u32 nMinTime, u32 nMaxTime);
	~CTaskNMInjuredOnGround();
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return m_strTaskName;}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMInjuredOnGround(m_nMinTime, m_nMaxTime);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_INJURED_ON_GROUND;}

	static u16 GetNumInjuredOnGroundAgents() { return ms_NumInjuredOnGroundAgents; }

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	//////////////////////////
	// Local functions and data
private:

	void HandleInjuredOnGround(CPed* pPed);

	static u16 ms_NumInjuredOnGroundAgents;

	// Tunable parameters: /////////////////
#if __BANK
public:
#else // __BANK
private:
#endif // __BANK

	////////////////////////////////////////



#if !__FINAL
	// For debugging the internal state machine.
	atString m_strTaskName;
#endif // !__FINAL
};


#endif // INC_TASKNMInjuredOnGround_H_