// Filename   :	TaskNMExplosion.h
// Description:	Natural Motion explosion class (FSM version)

#ifndef INC_TASKNMEXPLOSION_H_
#define INC_TASKNMEXPLOSION_H_

// --- Include Files ------------------------------------------------------------

// Game headers
#include "Task/System/Task.h"
#include "Task\System\TaskTypes.h"
#include "Task/System/Tuning.h"
#include "Task/Physics/TaskNM.h"
#include "Task/Physics/TaskNMBrace.h"

// ------------------------------------------------------------------------------

//
// Task info for CTaskNMExplosion
//
class CClonedNMExposionInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedNMExposionInfo(const Vector3& vecExplosionPos);
	CClonedNMExposionInfo();
	~CClonedNMExposionInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_NM_EXPLOSION;}

	// the clone task is created by the CTaskNMControl clone task
	virtual bool AutoCreateCloneTask(CPed* UNUSED_PARAM(pPed)) { return false; }

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_POSITION(serialiser, m_explosionPos, "Explosion pos");
	}

private:

	CClonedNMExposionInfo(const CClonedNMExposionInfo &);
	CClonedNMExposionInfo &operator=(const CClonedNMExposionInfo &);

	Vector3	m_explosionPos;
};

//
// Task CTaskNMExplosion
//
class CTaskNMExplosion : public CTaskNMBehaviour
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		// Defines the min and max time to spend in the stunned state at the end of the behaviour if the ped survives.
		u32		m_MinStunnedTime;
		u32		m_MaxStunnedTime;

		// should players do the stunned reaction or not
		bool	m_AllowPlayerStunned;

		bool	m_UseRelaxBehaviour;
		float	m_RollUpHeightThreshold;
		float	m_CatchFallHeightThresholdRollUp;
		float	m_CatchFallHeightThresholdWindmill;
		float	m_CatchFallHeightThresholdClipPose;
		u32		m_TimeToStartCatchFall;
		u32		m_TimeToStartCatchFallPlayer;
		bool	m_DoCatchFallRelax;					// Whether or not to relax the muscle stiffness after impact.
		float	m_CatchFallRelaxHeight;				// Height to relax muscle stiffness for catch-fall.
		float   m_HeightToStartWrithe;
		
		// Define the min and max time that we should spend in the initial behaviour. The actual time before testing
		// for state switch conditions is determined by a random number generator (in milliseconds).
		u32 m_MinTimeForInitialState;
		u32 m_MaxTimeForInitialState;

		// Define the min and max time that the ped should writhe for when on fire and on the ground. As above, the
		// actual time is determined by a random number generator (in milliseconds).
		u32 m_MinWritheTime;
		u32 m_MaxWritheTime;

		bool m_ForceRollUp;
		bool m_ForceWindmill;

		CNmTuningSet m_StartWindmill;
		CNmTuningSet m_StartCatchFall;
		CNmTuningSet m_StartRollDownStairs;
		CNmTuningSet m_Update;

		struct Explosion
		{
			float m_NMBodyScale;
			float m_HumanBodyScale;
			float m_HumanPelvisScale;
			float m_HumanSpine0Scale;
			float m_HumanSpine1Scale;
			float m_AnimalBodyScale;
			float m_AnimalPelvisScale;

			float m_StrongBlastMagnitude;
			float m_FastMovingPedSpeed;

			float m_MaxDistanceAbovePedPositionToClampPitch;
			float m_PitchClampMin;
			float m_PitchClampMax;

			float m_MagnitudeClamp;

			float m_SideScale;
			float m_PitchSideAngle;
			float m_PitchTorqueMin;
			float m_PitchTorqueMax;

			float m_BlanketForceScale;

			float m_ExtraTorqueTwistMax;

			float m_DisableInjuredBehaviorImpulseLimit;
			float m_DisableInjuredBehaviorDistLimit;

			PAR_SIMPLE_PARSABLE;
		};

		Explosion m_Explosion;

		PAR_PARSABLE;
	};

	CTaskNMExplosion(u32 nMinTime, u32 nMaxTime, const Vector3& vecExplosionPos);
	~CTaskNMExplosion();

	////////////////////////
	// CTaskSimple functions
#if !__FINAL
	virtual atString GetName() const {return m_strTaskName;}
#endif

	virtual aiTask* Copy() const;
	virtual int GetTaskTypeInternal() const {return CTaskTypes::TASK_NM_EXPLOSION;}

	////////////////////////
	// CTaskFSMClone functions
	virtual CTaskInfo* CreateQueriableState() const
	{
		return rage_new CClonedNMExposionInfo(m_vExplosionOrigin);
	}

	///////////////////////////
	// CTaskNMBehaviour functions
protected:
	virtual void StartBehaviour(CPed* pPed);
	virtual void ControlBehaviour(CPed* pPed);
	virtual bool FinishConditions(CPed* pPed);

	virtual void StartAnimatedFallback(CPed* pPed);
	virtual bool ControlAnimatedFallback(CPed* pPed);

	//////////////////////////
	// Local functions and data
public:

	Vector3 GetExplosionOrigin() { return m_vExplosionOrigin; }
	void StartWindmillAndPedal(CPed *pPed);

private:

	enum eStates
	{
		// moving states
		WINDMILL_AND_PEDAL,

		// falling down states
		ROLL_DOWN_STAIRS,
		CATCH_FALL,

		// final states
		WAIT,
	};

	Vector3			m_vExplosionOrigin;
	eStates			m_eState;

	int				m_nMaxPedalTimeInRollUp;
	bool			m_bUseCatchFall;

public:

	bool m_bRollUp, m_bWrithe, m_bStunned;

	// A random time to stay in the initial state before checking for exit conditions.
	u32 m_nInitialStateTime;
	// Random time to keep writhing for when on fire and on the ground.
	u32 m_nWritheTime;
	u32 m_nStartWritheTime;
	u32 m_nStartCatchfallTime;
	// Has the ped reached the apex of its trajectory?
	bool m_bPostApex;

#if !__FINAL
	// For debugging the internal state machine.
	atString m_strTaskName;
#endif // !__FINAL

	static Tunables	sm_Tunables;
};

#endif // ! INC_TASKNMEXPLOSION_H_
