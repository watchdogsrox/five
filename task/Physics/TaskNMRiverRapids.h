// Filename   :	TaskNMRiverRapids.h
// Description:	GTA-side of the NM underwater behavior for river rapids

#ifndef INC_TASKNMRIVERRAPIDS_H_
#define INC_TASKNMRIVERRAPIDS_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMRiverRapids
//
class CClonedNMRiverRapidsInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMRiverRapidsInfo() {}
	~CClonedNMRiverRapidsInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_RIVERRAPIDS;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
	}

private:

	CClonedNMRiverRapidsInfo(const CClonedNMRiverRapidsInfo &);
	CClonedNMRiverRapidsInfo &operator=(const CClonedNMRiverRapidsInfo &);
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMRiverRapids //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CTaskNMRiverRapids : public CTaskNMBehaviour
{
public:
	struct Tunables : CTuning
	{
		Tunables();

		float m_fMinRiverFlowForRapids;
		float m_fMinRiverGroundClearanceForRapids;

		bool m_bHorizontalRighting;
		float m_fHorizontalRightingStrength;
		float m_fHorizontalRightingTime;

		bool m_bVerticalRighting;
		float m_fVerticalRightingStrength;
		float m_fVerticalRightingTime;

		float m_fRagdollComponentBuoyancy[RAGDOLL_NUM_COMPONENTS];

		struct BodyWrithe
		{
			bool m_bControlledByPlayerSprintInput;
			float m_fMinArmAmplitude;
			float m_fMaxArmAmplitude;
			float m_fMinArmStiffness;
			float m_fMaxArmStiffness;
			float m_fMinArmPeriod;
			float m_fMaxArmPeriod;
			float m_fMinStroke;
			float m_fMaxStroke;
			float m_fMinBuoyancy;
			float m_fMaxBuoyancy;

			PAR_SIMPLE_PARSABLE;
		};

		BodyWrithe m_BodyWrithe;

		CNmTuningSet m_Start;
		CNmTuningSet m_Update;

#if __BANK
		void PreAddWidgets(bkBank& bank);
#endif

		PAR_PARSABLE;
	};

	static Tunables	sm_Tunables;

	CTaskNMRiverRapids(u32 nMinTime, u32 nMaxTime);
	~CTaskNMRiverRapids();
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return m_strTaskName;}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMRiverRapids(m_nMinTime, m_nMaxTime);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_RIVER_RAPIDS;}

	virtual void CleanUp();

	virtual bool ProcessPhysics(float fTimeStep, int iTimeSlice);

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

	void HandleBuoyancy(CPed* pPed);

	// We need to make a per-instance copy of the buoyancy info since we change the buoyancy constants and don't want this
	// to affect all peds of this type.
	CBuoyancyInfo* m_pBuoyancyOverride;

#if !__FINAL
	// For debugging the internal state machine.
	atString m_strTaskName;
#endif // !__FINAL

	float m_fTimeSubmerged;
};


#endif // INC_TASKNMRIVERRAPIDS_H_