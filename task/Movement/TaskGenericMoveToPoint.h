#ifndef TASK_GENERIC_MOVE_TO_POINT_H
#define TASK_GENERIC_MOVE_TO_POINT_H

//***************************************
//File:  TaskGenericMoveToPoint.h
//***************************************

#include "TaskGoTo.h"

#include "AI/AITarget.h"
#include "Peds/PedDefines.h"
#include "Task/System/Task.h"
#include "Task/General/TaskBasic.h"


//***************************************
//Class:  CTaskGenericMoveToPoint
//Purpose:  High level movement task that changes the style of movement based on ped preferences.
//***************************************

class CTaskGenericMoveToPoint : public CTaskMove {

public:

	static const float ms_fTargetRadius;

	CTaskGenericMoveToPoint(const float fMoveBlend, const Vector3& vTarget, const float fTargetRadius=ms_fTargetRadius);

	virtual ~CTaskGenericMoveToPoint();

	virtual aiTask* Copy() const
	{
		CTaskGenericMoveToPoint * pClone = rage_new CTaskGenericMoveToPoint(
			m_fMoveBlendRatio,
			m_vTarget,
			m_fTargetRadius
			);
		return pClone;
	}

//draw debug function
#if !__FINAL
	virtual void Debug() const;
#endif //!__FINAL

	virtual bool HasTarget() const { return true; }

	virtual Vector3 GetTarget(void) const {return m_vTarget;}
	virtual Vector3 GetLookAheadTarget() const { return GetTarget(); }

	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_GENERIC_MOVE_TO_POINT;}

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	
	virtual s32 GetDefaultStateAfterAbort() const;

	virtual float GetTargetRadius() const;

	enum
	{
		State_Moving = 0,
	};

//name debug info
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

protected:

	virtual const Vector3 & GetCurrentTarget() { return m_vTarget; }

	FSM_Return StateMoving_OnEnter();

	FSM_Return StateMoving_OnUpdate();



private:

	Vector3 m_vTarget;				// The target position
	
	float m_fTargetRadius;			// The radius that the ped must get to the target for the task to complete

};//CTaskGenericMoveToPoint



#endif //TASK_GENERIC_MOVE_TO_POINT_H

