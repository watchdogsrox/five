// FILE :    TaskReactToExplosion.h
// CREATED : 11-4-2011

#ifndef TASK_REACT_TO_EXPLOSION_H
#define TASK_REACT_TO_EXPLOSION_H

// Rage headers
#include "fwutil/Flags.h"

// Game headers
#include "ai/AITarget.h"
#include "task/Combat/Subtasks/TaskShellShocked.h"
#include "Task/System/Task.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

////////////////////////////////////////////////////////////////////////////////
// CTaskReactToExplosion
////////////////////////////////////////////////////////////////////////////////

class CTaskReactToExplosion : public CTaskFSMClone
{

public:

	enum ConfigFlags
	{
		CF_DisableThreatResponse = BIT0,
	};

public:

	enum ReactToExplosionState
	{
		State_Start = 0,
		State_Resume,
		State_ShellShocked,
		State_Flinch,
		State_ThreatResponse,
		State_Flee,
		State_Finish
	};

public:

	struct Tunables : CTuning
	{
		Tunables();

		float	m_MaxShellShockedDistance;
		float	m_MaxFlinchDistance;
		float	m_MaxLookAtDistance;

		PAR_PARSABLE;
	};

public:

	explicit CTaskReactToExplosion(Vec3V_In vPosition, CEntity* pOwner, float fRadius, fwFlags8 uConfigFlags = 0);
	virtual ~CTaskReactToExplosion();

public:

	fwFlags8& GetConfigFlagsForShellShocked() { return m_uConfigFlagsForShellShocked; }

public:

	void SetAimTarget(const CAITarget& rTarget) { m_AimTarget = rTarget; }
	void SetConfigFlagsForShellShocked(fwFlags8 uFlags) { m_uConfigFlagsForShellShocked = uFlags; }
	void SetDurationOverrideForShellShocked(CTaskShellShocked::Duration nOverride) { m_nDurationOverrideForShellShocked = nOverride; }

public:

	static const Tunables& GetTunables() { return sm_Tunables; }

public:

	static float GetMaxFlinchDistance(float fRadius)
	{
		return (fRadius + sm_Tunables.m_MaxFlinchDistance);
	}

	static float GetMaxLookAtDistance(float fRadius)
	{
		return (fRadius + sm_Tunables.m_MaxLookAtDistance);
	}

	static float GetMaxReactionDistance(float fRadius)
	{
		return GetMaxFlinchDistance(fRadius);
	}

	static float GetMaxShellShockedDistance(float fRadius)
	{
		return (fRadius + sm_Tunables.m_MaxShellShockedDistance);
	}

public:

	static bool IsTemporary(const CPed& rPed, CEntity* pOwner, fwFlags8 uConfigFlags = 0);
	static bool IsValid(const CPed& rPed, Vec3V_In vPosition, CEntity* pOwner, float fRadius, fwFlags8 uConfigFlags = 0, const CAITarget& rAimTarget = CAITarget());

public:

	virtual aiTask*	Copy() const;
	virtual s32	GetTaskTypeInternal() const { return CTaskTypes::TASK_REACT_TO_EXPLOSION; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Resume; }

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return	UpdateFSM(const s32 iState, const FSM_Event iEvent);

#if !__FINAL
	virtual void	Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

public:

	// Clone task implementation
	virtual bool                ControlPassingAllowed(CPed*, const netPlayer&, eMigrationType)	{ return false; }
	virtual bool                OverridesNetworkBlender(CPed*) { return GetState() <= State_Flinch; }
	virtual	bool				OverridesNetworkHeadingBlender(CPed*) { return GetState() <= State_Flinch; }
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual void				ReadQueriableState(CClonedFSMTaskInfo* pTaskInfo);
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);
	virtual void                OnCloneTaskNoLongerRunningOnOwner();

private:

	FSM_Return	Start_OnUpdate();
	FSM_Return	Resume_OnUpdate();
	void		ShellShocked_OnEnter();
	FSM_Return	ShellShocked_OnUpdate();
	void		Flinch_OnEnter();
	FSM_Return	Flinch_OnUpdate();
	void		ThreatResponse_OnEnter();
	FSM_Return	ThreatResponse_OnUpdate();
	void		Flee_OnEnter();
	FSM_Return	Flee_OnUpdate();

private:

	s32		ChooseStateForThreatResponse() const;
	int		ChooseStateToResumeTo(bool& bKeepSubTask) const;
	CPed*	GetOwnerPed() const;
	void	GetTargetToFleeFrom(CAITarget& rTarget) const;
	void	SetStateForThreatResponse();
	
private:
	
	static bool		CanFlee(const CPed& rPed);
	static bool		CanUseThreatResponse(const CPed& rPed, CEntity* pOwner);
	static s32		ChooseState(const CPed& rPed, Vec3V_In vPosition, CEntity* pOwner, float fRadius, fwFlags8 uConfigFlags, const CAITarget& rAimTarget);
	static s32		ChooseStateForThreatResponse(const CPed& rPed, CEntity* pOwner, fwFlags8 uConfigFlags);
	static CPed*	GetOwnerPed(CEntity* pOwner);

private:

	Vec3V						m_vPosition;
	RegdEnt						m_pOwner;
	CAITarget					m_AimTarget;
	float						m_fRadius;
	CTaskShellShocked::Duration	m_nDurationOverrideForShellShocked;
	fwFlags8					m_uConfigFlags;
	fwFlags8					m_uConfigFlagsForShellShocked;

private:

	static Tunables	sm_Tunables;

};



//-------------------------------------------------------------------------
// Task info for react to explosion
//-------------------------------------------------------------------------
class CClonedReactToExplosionInfo : public CSerialisedFSMTaskInfo
{

public:

	CClonedReactToExplosionInfo(s32 iState, Vector3 vPosition, CEntity* pOwner, u8 uConfigFlags) : 
		m_explosionPosition(vPosition), 
		m_pExplosionOwner(pOwner),
		m_configFlagsForShellShocked(uConfigFlags) 
	{
		SetStatusFromMainTaskState(iState);
	}

	CClonedReactToExplosionInfo() :
		m_explosionPosition(VEC3_ZERO), 
		m_configFlagsForShellShocked(0) 
	{
	}

	~CClonedReactToExplosionInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_REACT_TO_EXPLOSION;}

	virtual bool			HasState() const				{ return true; }
	virtual u32				GetSizeOfState() const			{ return datBitsNeeded<CTaskReactToExplosion::State_Finish>::COUNT; }
	virtual const char*		GetStatusName(u32) const		{ return "Status";}

	const Vector3&	GetExplosionPosition() const			{ return m_explosionPosition; }

	CEntity*		GetExplosionOwner()						{ return m_pExplosionOwner.GetEntity(); }
	const CEntity*	GetExplosionOwner() const				{ return m_pExplosionOwner.GetEntity(); }

	u8				GetConfigFlagsForShellShocked() const	{ return m_configFlagsForShellShocked; }

	virtual CTaskFSMClone*	CreateCloneFSMTask()
	{
		return rage_new CTaskReactToExplosion(VECTOR3_TO_VEC3V(m_explosionPosition), m_pExplosionOwner.GetEntity(), m_configFlagsForShellShocked);
	}

	void Serialise(CSyncDataBase& serialiser)
	{
		static const unsigned SIZEOF_CONFIG_FLAGS = 2;

		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_POSITION(serialiser, m_explosionPosition, "Explosion position");


		ObjectId targetID = m_pExplosionOwner.GetEntityID();
		SERIALISE_OBJECTID(serialiser, targetID, "Explosion Owner");
		m_pExplosionOwner.SetEntityID(targetID);

		SERIALISE_UNSIGNED(serialiser, m_configFlagsForShellShocked, SIZEOF_CONFIG_FLAGS, "Shell shocked config flags");
	}

private:

	CClonedReactToExplosionInfo(const CClonedReactToExplosionInfo &);
	CClonedReactToExplosionInfo &operator=(const CClonedReactToExplosionInfo &);

private:

	Vector3			m_explosionPosition;
	CSyncedEntity   m_pExplosionOwner;
	u8				m_configFlagsForShellShocked;
};

#endif //TASK_REACT_TO_EXPLOSION_H
