// FILE :    TaskHeliChase.h
// PURPOSE : Subtask of heli combat used to chase a vehicle

#ifndef TASK_HELI_CHASE_H
#define TASK_HELI_CHASE_H

// Game headers
#include "ai/AITarget.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/Task.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"

// Forward declarations
class CHeli;

//=========================================================================
// CTaskHeliChase
//=========================================================================

class CTaskHeliChase : public CTaskFSMClone
{

public:

	enum State
	{
		State_Start = 0,
		State_Pursue,
		State_Finish,
	};

	enum OrientationMode
	{
		OrientationMode_None,
		OrientationMode_OrientOnArrival,
		OrientationMode_OrientNearArrival,
		OrientationMode_OrientAllTheTime,
	};

	enum OrientationRelative
	{
		OrientationRelative_World,
		OrientationRelative_HeliToTarget,
		OrientationRelative_TargetForward,
		OrientationRelative_TargetVelocity,
	};

	enum OffsetRelative
	{
		OffsetRelative_Local,
		OffsetRelative_World,
	};

	enum DriftMode
	{
		DriftMode_Enabled,
		DriftMode_Disabled,
	};

	
public:

	struct Tunables : public CTuning
	{
		struct Drift
		{
			Drift()
			{}

			float m_MinValueForCorrection;
			float m_MaxValueForCorrection;
			float m_MinRate;
			float m_MaxRate;

			PAR_SIMPLE_PARSABLE;
		};
		
		Tunables();
		
		Drift	m_DriftX;
		Drift	m_DriftY;
		Drift	m_DriftZ;
		int		m_MinHeightAboveTerrain;
		float	m_SlowDownDistanceMin;
		float	m_SlowDownDistanceMax;
		float	m_CruiseSpeed;
		float	m_MaxDistanceForOrientation;
		float	m_NearDistanceForOrientation;

		PAR_PARSABLE;
	};
	
public:

	CTaskHeliChase(const CAITarget& rTarget, Vec3V_In vTargetOffset);
	virtual ~CTaskHeliChase();
	
public:

	Vec3V_Out	GetTargetOffset() const { return m_vTargetOffset; }
	void		SetTargetOffset(Vec3V_In vTargetOffset) { m_vTargetOffset = vTargetOffset; }
	void		SetTargetOffsetRelative( OffsetRelative in_OffsetRelative) { m_OffsetRelative = in_OffsetRelative; }

	void		SetOrientationRelative(OrientationRelative in_OrientationRelative) { m_OrientationRelative = in_OrientationRelative; }
	void		SetOrientationMode(OrientationMode in_OrientationMode) { m_OrientationMode = in_OrientationMode; }
	void		SetOrientationOffset(float in_OrientationOffset) { m_fOrientationOffset = in_OrientationOffset; }

	virtual void				OnCloneTaskNoLongerRunningOnOwner();
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual FSM_Return			UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);
	virtual CTaskFSMClone*		CreateTaskForClonePed(CPed* pPed);
	virtual CTaskFSMClone*		CreateTaskForLocalPed(CPed* pPed);

public:

#if !__FINAL
	void Debug() const;
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32);
#endif // !__FINAL

private:

	const CPhysical*	GetDominantTarget() const;
	CHeli*				GetHeli() const;

private:

	virtual aiTask* Copy() const { return rage_new CTaskHeliChase(m_Target, m_vTargetOffset); }
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_HELI_CHASE; }
	virtual s32	GetDefaultStateAfterAbort()	const { return State_Finish; }

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

private:

	FSM_Return	Start_OnUpdate();
	void		Pursue_OnEnter();
	void		Pursue_OnEnterClone();
	FSM_Return	Pursue_OnUpdate();
	FSM_Return	Pursue_OnUpdateClone();
	
private:

	bool		CalculateTargetOrientation(float& fOrientation) const;
	Vec3V_Out	CalculateTargetPosition() const;
	float		CalculateSpeed() const;
	Vec3V_Out	GetDrift() const;
	void		ProcessDrift();
	void		UpdateVehicleMission();

private:

	Vec3V				m_vTargetOffset;
	Vec3V				m_vTargetVelocitySmoothed;
	OffsetRelative		m_OffsetRelative;
	float				m_fOrientationOffset;
	OrientationRelative	m_OrientationRelative;
	OrientationMode		m_OrientationMode;
	DriftMode			m_DriftMode;


	CVehicleDriftHelper	m_DriftHelperX;
	CVehicleDriftHelper	m_DriftHelperY;
	CVehicleDriftHelper	m_DriftHelperZ;
	CAITarget			m_Target;
	bool				m_CloneQuit;

private:

	static Tunables	sm_Tunables;

};


class CClonedHeliChaseInfo : public CSerialisedFSMTaskInfo
{
public:

	CClonedHeliChaseInfo(const CAITarget& rTarget, Vec3V_In vTargetOffset) : m_Target(rTarget), m_TargetOffset(VEC3V_TO_VECTOR3(vTargetOffset)) {}
	CClonedHeliChaseInfo() : m_TargetOffset(VEC3_ZERO) {}

	virtual CTask  *CreateLocalTask(fwEntity *UNUSED_PARAM(pEntity))
	{
		return rage_new CTaskHeliChase(m_Target, VECTOR3_TO_VEC3V(m_TargetOffset));
	}
	virtual CTaskFSMClone *CreateCloneFSMTask()
	{
		return rage_new CTaskHeliChase(m_Target, VECTOR3_TO_VEC3V(m_TargetOffset));
	}

	virtual s32		GetTaskInfoType() const { return CTaskInfo::INFO_TYPE_HELI_CHASE; }
	virtual int GetScriptedTaskType() const { return SCRIPT_TASK_HELI_CHASE; }

	void Serialise(CSyncDataBase& serialiser)
	{
		m_Target.Serialise(serialiser);

		bool bOffsetNonZero = !m_TargetOffset.IsZero();

		SERIALISE_BOOL(serialiser, bOffsetNonZero);

		if (bOffsetNonZero || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_POSITION(serialiser, m_TargetOffset, "Target Offset");
		}
		else
		{
			m_TargetOffset.Zero();
		}
	}

private:

	CAITarget    m_Target; // move blend ratio to use when navigating
	Vector3		m_TargetOffset;      // target position to navigate to
};

#endif // TASK_HELI_CHASE_H
