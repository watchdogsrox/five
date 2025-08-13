// Filename   :	TaskNMFallDown.h
// Description:	Natural Motion fall down class (FSM version)

#ifndef INC_TASKNMFALLDOWN_H_
#define INC_TASKNMFALLDOWN_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/GrabHelper.h"

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMFallDown
//
class CClonedNMFallDownInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMFallDownInfo(u32 nFallType, const Vector3& fallDirection, float fGroundHeight, bool bForceFatal);
	CClonedNMFallDownInfo();
	~CClonedNMFallDownInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_FALLDOWN;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_UNSIGNED(serialiser, m_nFallType, SIZEOF_FALL_TYPE, "Fall type");

		SERIALISE_VECTOR(serialiser, m_fallDirection, 1.0f, SIZEOF_DIRN, "Fall direction");

		m_fallDirection.Normalize(); 

		SERIALISE_PACKED_FLOAT(serialiser, m_fGroundHeight, 2000.0f, SIZEOF_GROUND_HEIGHT, "Ground height");

		SERIALISE_BOOL(serialiser, m_bForceFatal, "Force Fatal");
	}

private:

	CClonedNMFallDownInfo(const CClonedNMFallDownInfo &);
	CClonedNMFallDownInfo &operator=(const CClonedNMFallDownInfo &);

	static const unsigned int SIZEOF_FALL_TYPE		= 3;
	static const unsigned int SIZEOF_DIRN			= 16;
	static const unsigned int SIZEOF_GROUND_HEIGHT	= 18;

	u32	m_nFallType;
	Vector3 m_fallDirection;
	float	m_fGroundHeight;
	bool	m_bForceFatal;
};

//
// Task CTaskNMFallDown
//
class CTaskNMFallDown : public CTaskNMBehaviour
{	
public:
	enum eNMFallType
	{
		TYPE_FROM_HIGH = 0,
		TYPE_OVER_WALL,
		TYPE_DOWN_STAIRS,
		TYPE_DIE_TYPES,	// not to be used, marker for start of die types
		TYPE_DIE_FROM_HIGH,
		TYPE_DIE_OVER_WALL,
		TYPE_DIE_DOWN_STAIRS
	};
	enum eNMFallState
	{
		STATE_READY = 0,
		STATE_BALANCE,
		STATE_FALLING,
		STATE_GRABBING,
		STATE_FALLING2,
		STATE_RELAX
	};

	// Ctor: Optional intersection can be passed in with information already computed in the logic which spawned this task.
	CTaskNMFallDown(u32 nMinTime, u32 nMaxTime, eNMFallType nFallType, const Vector3& vecDirn, float fGroundHeight,
		CEntity* pEntityResponsible = NULL, const CGrabHelper* pGrabHelper=NULL, const Vector3 &vecWallPosition=VEC3_ZERO, bool bForceFatal = false);
	~CTaskNMFallDown();

	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return atString("NMFallDown");}
#endif
	virtual aiTask* Copy() const {return rage_new CTaskNMFallDown(m_nMinTime, m_nMaxTime, m_nFallType, m_vecDirn, m_fGroundHeight, m_pEntityResponsible, NULL, m_vecWallPos, m_bForceFatal);}
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_FALL_DOWN;}

	virtual void BehaviourFailure(CPed* pPed, ARTFeedbackInterfaceGta* pFeedbackInterface);

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const
	{
		return rage_new CClonedNMFallDownInfo(m_nFallType, m_vecDirn, m_fGroundHeight, m_bForceFatal);
	}

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	//////////////////////////
	// Local functions and data
private:
	eNMFallType m_nFallType;
	eNMFallState m_nFallState;

	Vector3		m_vecDirn;
	float		m_fGroundHeight;
	float		m_fStartHeight;
	u32		m_nFailBalanceTime;
	RegdEnt		m_pEntityResponsible;
	bool		m_bForceFatal;	//kill the user ped even if the fall messes up

	//CGrabHelper m_GrabHelper;

	Vector3 m_vecWallPos; // Optional vector to share information about line tests.
};

#endif // ! INC_TASKNMFALLDOWN_H_
