#ifndef TASK_DEBUG_H
#define TASK_DEBUG_H
// This file is only ever compiled in __DEV builds
#if !__FINAL

// Game headers
#include "Task/Weapons/Gun/TaskAimGun.h"
//////////////////////////////////////////////////////////////////////////
// CTaskTestClipDistance
//////////////////////////////////////////////////////////////////////////

class CTaskTestClipDistance : public CTask
{
public:

	CTaskTestClipDistance(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId) : m_clipSetId(clipSetId), m_clipId(clipId) {}

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskTestClipDistance(m_clipSetId, m_clipId); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_TEST_CLIP_DISTANCE; }

protected:

	typedef enum
	{
		State_Start = 0,
		State_Quit,
	} State;

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32    ) {  return "TestClipDistance";  };
#endif // !__FINAL

private:

	fwMvClipSetId m_clipSetId;
	fwMvClipId m_clipId;
	Vector3 m_vPedPosAtStart;
};


#endif // !__FINAL
#endif // TASK_DEBUG_H
