// FILE :    TaskDiveToGround.h
// CREATED : 07/02/2012

#ifndef TASK_DIVE_TO_GROUND_H
#define TASK_DIVE_TO_GROUND_H

// Game headers
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"

class CTaskDiveToGround : public CTaskFSMClone
{

public:

	enum DiveToGroundState
	{
		State_Start = 0,
		State_Dive,
		State_GetUp,
		State_Finish
	};

	explicit CTaskDiveToGround(Vec3V_ConstRef vDiveDirection, const fwMvClipSetId& clipSet);
	virtual ~CTaskDiveToGround();

public:

	void SetBlendInDelta(float fValue) { m_fBlendInDelta = fValue; }

public:

	virtual aiTask*	Copy() const;
	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_DIVE_TO_GROUND; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);

	virtual FSM_Return	ProcessPreFSM();
	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

#if !__FINAL
	virtual void	Debug() const;
	static const char*		GetStaticStateName(s32 iState);
#endif // !__FINAL

	// Clone task implementation
	virtual bool                ControlPassingAllowed(CPed*, const netPlayer&, eMigrationType)	{ return false; }
	virtual bool                OverridesNetworkBlender(CPed*) { return true; }
	virtual	bool				OverridesNetworkHeadingBlender(CPed*) { return true; }
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);

private:

	// States
	FSM_Return					Start_OnUpdate();

	void						Dive_OnEnter();
	FSM_Return					Dive_OnUpdate();
	void						Dive_OnExit();

	void						GetUp_OnEnter();
	FSM_Return					GetUp_OnUpdate();
	void						GetUp_OnExit();

	void						PickDiveReactionClip(fwMvClipId& clipHash);

private:

	Vec3V			m_vDiveDirection;
	fwMvClipSetId	m_clipSet;
	float			m_fBlendInDelta;
#if __DEV
	Matrix34 m_lastMatrix;
#endif //__DEV

};



//-------------------------------------------------------------------------
// Task info for dive to ground
//-------------------------------------------------------------------------
class CClonedDiveToGroundInfo : public CSerialisedFSMTaskInfo
{

public:

	CClonedDiveToGroundInfo(Vector3 diveDirection, const fwMvClipSetId& clipSetId) : 
		m_diveDirection(diveDirection), 
		m_clipsetId(clipSetId)
	{
	}

	CClonedDiveToGroundInfo() :
		m_diveDirection(VEC3_ZERO)
	{
	}

	~CClonedDiveToGroundInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_DIVE_TO_GROUND;}

	virtual bool			HasState() const				{ return false; }

	virtual CTaskFSMClone*	CreateCloneFSMTask()
	{
		return rage_new CTaskDiveToGround(VECTOR3_TO_VEC3V(m_diveDirection), m_clipsetId);
	}

	void Serialise(CSyncDataBase& serialiser)
	{
		const unsigned SIZEOF_CLIP_SET_ID = 32;

		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_POSITION(serialiser, m_diveDirection, "Dive direction");

		u32 clipSetIdHash = m_clipsetId.GetHash();
		SERIALISE_UNSIGNED(serialiser, clipSetIdHash, SIZEOF_CLIP_SET_ID);
		m_clipsetId.SetHash(clipSetIdHash);
	}

private:

	CClonedDiveToGroundInfo(const CClonedDiveToGroundInfo &);
	CClonedDiveToGroundInfo &operator=(const CClonedDiveToGroundInfo &);

private:

	Vector3			m_diveDirection;
	fwMvClipSetId	m_clipsetId;
};

#endif //TASK_DIVE_TO_GROUND_H