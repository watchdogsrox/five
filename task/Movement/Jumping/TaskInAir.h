#ifndef TASK_IN_AIR_H
#define TASK_IN_AIR_H

// Game headers
#include "Task/System/TaskComplex.h"

//////////////////////////////////////////////////////////////////////////
// CTaskComplexStuckInAir
//////////////////////////////////////////////////////////////////////////

class CTaskComplexStuckInAir : public CTaskComplex
{
public:

	CTaskComplexStuckInAir();
	~CTaskComplexStuckInAir();

	virtual aiTask* Copy() const { return rage_new CTaskComplexStuckInAir(); }

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_COMPLEX_STUCK_IN_AIR; }

#if !__FINAL
	virtual atString GetName() const { return atString("CplxStuckInAir"); }
#endif    

	s32 GetDefaultStateAfterAbort() const { return CTaskComplex::State_ControlSubTask; }

	virtual aiTask* CreateNextSubTask(CPed* pPed);
	virtual aiTask* CreateFirstSubTask(CPed* pPed);
	virtual aiTask* ControlSubTask(CPed* pPed);

private:    

	aiTask* CreateSubTask(const int iSubTaskType, CPed* pPed);
};

//////////////////////////////////////////////////////////////////////////
// CTaskDropOverEdge
//////////////////////////////////////////////////////////////////////////
/*
class CTaskDropOverEdge : public CTask
{
public:

	static bank_float ms_fMinDropHeight;
	static bank_float ms_fMaxDropHeight;

	static bank_float ms_fDropDeltaLow;
	static bank_float ms_fDropDeltaMedium;
	static bank_float ms_fDropDeltaHigh;

	static bank_bool ms_bDisableCollisionForDropStart;
	static bank_bool ms_bDisableGravityForDropStart;

	CTaskDropOverEdge(const Vector3 & vStartPos, const float fHeading, const int iDropType=DropType_Unknown);
	virtual ~CTaskDropOverEdge();

	enum
	{
		State_Initial,
		State_SlideToCoord,
		State_WaitForClipsToStream,
		State_DropStart,
		State_DropLoop,
		State_DropLand
	};

	enum
	{
		DropType_Unknown	= -1,
		DropType_None		= 0,
		DropType_Low		= 1,
		DropType_Medium		= 2,
		DropType_High		= 3
	};

	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_DROP_OVER_EDGE; }
	virtual aiTask* Copy() const { return rage_new CTaskDropOverEdge(m_vStartPos, m_fHeading, m_iDropType); }

	virtual bool ShouldAbort( const AbortPriority iPriority, const aiEvent* pEvent);
	virtual bool ProcessPhysics(float fTimestep, int nTimeSlice);
	virtual void CleanUp();

#if !__FINAL
    virtual void Debug() const;
	friend class CTaskClassInfoManager;
	static const char *GetStaticStateName(s32 iState)
	{
		switch(iState)
		{
			case State_Initial: return "Initial";
			case State_SlideToCoord: return "SlideToCoord";
			case State_WaitForClipsToStream: return "WaitForAnimsToStream";
			case State_DropStart: return "DropStart";
			case State_DropLoop: return "DropLoop";
			case State_DropLand: return "DropLand";
		}
		return NULL;
	}
#endif

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Initial; }

protected:

	FSM_Return StateInitial_OnEnter(CPed* pPed);
	FSM_Return StateInitial_OnUpdate(CPed* pPed);
	FSM_Return StateSlideToCoord_OnEnter(CPed* pPed);
	FSM_Return StateSlideToCoord_OnUpdate(CPed* pPed);
	FSM_Return StateWaitForClipsToStream_OnEnter(CPed* pPed);
	FSM_Return StateWaitForClipsToStream_OnUpdate(CPed* pPed);
	FSM_Return StateDropStart_OnEnter(CPed* pPed);
	FSM_Return StateDropStart_OnUpdate(CPed* pPed);
	FSM_Return StateDropLoop_OnEnter(CPed* pPed);
	FSM_Return StateDropLoop_OnUpdate(CPed* pPed);
	FSM_Return StateDropLand_OnEnter(CPed* pPed);
	FSM_Return StateDropLand_OnUpdate(CPed* pPed);

	bool CalculateDropType(CPed * pPed);

	Vector3 m_vStartPos;
	Vector3 m_vDropPos;
	float m_fDropEndZ;
	float m_fHeading;
	int m_iDropType;

	float m_fDropClipVelScale;
	Vector3 m_vDropClipEndingVel;

	static const fwMvClipId ms_iDropIntroIDs[3];
	static const fwMvClipId ms_iDropLoopIDs[3];
	static const fwMvClipId ms_iDropLandIDs[3];
};
*/
#endif // TASK_IN_AIR_H
