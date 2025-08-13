// Filename   :	TaskNMPrototype.h
// Description:	

#ifndef INC_TASKNMPROTOTYPE_H_
#define INC_TASKNMPROTOTYPE_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

class CClonedNMPrototypeInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMPrototypeInfo() {}
	~CClonedNMPrototypeInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_PROTOTYPE;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
	}

private:

	CClonedNMPrototypeInfo(const CClonedNMPrototypeInfo &);
	CClonedNMPrototypeInfo &operator=(const CClonedNMPrototypeInfo &);
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMPrototype //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CTaskNMPrototype : public CTaskNMBehaviour
{
public:
	CTaskNMPrototype(u32 nMinTime, u32 nMaxTime);
	~CTaskNMPrototype();

	struct Tunables : CTuning
	{
		Tunables();

		struct TimedTuning
		{
			virtual ~TimedTuning() {}
			float m_TimeInSeconds;
			CNmTuningSet m_Messages;
			bool m_Periodic;

			PAR_PARSABLE;
		};

#if __BANK
		void AddPrototypeWidgets(bkBank& bank);
#endif //__BANK

		bool m_RunForever;
		bool m_CheckForMovingGround;
		s32 m_SimulationTimeInMs;

		CNmTuningSet m_Start;
		CNmTuningSet m_Update;
		CNmTuningSet m_OnBalanceFailed;

		atArray<TimedTuning>	m_TimedMessages;

		CNmTuningSet	m_DynamicSet1;
		CNmTuningSet	m_DynamicSet2;
		CNmTuningSet	m_DynamicSet3;

		PAR_PARSABLE;
	};


	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return m_strTaskName;}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMPrototype(m_nMinTime, m_nMaxTime);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_PROTOTYPE;}

#if __BANK
	static void StartPrototypeTask();
	static void StopPrototypeTask();
	static void SendDynamicSet1() { SendDynamicSet(sm_Tunables.m_DynamicSet1);}
	static void SendDynamicSet2() { SendDynamicSet(sm_Tunables.m_DynamicSet2);}
	static void SendDynamicSet3() { SendDynamicSet(sm_Tunables.m_DynamicSet3);}
	static void SendDynamicSet(CNmTuningSet&);

	static bool GetRunForever() { return sm_Tunables.m_RunForever; }
	static s32 GetDesiredSimulationTime() { return sm_Tunables.m_SimulationTimeInMs; }
#endif //__BANK
	
	// Ends the prototype task immediately
	void Stop() { m_bStopTask=true; }

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

	//////////////////////////
	// Local functions and data
private:


	// Tunable parameters: /////////////////
#if __BANK
public:
#else // __BANK
private:
#endif // __BANK

	////////////////////////////////////////

	bool m_WasInfinite;
	bool m_bBalanceFailedSent;
	bool m_bStopTask;
	float m_fLastUpdateTime;

#if !__FINAL
	// For debugging the internal state machine.
	atString m_strTaskName;
#endif // !__FINAL

	static Tunables	sm_Tunables;
};


#endif // INC_TASKNMPROTOTYPE_H_