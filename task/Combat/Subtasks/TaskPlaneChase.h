//
// PlaneChase
// Designed for following some sort of entity
//


#ifndef TASK_PLANE_CHASE_H
#define TASK_PLANE_CHASE_H

// Game headers
#include "ai/AITarget.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "Task/System/TaskFSMClone.h"
#include "Peds/QueriableInterface.h"

// Forward declarations
class CPlane;

//=========================================================================
// CTaskPlaneChase
//=========================================================================

class CTaskPlaneChase : public CTaskFSMClone
{

public:

	enum State
	{
		State_Start = 0,
		State_Pursue,
		State_Finish,
	};

	struct Tunables : public CTuning
	{
		Tunables();

		PAR_PARSABLE;
	};
	
	CTaskPlaneChase(const CAITarget& rTarget);
	virtual ~CTaskPlaneChase();
	

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual void				UpdateClonedSubTasks(CPed* pPed, int const iState);
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

	virtual aiTask*				Copy() const { return rage_new CTaskPlaneChase(m_Target); }
	virtual s32					GetTaskTypeInternal() const { return CTaskTypes::TASK_PLANE_CHASE; }
	virtual s32					GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return			ProcessPreFSM();
	virtual FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);

	void						TaskSetState(u32 const iState);

	FSM_Return	Start_OnUpdate();
	void		Pursue_OnEnter();
	FSM_Return	Pursue_OnUpdate();

	CPlane*						GetPlane() const;
	const CEntity*				GetTargetEntity() const;
	bool						IsBeingPursued();

	CAITarget			m_Target;
	static Tunables	sm_Tunables;

};

class CClonedPlaneChaseInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedPlaneChaseInfo(u32 const state, CEntity const* target);

	CClonedPlaneChaseInfo();

	~CClonedPlaneChaseInfo();

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_PLANE_CHASE;}
	virtual CTaskFSMClone *CreateCloneFSMTask();

    virtual bool			HasState() const					{ return true; }
	virtual u32				GetSizeOfState() const				{ return datBitsNeeded<CTaskPlaneChase::State_Finish>::COUNT; }
    virtual const char*		GetStatusName(u32) const			{ return "Status";}	

	inline const CEntity*	GetTarget()	const					{ return m_target.GetEntity();	}
	inline CEntity*			GetTarget()							{ return m_target.GetEntity();	}

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		ObjectId targetID = m_target.GetEntityID();
		SERIALISE_OBJECTID(serialiser, targetID, "Target Entity");
		m_target.SetEntityID(targetID);
	}

private:

	CSyncedEntity	m_target;
};

#endif // TASK_PLANE_CHASE_H
