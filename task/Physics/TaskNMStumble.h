// Filename   :	TaskNMStumble.h
// Description:	GTA-side of the NM Stumble behavior

#if 0 // This code is currently out of use

#ifndef INC_TASKNMSTUMBLE_H_
#define INC_TASKNMSTUMBLE_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"

#define NUM_GRAB_POINTS 4

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMElectrocute
//
class CClonedNMStumbleInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMStumbleInfo() {}
	~CClonedNMStumbleInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_STUMBLE;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);
	}

private:

	CClonedNMStumbleInfo(const CClonedNMStumbleInfo &);
	CClonedNMStumbleInfo &operator=(const CClonedNMStumbleInfo &);
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CTaskNMStumble //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CTaskNMStumble : public CTaskNMBehaviour
{
public:
	CTaskNMStumble(u32 nMinTime, u32 nMaxTime);
	~CTaskNMStumble();
	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return m_strTaskName;}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMStumble(m_nMinTime, m_nMaxTime);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_STUMBLE;}

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

	void HandleStumble(CPed* pPed);
	void HandleGrab(CPed* pPed);
	void HandleStayUpright(CPed* pPed);
	void HandleConfigureBalance(CPed* pPed);
	void HandleLeanRandom(CPed* pPed);
	void HandleSetCharacterHealth(CPed* pPed);

	bool ProbeEnvironment(CPed* pPed);  // returns true if new data was obtained
	bool IsValidHit(const phInst *pHitInst, const CPed *pPed);

	enum eStates // for internal state machine.
	{
		STANDING,
		STAGGERING,
		TEETERING,
		FALLING,
		ON_GROUND
	};
	eStates m_eState;

	// Tunable parameters: /////////////////
#if __BANK
public:
#else // __BANK
private:
#endif // __BANK

	////////////////////////////////////////

	enum Points
	{
		LeftHigh = 0,
		LeftMid,
		RightHigh,
		RightMid,
	};

	atRangeArray<Vec3V, NUM_GRAB_POINTS> m_GrabPoints;
	atRangeArray<Vec3V, NUM_GRAB_POINTS> m_GrabNormals;
	atRangeArray<bool, NUM_GRAB_POINTS> m_Hit;
	atRangeArray<bool, NUM_GRAB_POINTS> m_Fixed;
	atRangeArray<int, NUM_GRAB_POINTS> m_HitLevelIndex;
	atRangeArray<u16, NUM_GRAB_POINTS> m_HitGenID;
	s16 m_FramesSinceLastProbe;


#if !__FINAL
	// For debugging the internal state machine.
	atString m_strTaskName;
#endif // !__FINAL
};


#endif // INC_TASKNMSTUMBLE_H_

#endif