// Filename   :	TaskNMSimple.h
// Description:	NM Simple behavior

#ifndef INC_TASKNMSimple_H_
#define INC_TASKNMSimple_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

class CClonedNMSimpleInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMSimpleInfo(atHashString uTuningId) :
	m_uTuningId(uTuningId)
	{
	}
	CClonedNMSimpleInfo() :
	m_uTuningId(0)
	{
	}
	~CClonedNMSimpleInfo() {}

	virtual s32 GetTaskInfoType() const {return INFO_TYPE_NM_SIMPLE;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_UNSIGNED(serialiser, m_uTuningId, SIZEOF_TUNING_ID, "Tuning Id");
	}

private:

	static const unsigned SIZEOF_TUNING_ID = 32;

	CClonedNMSimpleInfo(const CClonedNMSimpleInfo &);
	CClonedNMSimpleInfo &operator=(const CClonedNMSimpleInfo &);

	u32 m_uTuningId;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMSimple /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CTaskNMSimple : public CTaskNMBehaviour
{
public:

	struct Tunables : CTuning
	{
	public:
		struct Tuning
		{
			int m_iMinTime;
			int m_iMaxTime;
			float m_fRagdollScore;
			CNmTuningSet m_Start;
			CNmTuningSet m_Update;
			CNmTuningSet m_OnBalanceFailure;
			CTaskNMBehaviour::Tunables::BlendOutThreshold m_BlendOutThreshold;

			PAR_SIMPLE_PARSABLE;
		};

		typedef atBinaryMap<Tuning, atHashString> Map;

		Tunables();
		bool Append(Tuning* newItem, atHashString key);
		void Revert(atHashString key);
		const Tuning* GetTuning(atHashString tuning) { return m_Tuning.SafeGet(tuning); }
		atHashString GetTuningId(const Tuning& tuning);

#if __BANK
		void PreAddWidgets(bkBank& bank);
#endif // __BANK

	private:
#if __BANK
		void RefreshTuningWidgets(bool bSetCurrentGroup = false);
		void AddTuning();
		void RemoveTuning();
		void RemoveAllTuning();

		bkGroup* m_pGroup;
		bkBank* m_pBank;
		atHashString m_TuningName;
#endif // __BANK

		Map m_Tuning;

		PAR_PARSABLE;
	};

	static Tunables	sm_Tunables;

	CTaskNMSimple(const Tunables::Tuning& tuning);
	CTaskNMSimple(const Tunables::Tuning& tuning, u32 overrideMinTime, u32 overrideMaxTime);
	~CTaskNMSimple();
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const;
#endif // !__FINAL
	virtual aiTask* Copy() const { return rage_new CTaskNMSimple(m_Tuning); }
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_NM_SIMPLE; }

	static bool AppendSimpleTask(Tunables::Tuning* newItem, atHashString key)
	{
		return sm_Tunables.Append(newItem,key);
	}
	static void RevertSimpleTask(atHashString key)
	{
		sm_Tunables.Revert(key);
	}
	///////////////////////////
	// CTaskNMBehaviour functions

protected:
	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	virtual void CreateAnimatedFallbackTask(CPed* pPed);
	virtual bool ControlAnimatedFallback(CPed* pPed);

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const;

private:
	//////////////////////////
	// Local functions and data
	class CWritheClipSetRequestHelper* m_pWritheClipSetRequestHelper;

	const Tunables::Tuning& m_Tuning;
	bool m_bBalanceFailedSent;
};

#endif // INC_TASKNMSimple_H_
