// Filename   :	TaskNMRelax.h
// Description:	Natural Motion relax class (FSM version)

#ifndef INC_TASKNMRELAX_H_
#define INC_TASKNMRELAX_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMRelax
//
class CClonedNMRelaxInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMRelaxInfo(float fStiffness, float fDamping, bool bHoldPose);
	CClonedNMRelaxInfo();
	~CClonedNMRelaxInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_RELAX;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_BOOL(serialiser, m_bHasStiffness, "Has stiffness");

		if (m_bHasStiffness || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fStiffness, 30.0f, SIZEOF_STIFFNESS, "Stiffness");
		}

		SERIALISE_BOOL(serialiser, m_bHasDamping, "Has damping");

		if (m_bHasDamping || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fDamping, 30.0f, SIZEOF_DAMPING, "Damping");
		}
	}

private:

	CClonedNMRelaxInfo(const CClonedNMRelaxInfo &);
	CClonedNMRelaxInfo &operator=(const CClonedNMRelaxInfo &);

	static const unsigned int SIZEOF_STIFFNESS			= 16;
	static const unsigned int SIZEOF_DAMPING			= 16;

	float		m_fStiffness;
	float		m_fDamping;
	bool		m_bHasStiffness;
	bool		m_bHasDamping;
	bool		m_bHoldPose;
};

//
// Task CTaskNMRelax
//
class CTaskNMRelax : public CTaskNMBehaviour
{
public:
	CTaskNMRelax(u32 nMinTime, u32 nMaxTime, float fStiffness = -1.0f, float fDamping = -1.0f, bool bHoldPose = false);
	~CTaskNMRelax();
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMRelax");}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMRelax(m_nMinTime, m_nMaxTime, m_fStiffness, m_fDamping, m_bHoldPose);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_RELAX;}

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const
	{
		return rage_new CClonedNMRelaxInfo(m_fStiffness, m_fDamping, m_bHoldPose);
	}

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	virtual void StartAnimatedFallback(CPed* pPed);
	virtual bool ControlAnimatedFallback(CPed* pPed);

	void SendRelaxMessage(CPed* pPed, bool bStart);

	//////////////////////////
	// Local functions and data
protected:

	// For whole body
	float m_fStiffness, m_fDamping;
	bool m_bHoldPose;
};

#endif // ! INC_TASKNMRELAX_H_
