#ifndef TASK_BOMB_H
#define TASK_BOMB_H

// Game headers
#include "Peds/QueriableInterface.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/Weapons/WeaponController.h"
#include "Task/Weapons/WeaponTarget.h"

// Forward declarations
class CClonedBombInfo;

//////////////////////////////////////////////////////////////////////////
// CTaskBomb
//////////////////////////////////////////////////////////////////////////

class CTaskBomb : public CTaskFSMClone
{
	friend CClonedBombInfo;

public:

	// The task state
	enum State
	{
		State_Init = 0,
		State_SlideAndPlant,
		State_Swap,
		State_Quit,
	};

	CTaskBomb();
	CTaskBomb(const CWeaponController::WeaponControllerType weaponControllerType, float fDuration = -1, bool bHasToBeAttached = false);
	virtual ~CTaskBomb();

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskBomb(m_weaponControllerType, m_fDuration, m_bHasToBeAttached); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_BOMB; }

	// Clone task implementation
	// Returns true below as it always takes its state from the remote machine
	virtual bool                ControlPassingAllowed(CPed *UNUSED_PARAM(pPed), const netPlayer& UNUSED_PARAM(player), eMigrationType UNUSED_PARAM(migrationType)) { return false; }
	virtual bool				HasState() const			{ return true; }
	virtual CTaskInfo*			CreateQueriableState() const;
	virtual FSM_Return			UpdateClonedFSM	(const s32 iState, const FSM_Event iEvent);

	// Used in FPS camera LookAt code
	Vector3 GetBombTargetPosition() const { return m_vTargetPosition; }

	// Distance at which we perform the vertical surface probe
	static fwMvClipId	ms_PlantInFrontClipId;
	static fwMvClipId	ms_PlantOnGroundClipId;
	static dev_float 	ms_fVerticalProbeDistance;
	static dev_float 	ms_fMaxPlantGroundNormalZ;
	static dev_float	ms_fUphillMaxPlantGroundNormalZ;

protected:

	virtual FSM_Return ProcessPreFSM();

	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

#if !__FINAL
	virtual void Debug() const;

	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif // !__FINAL

private:

	//
	// State functions
	//

	FSM_Return StateInit_OnUpdate(CPed* pPed);
	FSM_Return StateInit_OnUpdateClone(CPed* pPed);
	FSM_Return StateSlideAndPlant_OnEnter(CPed* pPed);
	FSM_Return StateSlideAndPlant_OnUpdate(CPed* pPed);
	FSM_Return StateSlideAndPlant_OnExit(CPed* pPed);
	FSM_Return StateSwap_OnEnter(CPed* pPed);
	FSM_Return StateSwap_OnUpdate(CPed* pPed);

	// Helpers
	bool CheckForPlayerMovement(CPed& ped);
	void GetTargetPosition(Vector3& vMoveTarget) const;
	float GetTargetHeading() const;

	//
	// Members
	//

	// Weapon controller
	const CWeaponController::WeaponControllerType m_weaponControllerType;

	// Time (in milliseconds) before ending the task (-1 for infinite)
	float m_fDuration;

	// Target entity
	RegdEnt m_pTargetEntity;

	// Target point
	Vector3 m_vTargetPosition;

	// Target normal
	Vector3 m_vTargetNormal;

	// Position of bomb on prior update
	Vec3V m_vPriorBombPosition;

	// Flags
	bool m_bHasToBeAttached : 1;
	bool m_bClonePlantInFront : 1;
	bool m_bHasPriorPosition : 1;

	// Clips
	fwMvClipId			m_ClipId;
};

//////////////////////////////////////////////////////////////////////////
// CClonedBombInfo
//////////////////////////////////////////////////////////////////////////
class CClonedBombInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedBombInfo() 
		: m_bPlantInFront(false)
		, m_targetNormal(Vector3(0.0f, 0.0f, 0.0f))
	{
	
	}
	CClonedBombInfo(s32 state, const Vector3& targetPosition, const Vector3& targetNormal, bool bPlantInFront);
	~CClonedBombInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_BOMB;}

	virtual CTaskFSMClone *CreateCloneFSMTask();

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_POSITION(serialiser, m_targetPosition, "Target position");
		SERIALISE_BOOL(serialiser, m_bPlantInFront, "Plant In front");

		if(m_bPlantInFront || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_VECTOR(serialiser, m_targetNormal, 1.1f, SIZEOF_NORMAL, "Target normal");
		}
	}

	virtual bool    HasState() const { return true; }
	virtual u32	GetSizeOfState() const { return datBitsNeeded<CTaskBomb::State_Quit>::COUNT;}
	static const char*	GetStaticStateName(s32)  { return "FSM State";}

	const Vector3& GetTargetPosition() const { return m_targetPosition; }
	const Vector3& GetTargetNormal() const { return m_targetNormal; }
	const bool		GetPlantInFront() const { return m_bPlantInFront; }

private:	
	static const unsigned int SIZEOF_NORMAL		= 8;

	CClonedBombInfo(const CClonedBombInfo &);
	CClonedBombInfo &operator=(const CClonedBombInfo &);

	Vector3 m_targetNormal;
	Vector3 m_targetPosition;
	bool	m_bPlantInFront;
};

#endif // TASK_BOMB_H
