// Filename   :	TaskAnimatedFallback.h
// Description:	Plays a given animation, altering the ped's orientation to match its current pose
//

#ifndef INC_TASKANIMATEDFALLBACK_H_
#define INC_TASKANIMATEDFALLBACK_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Game headers
#include "Task/System/Task.h"
//#include "Task/System/TaskTypes.h"

// ------------------------------------------------------------------------------

class CTaskAnimatedFallback : public CTask
{
public:

	typedef enum
	{
		State_Start = 0,
		State_PlayingClip,
		State_Finish,
	} State;

	CTaskAnimatedFallback(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fStartPhase = 0.0f, float fRootSlopeFixupPhase = -1.0f, bool bHoldAtEnd = false);
	CTaskAnimatedFallback(const CTaskAnimatedFallback& other);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskAnimatedFallback(m_clipSetHelper.GetClipSetId(), m_clipId, m_fRootSlopeFixupPhase); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual int GetTaskTypeInternal() const { return CTaskTypes::TASK_ANIMATED_FALLBACK; }

	fwMvClipSetId GetClipSetId() { return m_clipSetHelper.GetClipSetId(); }
	fwMvClipId GetClipId() { return m_clipId; }
	float GetClipPhase() const;

protected:

	//virtual FSM_Return ProcessPreFSM();

	virtual bool ProcessPostPreRender();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual	void CleanUp();

	// Determine whether or not the task should abort
	//virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Finish; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName(s32 iState)
	{
		Assert(iState >= State_Start && iState <= State_Finish);
		static const char* aStateNames[] = 
		{
			"State_Start",
			"State_PlayingClip",
			"State_Finish"
		};

		return aStateNames[iState];
	}
#endif // !__FINAL

private:

	//
	// State functions
	//

	FSM_Return StateStart_OnUpdate(CPed* pPed);

	void StatePlayingClip_OnEnter(CPed* pPed);
	FSM_Return StatePlayingClip_OnUpdate(CPed* pPed);

	//
	// Members
	//

	CMoveNetworkHelper m_networkHelper;
	fwClipSetRequestHelper m_clipSetHelper;

	// The fall over clip to perform
	fwMvClipId m_clipId;

	float m_fStartPhase;
	float m_fRootSlopeFixupPhase;

	bool m_bHoldAtEnd;
};

#endif // ! INC_TASKBLENDFROMNM_H_